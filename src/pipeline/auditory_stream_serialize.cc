// Serialization functions for AuditoryStream.
//
// Extracted from auditory_stream.cc to keep the controller focused on
// lifecycle. All three functions are AuditoryStream member functions declared
// in include/pipeline/auditory_stream.h.

#include "pipeline/auditory_stream.h"

#include "core/log.h"
#include "core/types.h"
#include "model/speaker_database.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"
#include "pipeline/runtime_config.h"

#include <cuda_runtime.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace orator {
namespace pipeline {
namespace {

std::optional<double> ReadDoubleFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  double v = 0.0;
  if (!(in >> v)) return std::nullopt;
  return v;
}

std::optional<std::string> ReadFirstLine(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::string s;
  if (!std::getline(in, s)) return std::nullopt;
  return s;
}

struct GpuUtilization {
  double pct = 0.0;
  std::string source;
};

std::string ReadCommandOutput(const std::vector<std::string>& args) {
  if (args.empty()) return "";
  int pipefd[2];
  if (pipe(pipefd) != 0) return "";

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return "";
  }
  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    close(STDERR_FILENO);
    const long max_fd = sysconf(_SC_OPEN_MAX);
    const int limit = max_fd > 0 ? static_cast<int>(max_fd) : 1024;
    for (int fd = 3; fd < limit; ++fd) {
      if (fd != STDOUT_FILENO) close(fd);
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(pipefd[1]);
  std::string out;
  char buf[512];
  ssize_t n = 0;
  while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
    out.append(buf, static_cast<size_t>(n));
  }
  close(pipefd[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  return out;
}

std::optional<double> ParseTegrastatsGpuUtilizationPct(
    const std::string& line) {
  const std::string keys[] = {"GR3D_FREQ", "GPU_FREQ"};
  for (const auto& key : keys) {
    const size_t key_pos = line.find(key);
    if (key_pos == std::string::npos) continue;
    const size_t pct_pos = line.find('%', key_pos + key.size());
    if (pct_pos == std::string::npos) continue;

    size_t value_start = pct_pos;
    while (value_start > key_pos) {
      const char c = line[value_start - 1];
      if ((c < '0' || c > '9') && c != '.') break;
      --value_start;
    }
    if (value_start == pct_pos) continue;
    char* end = nullptr;
    const double value = std::strtod(line.c_str() + value_start, &end);
    if (end == line.c_str() + value_start || value < 0.0) continue;
    return value > 100.0 ? 100.0 : value;
  }
  return std::nullopt;
}

std::optional<GpuUtilization> ReadTegrastatsGpuUtilizationPct() {
  const std::string output = ReadCommandOutput(
      {"timeout", "0.5s", "tegrastats", "--readall", "--interval", "100"});
  size_t start = 0;
  while (start < output.size()) {
    const size_t end = output.find('\n', start);
    const std::string line = output.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    const auto pct = ParseTegrastatsGpuUtilizationPct(line);
    if (pct) return GpuUtilization{*pct, "tegrastats"};
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return std::nullopt;
}

std::optional<GpuUtilization> ReadNvidiaSmiGpuUtilizationPct() {
  const std::string output =
      ReadCommandOutput({"nvidia-smi", "--query-gpu=utilization.gpu",
                         "--format=csv,noheader,nounits"});
  std::optional<double> best;
  size_t start = 0;
  while (start < output.size()) {
    const size_t line_end = output.find('\n', start);
    const std::string line =
        output.substr(start, line_end == std::string::npos ? std::string::npos
                                                           : line_end - start);
    char* end = nullptr;
    const double value = std::strtod(line.c_str(), &end);
    if (end != line.c_str() && value >= 0.0) {
      if (!best || value > *best) best = value > 100.0 ? 100.0 : value;
    }
    if (line_end == std::string::npos) break;
    start = line_end + 1;
  }
  if (!best) return std::nullopt;
  return GpuUtilization{*best, "nvidia-smi"};
}

std::optional<GpuUtilization> ReadGpuUtilization() {
  const std::filesystem::path known_paths[] = {
      "/sys/devices/gpu.0/load",
      "/sys/devices/platform/gpu.0/load",
      "/sys/devices/platform/17000000.ga10b/load",
      "/sys/kernel/debug/gpu.0/load",
  };
  for (const auto& path : known_paths) {
    const auto load = ReadDoubleFile(path);
    if (load) {
      const double pct = *load > 100.0 ? *load / 10.0 : *load;
      return GpuUtilization{pct, "sysfs"};
    }
  }

  const std::filesystem::path root("/sys/class/devfreq");
  std::error_code ec;
  if (std::filesystem::exists(root, ec)) {
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
      if (ec) break;
      const auto name = entry.path().filename().string();
      if (name.find("gpu") == std::string::npos &&
          name.find("nvd") == std::string::npos &&
          name.find("gpc") == std::string::npos) {
        continue;
      }
      const auto load = ReadDoubleFile(entry.path() / "load");
      if (!load) continue;
      // Jetson devfreq load is commonly reported as 0..1000.
      const double pct = *load > 100.0 ? *load / 10.0 : *load;
      return GpuUtilization{pct, "sysfs"};
    }
  }

  // Thor's tegrastats omits GR3D/GPU utilization while nvidia-smi reports it;
  // Orin commonly exposes utilization through sysfs above. Query nvidia-smi
  // before starting a timed tegrastats subprocess, then retain tegrastats as
  // the final compatibility fallback.
  const auto nvidia_smi = ReadNvidiaSmiGpuUtilizationPct();
  if (nvidia_smi) return nvidia_smi;
  return ReadTegrastatsGpuUtilizationPct();
}

std::optional<double> ReadGpuFreqMhz() {
  const std::filesystem::path root("/sys/class/devfreq");
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) return std::nullopt;
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (ec) break;
    const auto name = entry.path().filename().string();
    if (name.find("gpu") == std::string::npos &&
        name.find("nvd") == std::string::npos &&
        name.find("gpc") == std::string::npos) {
      continue;
    }
    const auto hz = ReadDoubleFile(entry.path() / "cur_freq");
    if (hz && *hz > 0.0) return *hz / 1000000.0;
  }
  return std::nullopt;
}

