// Offline evidence dump for Sortformer diarization.
//
// Reads runtime parameters from orator.toml, runs the current Sortformer
// configuration over an audio file, and writes frame-level speaker probabilities
// plus the current onset/offset segment view. This is a diagnostic probe only;
// it does not change the runtime path.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/types.h"
#include "io/audio_file.h"
#include "io/config_reader.h"
#include "model/streaming_sortformer.h"
#include "pipeline/auditory_stream.h"
#include "pipeline/diar_postprocess.h"

namespace {

using orator::core::DiarizationFrames;
using orator::pipeline::AuditoryStream;

struct Top2 {
  int first = -1;
  int second = -1;
  float first_prob = 0.0f;
  float second_prob = 0.0f;
};

struct FrameRow {
  long frame = 0;
  double time_sec = 0.0;
  int session = 0;
  std::vector<float> probs;
};

Top2 FindTop2(const std::vector<float>& probs) {
  Top2 top;
  for (int s = 0; s < static_cast<int>(probs.size()); ++s) {
    const float p = probs[static_cast<size_t>(s)];
    if (top.first < 0 || p > top.first_prob) {
      top.second = top.first;
      top.second_prob = top.first_prob;
      top.first = s;
      top.first_prob = p;
    } else if (top.second < 0 || p > top.second_prob) {
      top.second = s;
      top.second_prob = p;
    }
  }
  return top;
}

double MeanMargin(const std::vector<FrameRow>& rows, double start_sec,
                  double end_sec) {
  double sum = 0.0;
  int count = 0;
  for (const auto& row : rows) {
    if (row.time_sec < start_sec || row.time_sec >= end_sec) continue;
    const Top2 top = FindTop2(row.probs);
    sum += static_cast<double>(top.first_prob - top.second_prob);
    ++count;
  }
  return count > 0 ? sum / count : 0.0;
}

void ApplyDiarTuning(const AuditoryStream::Config& cfg,
                     orator::model::SortformerDiarizer* diar) {
  orator::model::SortformerTuning tuning;
  tuning.spkcache_len = cfg.diar_spkcache_len;
  tuning.chunk_len = cfg.diar_chunk_len;
  tuning.spkcache_update_period = cfg.diar_spkcache_update_period;
  tuning.chunk_left_context = cfg.diar_chunk_left_context;
  tuning.chunk_right_context = cfg.diar_chunk_right_context;
  tuning.spkcache_sil_frames = cfg.diar_spkcache_sil_frames;
  tuning.spkcache_refresh_rate = cfg.diar_spkcache_refresh_rate;
  tuning.use_silence_profile = cfg.diar_use_silence_profile ? 1 : 0;
  tuning.fifo_len = cfg.diar_fifo_len;
  diar->ApplyStreamingTuning(tuning);
}

void AppendFrameRows(const DiarizationFrames& frames, int session,
                     std::vector<FrameRow>* rows) {
  for (int f = 0; f < frames.num_frames; ++f) {
    FrameRow row;
    row.frame = static_cast<long>(rows->size());
    row.time_sec =
        frames.t_start_sec + static_cast<double>(f) * frames.frame_period_sec;
    row.session = session;
    row.probs.resize(static_cast<size_t>(frames.num_speakers));
    for (int s = 0; s < frames.num_speakers; ++s) {
      row.probs[static_cast<size_t>(s)] = frames.At(f, s);
    }
    rows->push_back(std::move(row));
  }
}

void WriteFrameCsv(const std::string& path, const std::vector<FrameRow>& rows,
                   int num_speakers, double onset) {
  std::ofstream out(path);
  if (!out) throw std::runtime_error("cannot write frame csv: " + path);

  out << "frame,time_sec,session,top1,top1_prob,top2,top2_prob,margin,"
         "active_count";
  for (int s = 0; s < num_speakers; ++s) out << ",spk" << s;
  out << "\n";

  out << std::fixed << std::setprecision(6);
  for (const auto& row : rows) {
    const Top2 top = FindTop2(row.probs);
    int active_count = 0;
    for (float prob : row.probs) {
      if (prob >= onset) ++active_count;
    }
    out << row.frame << "," << row.time_sec << "," << row.session << ","
        << top.first << "," << top.first_prob << "," << top.second << ","
        << top.second_prob << "," << (top.first_prob - top.second_prob) << ","
        << active_count;
    for (int s = 0; s < num_speakers; ++s) {
      const float prob =
          s < static_cast<int>(row.probs.size()) ? row.probs[s] : 0.0f;
      out << "," << prob;
    }
    out << "\n";
  }
}

void WriteSegmentCsv(const std::string& path, const std::vector<FrameRow>& rows,
                     int speakers_per_session,
                     const std::vector<orator::core::DiarSegment>& segs) {
  std::ofstream out(path);
  if (!out) throw std::runtime_error("cannot write segment csv: " + path);

  out << "start_sec,end_sec,duration_sec,session,local_speaker,confidence,"
         "mean_margin\n";
  out << std::fixed << std::setprecision(6);
  for (const auto& s : segs) {
    const int session =
        speakers_per_session > 0 ? s.local_speaker / speakers_per_session : 0;
    out << s.start_sec << "," << s.end_sec << "," << (s.end_sec - s.start_sec)
        << "," << session << "," << s.local_speaker << "," << s.confidence
        << "," << MeanMargin(rows, s.start_sec, s.end_sec) << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  const std::string audio_path = argc > 1 ? argv[1] : "test.mp3";
  const std::string out_prefix =
      argc > 2 ? argv[2] : "/tmp/orator_diar_evidence";
  const char* cfg_env = std::getenv("ORATOR_CONFIG");
  const std::string config_path = cfg_env ? cfg_env : "orator.toml";

  AuditoryStream::Config cfg;
  orator::io::ApplyTomlConfig(config_path, cfg);

  std::cout << "config=" << config_path << "\n";
  std::cout << "audio=" << audio_path << "\n";
  std::cout << "diarizer_weights=" << cfg.diarizer_weights << "\n";
  std::cout << "spkcache_len=" << cfg.diar_spkcache_len
            << " chunk_len=" << cfg.diar_chunk_len
            << " left=" << cfg.diar_chunk_left_context
            << " right=" << cfg.diar_chunk_right_context
            << " fifo_len=" << cfg.diar_fifo_len
            << " refresh=" << cfg.diar_spkcache_refresh_rate << "\n";
  std::cout << "onset=" << cfg.diar_onset << " offset=" << cfg.diar_offset
            << " min_dur_on=" << cfg.diar_min_dur_on
            << " min_dur_off=" << cfg.diar_min_dur_off
            << " reset_period_sec=" << cfg.diar_reset_period_sec << "\n";

  orator::io::AudioData audio =
      orator::io::LoadAudioMono(audio_path, cfg.sample_rate);
  std::cout << "samples=" << audio.samples.size() << " sr="
            << audio.sample_rate << " duration=" << audio.DurationSec()
            << "s\n";

  orator::model::SortformerDiarizer diar;
  orator::core::DiarizationConfig dc;
  dc.sample_rate = cfg.sample_rate;
  dc.max_speakers = cfg.max_speakers;
  dc.activity_threshold = cfg.diar_threshold;
  diar.Initialize(dc);
  diar.LoadWeights(cfg.diarizer_weights);
  ApplyDiarTuning(cfg, &diar);

  const auto t0 = std::chrono::steady_clock::now();
  std::vector<FrameRow> rows;
  std::vector<orator::core::DiarSegment> segs;
  int session_count = 0;
  const int speakers_per_session = cfg.max_speakers;

  auto run_session = [&](size_t offset_samples, size_t count_samples,
                         int session) {
    orator::core::AudioChunk chunk;
    chunk.samples = audio.samples.data() + offset_samples;
    chunk.num_samples = static_cast<int>(count_samples);
    chunk.sample_rate = audio.sample_rate;
    chunk.t_start_sec =
        static_cast<double>(offset_samples) / static_cast<double>(audio.sample_rate);
    DiarizationFrames frames = diar.ProcessChunk(chunk);
    AppendFrameRows(frames, session, &rows);
    std::vector<orator::core::DiarSegment> part =
        orator::pipeline::OnsetOffsetSegments(
            frames, cfg.diar_onset, cfg.diar_offset, cfg.diar_pad_onset,
            cfg.diar_pad_offset, cfg.diar_min_dur_on, cfg.diar_min_dur_off);
    const int speaker_offset = session * frames.num_speakers;
    for (auto& seg : part) {
      seg.local_speaker += speaker_offset;
      segs.push_back(seg);
    }
  };

  if (cfg.diar_reset_period_sec > 0.0) {
    const size_t session_samples = static_cast<size_t>(
        std::llround(cfg.diar_reset_period_sec * audio.sample_rate));
    for (size_t off = 0; off < audio.samples.size();) {
      const size_t count =
          std::min(session_samples, audio.samples.size() - off);
      run_session(off, count, session_count);
      ++session_count;
      off += count;
      diar.Reset();
    }
  } else {
    run_session(0, audio.samples.size(), /*session=*/0);
    session_count = 1;
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double compute_sec = std::chrono::duration<double>(t1 - t0).count();

  const std::string frame_csv = out_prefix + "_frames.csv";
  const std::string segment_csv = out_prefix + "_segments.csv";
  WriteFrameCsv(frame_csv, rows, cfg.max_speakers, cfg.diar_onset);
  WriteSegmentCsv(segment_csv, rows, speakers_per_session, segs);

  const double audio_sec = audio.DurationSec();
  std::cout << "frames=" << rows.size() << " speakers_per_session="
            << speakers_per_session << " sessions=" << session_count << "\n";
  std::cout << "segments=" << segs.size() << "\n";
  std::cout << "compute_sec=" << compute_sec
            << " rtf=" << (compute_sec > 0.0 ? compute_sec / audio_sec : 0.0)
            << "\n";
  std::cout << "frame_csv=" << frame_csv << "\n";
  std::cout << "segment_csv=" << segment_csv << "\n";
  return 0;
}
