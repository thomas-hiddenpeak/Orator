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

#include <cuda_runtime.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
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
  const std::string output = ReadCommandOutput(
      {"nvidia-smi", "--query-gpu=utilization.gpu",
       "--format=csv,noheader,nounits"});
  std::optional<double> best;
  size_t start = 0;
  while (start < output.size()) {
    const size_t line_end = output.find('\n', start);
    const std::string line = output.substr(
        start, line_end == std::string::npos ? std::string::npos
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

  const auto tegrastats = ReadTegrastatsGpuUtilizationPct();
  if (tegrastats) return tegrastats;
  return ReadNvidiaSmiGpuUtilizationPct();
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
    for (const auto& f : std::filesystem::directory_iterator(hwmon.path(),
                                                             file_ec)) {
      if (file_ec) break;
      const auto fn = f.path().filename().string();
      if (fn.rfind("power", 0) != 0 ||
          fn.find("_input") == std::string::npos) {
        continue;
      }
      const auto microwatts = ReadDoubleFile(f.path());
      if (!microwatts || *microwatts <= 0.0) continue;
      PowerRail rail;
      rail.name = hwmon_name.empty() ? hwmon.path().filename().string()
                                     : hwmon_name;
      rail.watts = *microwatts / 1000000.0;
      rails.push_back(rail);
      has_power_input = true;
    }
    if (!file_ec && !has_power_input) {
      for (int i = 1; i <= 8; ++i) {
        const auto mv = ReadDoubleFile(hwmon.path() /
                                       ("in" + std::to_string(i) + "_input"));
        const auto ma = ReadDoubleFile(hwmon.path() /
                                       ("curr" + std::to_string(i) +
                                        "_input"));
        if (!mv || !ma || *mv <= 0.0 || *ma <= 0.0) continue;
        PowerRail rail;
        rail.name = ReadFirstLine(hwmon.path() /
                                  ("in" + std::to_string(i) + "_label"))
                        .value_or(hwmon_name.empty()
                                      ? hwmon.path().filename().string()
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

// Shared revision serializer defined in src/pipeline/json_util.cc.
std::string SerializeRevisionToJson(
    const ComprehensiveTimeline::Revision& r, const char* source,
    const std::map<std::string, std::string>* label_ids = nullptr);

// ---------------------------------------------------------------------------
// Serialize one revision (Spec 004) to a {"type":"revision",...} message.
// Delegates to the shared SerializeRevisionToJson helper in json_util.
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeRevision(
    const ComprehensiveTimeline::Revision& r, const char* source) {
  return SerializeRevisionToJson(r, source);
}

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
  out += ",\"system_power_w\":" +
         OptionalJson(system_power_w.value_or(0.0),
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
  // Read both result sets from the timeline store under its lock, then build
  // the timeline document. The document has a shared time axis and three parts:
  //   - "tracks": one independent track per pipeline (diarization, asr), each a
  //     list of that pipeline's time-ordered entries.
  //   - "comprehensive": an accuracy-first derived view that projects each
  //     finalized ASR text_id span through diarization ownership, preserving
  //     ASR final boundaries while splitting text at diarization boundaries.
  // A consumer reads whichever part it needs. Adding a future pipeline adds a
  // track and, optionally, a contribution to the comprehensive view.
  // StreamTimeline removed — ASR track data now comes from
  // comp_.SnapshotRawTexts(). Spec 004 Step 2: the diarization worker is the
  // sole producer of the speaker view (it delivers live via ReplaceSpeakers +
  // keeps last_segments_ fresh); the ASR worker delivers text live via
  // UpsertText. So Serialize is a pure reader: snapshot everything under
  // comp_mutex_. No derivation, no upserts here.
  std::vector<core::DiarSegment> diar_view;
  std::vector<ComprehensiveTimeline::Entry> comp_view;
  std::vector<ComprehensiveTimeline::VadSeg> vad_view;
  std::vector<ComprehensiveTimeline::RawTextSeg> raw_texts;
  std::vector<ComprehensiveTimeline::AlignGroup> align_view;
  std::map<std::string, std::string> speaker_label_ids;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    diar_view = last_segments_;
    comp_view = comp_.Snapshot();
    vad_view = comp_.SnapshotVad();
    raw_texts = comp_.SnapshotRawTexts();
    align_view = comp_.SnapshotAlign();
    speaker_label_ids = comp_.SpeakerLabelIds();
  }
  // Populate last_transcript_ from raw_texts for the transcript() accessor.
  {
    core::Transcript transcript;
    for (const auto& r : raw_texts) {
      core::AsrToken tok;
      tok.start_sec = r.start;
      tok.end_sec = r.end;
      tok.text = r.text;
      transcript.tokens.push_back(std::move(tok));
    }
    last_transcript_ = std::move(transcript);
  }

  const double audio = audio_sec();
  const double diar_c = diar_compute_sec();
  const double asr_c = asr_compute_sec();
  const double wall_start = session_start_wall_sec_.load();
  const bool wclk_ok = wall_clock_ok_.load();

  char buf[256];
  std::string out = "{\"type\":\"timeline\",\"schema_version\":1,";
  std::snprintf(buf, sizeof(buf),
                "\"audio_sec\":%.3f,\"sample_rate\":%d,"
                "\"session_start_wall_sec\":%.3f,\"wall_clock_ok\":%s,",
                audio, config_.sample_rate, wall_start,
                wclk_ok ? "true" : "false");
  out += buf;
  out += "\"tracks\":[";

  // Track: speaker diarization.
  std::snprintf(buf, sizeof(buf),
                "{\"kind\":\"diarization\",\"source\":\"sortformer\","
                "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                diar_c, diar_c > 0 ? audio / diar_c : 0.0);
  out += buf;
  for (size_t i = 0; i < diar_view.size(); ++i) {
    const auto& s = diar_view[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d",
                  s.start_sec, s.end_sec, s.local_speaker);
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
    std::snprintf(buf, sizeof(buf), ",\"confidence\":%.3f}", s.confidence);
    out += buf;
    if (i + 1 < diar_view.size()) out += ",";
  }
  out += "]}";

  // Track: automatic speech recognition (present only when ASR is enabled).
  if (asr_) {
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"asr\",\"source\":\"qwen3_asr\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        asr_c, asr_c > 0 ? audio / asr_c : 0.0);
    out += buf;
    for (size_t i = 0; i < last_transcript_.tokens.size(); ++i) {
      const auto& t = last_transcript_.tokens[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"text\":\"", t.start_sec,
                    t.end_sec);
      out += std::string(buf) + JsonEscape(t.text) + "\"}";
      if (i + 1 < last_transcript_.tokens.size()) out += ",";
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
      std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f}",
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
                    "{\"text_id\":%ld,\"start\":%.3f,\"end\":%.3f,\"units\":[",
                    g.text_id, g.start, g.end);
      out += buf;
      for (size_t j = 0; j < g.units.size(); ++j) {
        const auto& u = g.units[j];
        std::snprintf(buf, sizeof(buf),
                      "{\"start\":%.3f,\"end\":%.3f,\"text\":\"", u.start,
                      u.end);
        out += std::string(buf) + JsonEscape(u.text) + "\"}";
        if (j + 1 < g.units.size()) out += ",";
      }
      out += "]}";
      if (i + 1 < align_view.size()) out += ",";
    }
    out += "]}";
  }
  out += "]";  // close "tracks"

  // Comprehensive view: diarization-attributed ASR text pieces, ordered by
  // time, with source text_id boundaries preserved.
  out += ",\"comprehensive\":[";
  if (asr_) {
    for (size_t i = 0; i < comp_view.size(); ++i) {
      const auto& e = comp_view[i];
      // Extract numeric speaker index from "speaker_N" format.
      int spk_idx = -1;
      if (e.speaker.size() > 8 && e.speaker.substr(0, 8) == "speaker_") {
        try {
          spk_idx = std::stoi(e.speaker.substr(8));
        } catch (const std::invalid_argument&) {
          spk_idx = -1;
        } catch (const std::out_of_range&) {
          spk_idx = -1;
        }
      }
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,"
                    "\"speaker\":%d",
                    e.start, e.end, e.text_id, spk_idx);
      out += buf;
      // Spec 010: the comprehensive entry carries the resolved global
      // voiceprint id (and optional display name) for this exact interval. Do
      // not remap by diarizer-local label here: local labels can drift to a new
      // global identity later in the session.
      std::string entry_speaker_id = e.speaker_id;
      if (entry_speaker_id.empty()) {
        auto id_it = speaker_label_ids.find(e.speaker);
        if (id_it != speaker_label_ids.end()) entry_speaker_id = id_it->second;
      }
      if (!entry_speaker_id.empty()) {
        out += ",\"speaker_id\":\"" + entry_speaker_id + "\"";
        const std::string nm =
            speaker_db_ ? speaker_db_->DisplayName(entry_speaker_id) : std::string();
        if (!nm.empty()) out += ",\"speaker_name\":\"" + JsonEscape(nm) + "\"";
      }
      out += ",\"text\":\"" + JsonEscape(e.text) + "\"}";
      if (i + 1 < comp_view.size()) out += ",";
    }
  }
  out += "]}";
  return out;
}

// ---------------------------------------------------------------------------
// Spec 006: speaker registry list + rename for the Web UI naming panel.
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeSpeakers() const {
  // List every global identity resolved anywhere in this session (accumulated
  // in the comprehensive timeline, read under comp_mutex_), joined with the
  // display names from the speaker database (its own name mutex). Using the
  // accumulated set — not the <=N current diarizer-slot mappings — keeps the
  // speaker panel consistent with the transcript, which accumulates ids the
  // same way. (This avoids racing the diarization thread on the database's
  // embedding/id vectors.)
  std::vector<std::string> id_list;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    id_list = comp_.AllSpeakerIds();
  }
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