struct PowerRail {
  std::string name;
  double watts = 0.0;
};

std::vector<PowerRail> ReadPowerRails() {
  std::vector<PowerRail> rails;
  const std::filesystem::path root("/sys/class/hwmon");
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) return rails;
  for (const auto& hwmon : std::filesystem::directory_iterator(root, ec)) {
    if (ec) break;
    const auto hwmon_name = ReadFirstLine(hwmon.path() / "name").value_or("");
    std::error_code file_ec;
    bool has_power_input = false;
    for (const auto& f :
         std::filesystem::directory_iterator(hwmon.path(), file_ec)) {
      if (file_ec) break;
      const auto fn = f.path().filename().string();
      if (fn.rfind("power", 0) != 0 || fn.find("_input") == std::string::npos) {
        continue;
      }
      const auto microwatts = ReadDoubleFile(f.path());
      if (!microwatts || *microwatts <= 0.0) continue;
      PowerRail rail;
      const std::string prefix =
          fn.substr(0, fn.size() - std::string("_input").size());
      rail.name = ReadFirstLine(hwmon.path() / (prefix + "_label"))
                      .value_or(ReadFirstLine(hwmon.path() / "label")
                                    .value_or(hwmon_name.empty()
                                                  ? hwmon.path()
                                                        .filename()
                                                        .string()
                                                  : hwmon_name));
      rail.watts = *microwatts / 1000000.0;
      rails.push_back(rail);
      has_power_input = true;
    }
    if (!file_ec && !has_power_input) {
      for (int i = 1; i <= 8; ++i) {
        const auto mv = ReadDoubleFile(hwmon.path() /
                                       ("in" + std::to_string(i) + "_input"));
        const auto ma = ReadDoubleFile(hwmon.path() /
                                       ("curr" + std::to_string(i) + "_input"));
        if (!mv || !ma || *mv <= 0.0 || *ma <= 0.0) continue;
        PowerRail rail;
        rail.name =
            ReadFirstLine(hwmon.path() / ("in" + std::to_string(i) + "_label"))
                .value_or(hwmon_name.empty() ? hwmon.path().filename().string()
                                             : hwmon_name);
        rail.watts = (*mv * *ma) / 1000000.0;
        rails.push_back(rail);
      }
    }
  }
  return rails;
}

std::optional<double> SystemPowerWatts(const std::vector<PowerRail>& rails) {
  for (const auto& rail : rails) {
    if (rail.name == "VIN") return rail.watts;
  }
  for (const auto& rail : rails) {
    if (rail.name.find("ina238") != std::string::npos) return rail.watts;
  }
  for (const auto& rail : rails) {
    if (rail.name.find("VIN") != std::string::npos) return rail.watts;
  }
  if (rails.empty()) return std::nullopt;
  double total = 0.0;
  for (const auto& rail : rails) total += rail.watts;
  return total;
}

std::string OptionalJson(double value, bool valid, int precision = 3) {
  if (!valid) return "null";
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", precision, value);
  return buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// Serialize GPU telemetry snapshot (Spec 002 FR7).
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeGpuTelemetry() const {
  const auto entries = scheduler_.Snapshot();
  const double audio = audio_sec();
  char buf[256];
  std::string out = "{\"type\":\"gpu_telemetry\",";
  std::snprintf(buf, sizeof(buf), "\"time_sec\":%.3f,\"sample_rate\":%d,",
                audio, config_.sample_rate);
  out += buf;
  out += "\"pipelines\":[";
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    double compute = 0.0;
    if (e.name == "diarization")
      compute = diar_worker_ ? diar_worker_->compute_sec() : 0.0;
    else if (e.name == "asr")
      compute = asr_worker_ ? asr_worker_->compute_sec() : 0.0;
    else if (e.name == "vad")
      compute = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    const double rtf = compute > 0.0 ? audio / compute : 0.0;
    std::snprintf(buf, sizeof(buf),
                  "{\"name\":\"%s\",\"priority_index\":%d,\"class\":\"%s\","
                  "\"stream_active\":%s,\"cuda_priority\":%d,"
                  "\"compute_sec\":%.3f,\"real_time_factor\":%.3f}",
                  e.name.c_str(), e.priority_index,
                  e.background ? "background" : "foreground",
                  e.stream_active ? "true" : "false", e.cuda_priority, compute,
                  rtf);
    out += buf;
    if (i + 1 < entries.size()) out += ",";
  }
  out += "]";

  size_t free_bytes = 0;
  size_t total_bytes = 0;
  const cudaError_t mem_status = cudaMemGetInfo(&free_bytes, &total_bytes);
  if (mem_status != cudaSuccess) {
    cudaGetLastError();
  }
  const bool mem_ok = mem_status == cudaSuccess && total_bytes > 0;
  const double total_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
  const double used_mb =
      static_cast<double>(total_bytes - free_bytes) / (1024.0 * 1024.0);
  const double used_pct = mem_ok ? (used_mb / total_mb) * 100.0 : 0.0;
  const auto gpu_util = ReadGpuUtilization();
  const auto gpu_freq = ReadGpuFreqMhz();
  const auto rails = ReadPowerRails();
  const auto system_power_w = SystemPowerWatts(rails);

  out += ",\"device\":{";
  out += "\"gpu_utilization_pct\":" +
         OptionalJson(gpu_util ? gpu_util->pct : 0.0, gpu_util.has_value(), 1);
  out += ",\"gpu_utilization_source\":";
  out += gpu_util ? ("\"" + JsonEscape(gpu_util->source) + "\"") : "null";
  out += ",\"gpu_freq_mhz\":" +
         OptionalJson(gpu_freq.value_or(0.0), gpu_freq.has_value(), 1);
  out += ",\"gpu_mem_used_mb\":" + OptionalJson(used_mb, mem_ok, 1);
  out += ",\"gpu_mem_total_mb\":" + OptionalJson(total_mb, mem_ok, 1);
  out += ",\"gpu_mem_used_pct\":" + OptionalJson(used_pct, mem_ok, 1);
  out += ",\"system_power_w\":" + OptionalJson(system_power_w.value_or(0.0),
                                               system_power_w.has_value(), 2);
  out += ",\"power_rails\":[";
  for (size_t i = 0; i < rails.size(); ++i) {
    if (i > 0) out += ",";
    out += "{\"name\":\"" + JsonEscape(rails[i].name) + "\",\"watts\":";
    out += OptionalJson(rails[i].watts, true, 2) + "}";
  }
  out += "]}}";
  return out;
}

// ---------------------------------------------------------------------------
// Serialize the comprehensive timeline JSON document.
// ---------------------------------------------------------------------------
std::string AuditoryStream::Serialize() {
  // Read one atomic copy of every typed track, then build the timeline
  // document. The document has a shared time axis and two views:
  //   - "tracks": one independent, time-ordered track per registered pipeline,
  //     including business_speaker.
  //   - "comprehensive": a compatibility alias of that stored business track.
  // Adding a future pipeline adds a track without changing existing raw tracks.
  // Every producer deposits typed evidence before protocol serialization, so
  // Serialize never parses protocol JSON and never mutates a track.
  const auto tracks = comp_.SnapshotTracks();
  const auto& diar_view = tracks.diarization;
  const auto& primary_view = tracks.primary_speaker;
  const auto& voiceprint_view = tracks.speaker_voiceprint;
  const auto& comp_view = tracks.business_speaker;
  const auto& vad_view = tracks.vad;
  const auto& raw_texts = tracks.asr;
  const auto& align_view = tracks.align;
  // Populate last_transcript_ from raw_texts for the transcript() accessor.
  core::Transcript transcript;
  for (const auto& r : raw_texts) {
    core::AsrToken tok;
    tok.text_id = r.id;
    tok.start_sec = r.start;
    tok.end_sec = r.end;
    tok.text = r.text;
    transcript.tokens.push_back(std::move(tok));
  }
  {
    std::lock_guard<std::mutex> lock(comp_mutex_);
    last_transcript_ = transcript;
  }

  const double audio = audio_sec();
  const double diar_c = diar_compute_sec();
  const double asr_c = asr_compute_sec();
  const double wall_start = session_start_wall_sec_.load();
  const bool wclk_ok = wall_clock_ok_.load();
  const bool timebase_reconciled = timebase_reconciled_.load();
  const bool timebase_ok = timebase_ok_.load();
  const auto extents = track_extents();

  char buf[256];
  std::string business_entries_json;
  for (size_t i = 0; i < comp_view.size(); ++i) {
    const auto& entry = comp_view[i];
    int speaker_index = -1;
    if (entry.speaker.rfind("speaker_", 0) == 0) {
      try {
        speaker_index = std::stoi(entry.speaker.substr(8));
      } catch (const std::invalid_argument&) {
        speaker_index = -1;
      } catch (const std::out_of_range&) {
        speaker_index = -1;
      }
    }
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.9f,\"end\":%.9f,\"text_id\":%ld,"
                  "\"speaker\":%d",
                  entry.start, entry.end, entry.text_id, speaker_index);
    business_entries_json += buf;
    if (!entry.speaker_id.empty()) {
      business_entries_json += ",\"speaker_id\":\"" + entry.speaker_id + "\"";
      const std::string name = speaker_db_
                                   ? speaker_db_->DisplayName(entry.speaker_id)
                                   : std::string();
      if (!name.empty()) {
        business_entries_json +=
            ",\"speaker_name\":\"" + JsonEscape(name) + "\"";
      }
    }
    std::snprintf(buf, sizeof(buf),
                  ",\"speaker_support\":\"%s\","
                  "\"speaker_uncertain\":%s,"
                  "\"diar_overlap_sec\":%.3f,"
                  "\"diar_total_overlap_sec\":%.3f,"
                  "\"diar_coverage_ratio\":%.3f,"
                  "\"diar_total_coverage_ratio\":%.3f,"
                  "\"diar_max_gap_sec\":%.3f,"
                  "\"diar_island_count\":%d",
                  entry.speaker_support.c_str(),
                  entry.speaker_uncertain ? "true" : "false",
                  entry.diar_overlap_sec, entry.diar_total_overlap_sec,
                  entry.diar_coverage_ratio, entry.diar_total_coverage_ratio,
                  entry.diar_max_gap_sec, entry.diar_island_count);
    business_entries_json += buf;
    business_entries_json +=
        SerializeSpeakerDecisionToJson(entry.speaker_decision);
    business_entries_json += ",\"text\":\"" + JsonEscape(entry.text) + "\"}";
    if (i + 1 < comp_view.size()) business_entries_json += ",";
  }

  std::string out = "{\"type\":\"timeline\",\"schema_version\":1,";
  std::snprintf(
      buf, sizeof(buf),
      "\"audio_sec\":%.9f,\"sample_rate\":%d,"
      "\"session_start_wall_sec\":%.3f,\"wall_clock_ok\":%s,"
      "\"timebase_reconciled\":%s,\"timebase_ok\":%s,",
      audio, config_.sample_rate, wall_start, wclk_ok ? "true" : "false",
      timebase_reconciled ? "true" : "false", timebase_ok ? "true" : "false");
  out += buf;
  out += "\"resolved_config\":" + SerializeResolvedConfig(config_) + ",";
  out += "\"track_extents\":[";
  for (std::size_t i = 0; i < extents.size(); ++i) {
    const auto& extent = extents[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"pipeline\":\"%s\",\"processed_samples\":%ld,"
                  "\"common_total_samples\":%ld,\"gap_samples\":%ld}",
                  extent.pipeline.c_str(), extent.processed_samples,
                  extent.common_total_samples, extent.gap_samples);
    out += buf;
    if (i + 1 < extents.size()) out += ",";
  }
  out += "],";
  out += "\"tracks\":[";

  // Track: speaker diarization.
  std::snprintf(buf, sizeof(buf),
                "{\"kind\":\"diarization\",\"source\":\"sortformer\","
                "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                diar_c, diar_c > 0 ? audio / diar_c : 0.0);
  out += buf;
  for (size_t i = 0; i < diar_view.size(); ++i) {
    const auto& s = diar_view[i];
    int speaker_index = -1;
    if (s.speaker.rfind("speaker_", 0) == 0) {
      try {
        speaker_index = std::stoi(s.speaker.substr(8));
      } catch (const std::invalid_argument&) {
        speaker_index = -1;
      } catch (const std::out_of_range&) {
        speaker_index = -1;
      }
    }
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.9f,\"end\":%.9f,\"speaker\":%d", s.start, s.end,
                  speaker_index);
    out += buf;
    // Spec 010: surface the resolved global voiceprint identity (and optional
    // display name) alongside the diarizer-local index (backward compatible:
    // "speaker" stays integer).
    if (!s.speaker_id.empty()) {
      out += ",\"speaker_id\":\"" + s.speaker_id + "\"";
      const std::string nm =
          speaker_db_ ? speaker_db_->DisplayName(s.speaker_id) : std::string();
      if (!nm.empty()) out += ",\"speaker_name\":\"" + JsonEscape(nm) + "\"";
    }
    std::snprintf(buf, sizeof(buf), ",\"confidence\":%.9g}", s.conf);
    out += buf;
    if (i + 1 < diar_view.size()) out += ",";
  }
  out += "]}";

  // Track: independent frame-wise top-1 Sortformer projection. The raw frame
  // blocks remain typed in ComprehensiveTimeline; this compact run view is the
  // externally auditable form used by speaker fusion.
  if (!primary_view.empty()) {
    out +=
        ",{\"kind\":\"primary_speaker\",\"source\":\"sortformer_top1\","
        "\"entries\":[";
    for (std::size_t i = 0; i < primary_view.size(); ++i) {
      const auto& segment = primary_view[i];
      int speaker_index = -1;
      if (segment.speaker.rfind("speaker_", 0) == 0) {
        try {
          speaker_index = std::stoi(segment.speaker.substr(8));
        } catch (const std::exception&) {
          speaker_index = -1;
        }
      }
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.9f,\"end\":%.9f,\"speaker\":%d,"
                    "\"confidence\":%.9g",
                    segment.start, segment.end, speaker_index, segment.conf);
      out += buf;
      if (!segment.speaker_id.empty()) {
        out += ",\"speaker_id\":\"" + JsonEscape(segment.speaker_id) + "\"";
      }
      out += "}";
      if (i + 1 < primary_view.size()) out += ",";
    }
    out += "]}";
  }

  // Track: automatic speech recognition (present only when ASR is enabled).
  if (asr_) {
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"asr\",\"source\":\"qwen3_asr\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        asr_c, asr_c > 0 ? audio / asr_c : 0.0);
    out += buf;
    for (size_t i = 0; i < transcript.tokens.size(); ++i) {
      const auto& t = transcript.tokens[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"text_id\":%ld,\"start\":%.9f,\"end\":%.9f,\"text\":\"",
                    t.text_id, t.start_sec, t.end_sec);
      out += std::string(buf) + JsonEscape(t.text) + "\"}";
      if (i + 1 < transcript.tokens.size()) out += ",";
    }
    out += "]}";
  }

  // Track: voice activity (VAD). Present only when the VAD pipeline is enabled.
  if (config_.vad_stream) {
    const double vad_c = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"vad\",\"source\":\"silero_gpu\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        vad_c, vad_c > 0 ? audio / vad_c : 0.0);
    out += buf;
    for (size_t i = 0; i < vad_view.size(); ++i) {
      std::snprintf(buf, sizeof(buf), "{\"start\":%.9f,\"end\":%.9f}",
                    vad_view[i].start, vad_view[i].end);
      out += buf;
      if (i + 1 < vad_view.size()) out += ",";
    }
    out += "]}";
  }

  // Track: forced alignment (present only when the aligner is enabled). Refines
  // each ASR segment into per-unit timestamps on the common time base, grouped
  // by the source text_id so consumers can tie units back to the asr track.
  if (aligner_) {
    const double align_c = align_worker_ ? align_worker_->compute_sec() : 0.0;
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"align\",\"source\":\"qwen3_forced_aligner\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        align_c, align_c > 0 ? audio / align_c : 0.0);
    out += buf;
    for (size_t i = 0; i < align_view.size(); ++i) {
      const auto& g = align_view[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"text_id\":%ld,\"start\":%.9f,\"end\":%.9f,\"units\":[",
                    g.text_id, g.start, g.end);
      out += buf;
      for (size_t j = 0; j < g.units.size(); ++j) {
        const auto& u = g.units[j];
        std::snprintf(buf, sizeof(buf),
                      "{\"start\":%.9f,\"end\":%.9f,\"text\":\"", u.start,
                      u.end);
        out += std::string(buf) + JsonEscape(u.text) + "\"}";
        if (j + 1 < g.units.size()) out += ",";
      }
      out += "]}";
      if (i + 1 < align_view.size()) out += ",";
    }
    out += "]}";
  }

  if (config_.speaker_fusion_enable) {
    out +=
        ",{\"kind\":\"speaker_voiceprint\",\"source\":\"titanet_large\","
        "\"entries\":[";
    for (std::size_t i = 0; i < voiceprint_view.size(); ++i) {
      const auto& evidence = voiceprint_view[i];
      std::snprintf(
          buf, sizeof(buf),
          "{\"evidence_id\":\"%s\",\"evidence_kind\":\"%s\","
          "\"text_id\":%ld,\"source_start\":%d,\"source_end\":%d,"
          "\"start\":%.9f,\"end\":%.9f,"
          "\"embedding_available\":%s,\"session_gallery_complete\":%s,"
          "\"robust_gallery_complete\":%s,",
          JsonEscape(evidence.evidence_id).c_str(),
          JsonEscape(evidence.kind).c_str(), evidence.text_id,
          evidence.source_start, evidence.source_end, evidence.start,
          evidence.end, evidence.embedding_available ? "true" : "false",
          evidence.session_gallery_complete ? "true" : "false",
          evidence.robust_gallery_complete ? "true" : "false");
      out += buf;
      auto append_scores = [&](const char* name, const auto& scores) {
        out += "\"" + std::string(name) + "\":[";
        for (std::size_t score_index = 0; score_index < scores.size();
             ++score_index) {
          const auto& score = scores[score_index];
          std::snprintf(buf, sizeof(buf),
                        "{\"speaker_id\":\"%s\",\"score\":%.9g}",
                        JsonEscape(score.speaker_id).c_str(), score.score);
          out += buf;
          if (score_index + 1 < scores.size()) out += ",";
        }
        out += "]";
      };
      append_scores("session_scores", evidence.session_scores);
      out += ",";
      append_scores("robust_scores", evidence.robust_scores);
      out += "}";
      if (i + 1 < voiceprint_view.size()) out += ",";
    }
    out += "]}";
  }
  out +=
      ",{\"kind\":\"business_speaker\",\"source\":\"business_speaker\","
      "\"entries\":[" +
      business_entries_json + "]}";
  out += "]";  // close "tracks"

  out += ",\"comprehensive\":[" + business_entries_json + "]}";
  return out;
}

// ---------------------------------------------------------------------------
// Spec 006: speaker registry list + rename for the Web UI naming panel.
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeSpeakers() const {
  // List every global identity resolved anywhere in this session (accumulated
  // in the typed evidence store), joined with the
  // display names from the speaker database (its own name mutex). Using the
  // accumulated set — not the <=N current diarizer-slot mappings — keeps the
  // speaker panel consistent with the transcript, which accumulates ids the
  // same way. (This avoids racing the diarization thread on the database's
  // embedding/id vectors.)
  const std::vector<std::string> id_list = comp_.AllSpeakerIds();
  std::string out = "{\"type\":\"speakers\",\"speakers\":[";
  bool first = true;
  for (const auto& id : id_list) {
    if (id.empty()) continue;
    if (!first) out += ",";
    first = false;
    const std::string nm =
        speaker_db_ ? speaker_db_->DisplayName(id) : std::string();
    out += "{\"id\":\"" + id + "\",\"name\":\"" + JsonEscape(nm) + "\"}";
  }
  out += "]}";
  return out;
}

bool AuditoryStream::RenameSpeaker(const std::string& speaker_id,
                                   const std::string& name) {
  if (!speaker_db_ || speaker_id.empty()) return false;
  speaker_db_->SetDisplayName(speaker_id, name);  // name-mutex protected
  // Persist so the name survives a restart. The names sidecar is small; the
  // rename is user-initiated and rare, so the brief overlap with enrollment is
  // acceptable.
  if (!config_.speaker_registry_path.empty())
    speaker_db_->Save(config_.speaker_registry_path);
  return true;
}

}  // namespace pipeline
}  // namespace orator
