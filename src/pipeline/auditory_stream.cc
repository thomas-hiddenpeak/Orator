#include "pipeline/auditory_stream.h"
#include "pipeline/auditory_stream_subscriptions.h"
#include "pipeline/periodic_deadline.h"

#include "core/log.h"
#include "core/registry.h"
#include "gpu/gpu_lock.h"
#include "model/builtin_registration.h"
#include "model/speaker_database.h"
#include "model/streaming_sortformer.h"
#include "model/titanet_embedder.h"
#include "pipeline/speaker_identity_stage.h"
#include "pipeline/speaker_evidence_stage.h"
#include "protocol/protocol_timeline.h"
#include "protocol/session_store.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <utility>

#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

AuditoryStream::AuditoryStream(const Config& config, Emit emit)
    : config_(config),
      time_base_(config.sample_rate, 0),
      emit_(std::move(emit)) {}

AuditoryStream::~AuditoryStream() {
  StopWorkers();
  comp_.UnsubscribeAsrFinals(comp_asr_subscription_id_);
  business_speaker_pipeline_.reset();
}

void AuditoryStream::Start() {
  gpu::SchedulingMode gpu_mode = gpu::SchedulingMode::kAuto;
  if (config_.gpu_scheduling_mode == 1) {
    gpu_mode = gpu::SchedulingMode::kSerial;
  } else if (config_.gpu_scheduling_mode == 2) {
    gpu_mode = gpu::SchedulingMode::kConcurrent;
  }
  gpu::ConfigureSchedulingMode(gpu_mode);
  model::EnsureBuiltinsRegistered();
  {
    std::string session_dir = config_.session_dir;
    if (session_dir.empty() && !config_.storage_disk_path.empty()) {
      session_dir = config_.storage_disk_path;
      if (session_dir.back() != '/') session_dir.push_back('/');
      session_dir += "sessions";
    }
    session_store_ = std::make_unique<protocol::SessionStore>(session_dir);
  }
  protocol_timeline_ = std::make_unique<protocol::ProtocolTimeline>(
      128 * 1024 * 1024, config_.storage_disk_path,
      "session_" + std::to_string(session_start_wall_sec_.load()));

  protocol::PipelineDescriptor ws_desc;
  ws_desc.name = "ws_input";
  ws_desc.version = "1.0.0";
  ws_desc.produces = {protocol::kAudioRaw};
  ws_input_handle_ = protocol_timeline_->RegisterPipeline(std::move(ws_desc));

  protocol::PipelineDescriptor vad_desc;
  vad_desc.name = "vad";
  vad_desc.version = "1.0.0";
  vad_desc.enabled = config_.vad_stream && !config_.vad_model.empty();
  vad_desc.produces = {protocol::kVadSpeechSegment, protocol::kVadProgress};
  vad_desc.consumes = {protocol::TopicPattern{"audio/+"}};
  vad_handle_ = protocol_timeline_->RegisterPipeline(std::move(vad_desc));

  protocol::PipelineDescriptor asr_desc;
  asr_desc.name = "asr";
  asr_desc.version = "1.0.0";
  asr_desc.enabled = !config_.asr_model_dir.empty();
  asr_desc.produces = {protocol::kAsrTranscript,
                       protocol::kAsrTranscriptPartial};
  asr_desc.consumes = {protocol::TopicPattern{"vad/+"}};
  asr_handle_ = protocol_timeline_->RegisterPipeline(std::move(asr_desc));

  protocol::PipelineDescriptor diar_desc;
  diar_desc.name = "diar";
  diar_desc.version = "1.0.0";
  diar_desc.enabled = !config_.diarizer_weights.empty();
  diar_desc.produces = {protocol::kDiarSpeakerSegment};
  diar_desc.consumes = {protocol::TopicPattern{"audio/+"}};
  diar_handle_ = protocol_timeline_->RegisterPipeline(std::move(diar_desc));

  protocol::PipelineDescriptor business_desc;
  business_desc.name = "business_speaker";
  business_desc.version = "1.0.0";
  business_desc.produces = {protocol::kBusinessSpeakerRevision};
  business_desc.consumes = {protocol::TopicPattern{"diar/+"},
                            protocol::TopicPattern{"asr/transcript"},
                            protocol::TopicPattern{"align/+"}};
  business_speaker_handle_ =
      protocol_timeline_->RegisterPipeline(std::move(business_desc));

  BusinessSpeakerPipeline::Config business_config;
  business_config.align_snap_pause_sec = config_.timeline_align_snap_pause_sec;
  business_config.align_boundary_split_tolerance_sec =
      config_.timeline_align_boundary_split_tolerance_sec;
  business_config.speaker_support_min_coverage_ratio =
      config_.timeline_speaker_support_min_coverage_ratio;
  business_config.speaker_support_max_gap_sec =
      config_.timeline_speaker_support_max_gap_sec;
  business_config.speaker_support_max_islands =
      config_.timeline_speaker_support_max_islands;
  business_config.gap_fill_enabled = config_.timeline_gap_fill_enabled;
  business_config.voiceprint_fusion_enabled = config_.speaker_fusion_enable;
  business_config.voiceprint_short_max_sec =
      config_.speaker_fusion_short_max_sec;
  business_config.voiceprint_short_min_score =
      config_.speaker_fusion_short_min_score;
  business_config.voiceprint_short_min_margin =
      config_.speaker_fusion_short_min_margin;
  business_config.voiceprint_regular_min_score =
      config_.speaker_fusion_regular_min_score;
  business_config.voiceprint_regular_min_margin =
      config_.speaker_fusion_regular_min_margin;
  business_config.voiceprint_primary_consensus_min_sec =
      config_.speaker_fusion_min_embed_sec;
  business_config.voiceprint_phrase_max_sec =
      config_.speaker_fusion_phrase_max_sec;
  business_config.voiceprint_four_view_min_aligned_units =
      config_.speaker_fusion_four_view_min_aligned_units;
  business_config.voiceprint_future_epoch_lookahead_sec =
      config_.speaker_fusion_future_epoch_lookahead_sec;
  business_config.voiceprint_punctuation = config_.speaker_fusion_punctuation;
  if (config_.timeline_speaker_overlap_tie_policy == "higher_confidence") {
    business_config.speaker_overlap_tie_policy =
        BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kHigherConfidence;
  } else if (config_.timeline_speaker_overlap_tie_policy ==
             "primary_speaker") {
    business_config.speaker_overlap_tie_policy =
        BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
  } else {
    business_config.speaker_overlap_tie_policy =
        BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kShorterSpan;
  }
  business_speaker_pipeline_ = std::make_unique<BusinessSpeakerPipeline>(
      &comp_, business_config, common_time_base(),
      [this](const ComprehensiveTimeline::Revision& revision) {
        HandleBusinessSpeakerRevision(
            protocol_timeline_.get(), business_speaker_handle_.get(),
            [this](const std::string& json) { EmitLocked(json); }, revision);
      });
  business_speaker_pipeline_->Start();

  const bool align_on =
      config_.align_enable && !config_.align_model_dir.empty();
  if (align_on) {
    protocol::PipelineDescriptor align_desc;
    align_desc.name = "align";
    align_desc.version = "1.0.0";
    align_desc.produces = {protocol::kAlignUnits};
    align_desc.consumes = {protocol::TopicPattern{"asr/+"}};
    align_handle_ = protocol_timeline_->RegisterPipeline(std::move(align_desc));
  }

  // Forced alignment consumes finalized ASR evidence only through the typed
  // comprehensive timeline subscription. Protocol topics mirror the same
  // records for persistence and transport; they are not the private data bus.
  if (align_on && comp_asr_subscription_id_ == 0) {
    comp_asr_subscription_id_ = comp_.SubscribeAsrFinals(
        [this](const ComprehensiveTimeline::RawTextSeg& segment) {
          if (!align_worker_) return;
          align_worker_->Enqueue(segment.id, segment.start, segment.end,
                                 segment.text);
        });
  }

  if (!config_.diarizer_weights.empty()) {
    diarizer_ =
        core::Registry<core::IDiarizer>::Instance().Create("sortformer");
    // Streaming capacities are part of model construction. Apply the complete
    // typed profile before Initialize allocates state for those capacities.
    model::SortformerTuning tuning;
    tuning.spkcache_len = config_.diar_spkcache_len;
    tuning.chunk_len = config_.diar_chunk_len;
    tuning.spkcache_update_period = config_.diar_spkcache_update_period;
    tuning.chunk_left_context = config_.diar_chunk_left_context;
    tuning.chunk_right_context = config_.diar_chunk_right_context;
    tuning.spkcache_sil_frames = config_.diar_spkcache_sil_frames;
    tuning.fifo_len = config_.diar_fifo_len;
    tuning.show_progress = config_.stream_progress ? 1 : 0;
    static_cast<model::SortformerDiarizer*>(diarizer_.get())
        ->ApplyStreamingTuning(tuning);
    core::DiarizationConfig dc;
    dc.sample_rate = config_.sample_rate;
    dc.max_speakers = config_.max_speakers;
    dc.activity_threshold = config_.diar_threshold;
    dc.show_progress = config_.stream_progress;
    diarizer_->Initialize(dc);
    diarizer_->LoadWeights(config_.diarizer_weights);
    diar_stream_ = scheduler_.Register("diarization", /*priority_index=*/0,
                                       /*background=*/false,
                                       /*create_stream=*/true);
  }
  if (!config_.asr_model_dir.empty()) {
    asr_ = core::Registry<core::IAsr>::Instance().Create("qwen3_asr");
    core::AsrConfig ac;
    ac.sample_rate = config_.sample_rate;
    ac.language = config_.asr_language;
    ac.system_prompt = config_.asr_system_prompt;
    ac.eos_ban_steps = config_.asr_ban_steps;
    ac.decode_batch = config_.asr_decode_batch;
    ac.profile = config_.asr_profile;
    ac.encoder_windowed_attention = config_.asr_windowed_encoder;
    ac.cuda_graph_enabled = config_.asr_cuda_graph_enabled;
    asr_->Initialize(ac);
    asr_->set_max_new_tokens(config_.asr_max_new_tokens);
    asr_->LoadWeights(config_.asr_model_dir);
    asr_stream_ = scheduler_.Register("asr", /*priority_index=*/2,
                                      /*background=*/false,
                                      /*create_stream=*/true);
    int greatest = 0, least = 0;
    scheduler_.PriorityRange(&greatest, &least);
    LOG_INFO(
        "[gpu-sched] priority range [greatest=%d, least=%d]; "
        "asr at index 1 (foreground)\n",
        greatest, least);
  }
  if (align_on) {
    aligner_ = core::Registry<core::IForcedAligner>::Instance().Create(
        "qwen3_forced_aligner");
    aligner_->set_profile(config_.align_profile);
    aligner_->LoadWeights(config_.align_model_dir);
  }

  // Spec 010: speaker identity (post-diarization stage inside the diar
  // pipeline). Gated on config; needs the diarizer and the TitaNet weights.
  const bool speaker_on =
      config_.speaker_enable && !config_.speaker_model_dir.empty() && diarizer_;
  if (speaker_on) {
    std::string wpath = config_.speaker_model_dir;
    if (!wpath.empty() && wpath.back() != '/') wpath += '/';
    wpath += "titanet_large.safetensors";
    speaker_embedder_ = std::make_unique<model::TitaNetEmbedder>();
    speaker_embedder_->LoadWeights(wpath);
    speaker_stream_ = scheduler_.Register(
        "speaker_embedding", /*priority_index=*/5, /*background=*/true,
        /*create_stream=*/true);
    speaker_embedder_->SetStream(speaker_stream_);
    const double warmup_window_sec =
        std::max(config_.speaker_max_embed_window_sec,
                 config_.speaker_fusion_max_embed_window_sec);
    speaker_embedder_->Warmup(
        static_cast<int>(std::ceil(warmup_window_sec * config_.sample_rate)));
    speaker_db_ = std::make_unique<model::SpeakerDatabase>(
        /*max_speakers=*/256, speaker_embedder_->dim());
    if (!config_.speaker_registry_path.empty()) {
      speaker_db_->Load(config_.speaker_registry_path);  // ok if absent
    }
    SpeakerIdConfig sc;
    sc.embedding_dim = speaker_embedder_->dim();
    sc.match_threshold = config_.speaker_match_threshold;
    sc.min_embed_sec = config_.speaker_min_embed_sec;
    sc.min_confidence = config_.speaker_min_confidence;
    sc.retain_sec = config_.speaker_retain_sec;
    sc.overlap_eps_sec = config_.speaker_overlap_eps_sec;
    sc.max_ref_segs = config_.speaker_max_ref_segs;
    sc.edge_margin_sec = config_.speaker_edge_margin_sec;
    sc.max_embed_window_sec = config_.speaker_max_embed_window_sec;
    sc.enroll_min_refs = config_.speaker_enroll_min_refs;
    sc.speakers_per_session = config_.speaker_speakers_per_session;
    sc.merge_threshold = config_.speaker_merge_threshold;
    sc.cosession_merge_threshold = config_.speaker_cosession_merge_threshold;
    sc.cross_session_match_min_refs =
        config_.speaker_cross_session_match_min_refs;
    sc.defer_unmatched_cross_session =
        config_.speaker_defer_unmatched_cross_session;
    sc.local_drift_threshold = config_.speaker_local_drift_threshold;
    sc.local_drift_min_span_sec = config_.speaker_local_drift_min_span_sec;
    sc.local_drift_min_epoch_sec = config_.speaker_local_drift_min_epoch_sec;
    sc.local_drift_allow_same_session_match =
        config_.speaker_local_drift_allow_same_session_match;
    sc.local_drift_competing_threshold =
        config_.speaker_local_drift_competing_threshold;
    sc.local_drift_competing_margin =
        config_.speaker_local_drift_competing_margin;
    sc.local_drift_competing_min_span_sec =
        config_.speaker_local_drift_competing_min_span_sec;
    sc.local_drift_competing_candidate_threshold =
        config_.speaker_local_drift_competing_candidate_threshold;
    sc.local_drift_competing_candidate_margin =
        config_.speaker_local_drift_competing_candidate_margin;
    sc.local_drift_competing_candidate_min_confirmations =
        config_.speaker_local_drift_competing_candidate_min_confirmations;
    sc.local_drift_competing_backfill_sec =
        config_.speaker_local_drift_competing_backfill_sec;
    sc.local_drift_competing_backfill_gap_sec =
        config_.speaker_local_drift_competing_backfill_gap_sec;
    speaker_id_stage_ = std::make_unique<SpeakerIdentityStage>(
        speaker_embedder_.get(), speaker_db_.get(), common_time_base(), sc);
    SpeakerEvidenceStage::Config evidence_config;
    evidence_config.enabled = config_.speaker_fusion_enable;
    evidence_config.min_embed_sec = config_.speaker_fusion_min_embed_sec;
    evidence_config.edge_margin_sec =
        config_.speaker_fusion_edge_margin_sec;
    evidence_config.max_embed_window_sec =
        config_.speaker_fusion_max_embed_window_sec;
    evidence_config.phrase_min_sec = config_.speaker_fusion_phrase_min_sec;
    evidence_config.phrase_max_sec = config_.speaker_fusion_phrase_max_sec;
    evidence_config.short_max_sec = config_.speaker_fusion_short_max_sec;
    evidence_config.boundary_tolerance_sec =
        config_.timeline_align_boundary_split_tolerance_sec;
    evidence_config.punctuation = config_.speaker_fusion_punctuation;
    evidence_config.frame_activity_threshold =
        config_.speaker_fusion_frame_activity_threshold;
    evidence_config.minimum_gallery_size =
        config_.speaker_fusion_minimum_gallery_size;
    evidence_config.precompute_interval_sec =
        config_.speaker_fusion_precompute_interval_sec;
    evidence_config.precompute_max_spans_per_cycle =
        config_.speaker_fusion_precompute_max_spans_per_cycle;
    speaker_evidence_stage_ = std::make_unique<SpeakerEvidenceStage>(
        speaker_id_stage_.get(), std::move(evidence_config));
    LOG_INFO("[speaker-id] enabled: %s (registry %s)\n", wpath.c_str(),
             config_.speaker_registry_path.empty()
                 ? "(none)"
                 : config_.speaker_registry_path.c_str());
  }
  StartWorkers();
}

void AuditoryStream::StartWorkers() {
  vad_processed_samples_.store(0);
  timebase_reconciled_.store(false);
  timebase_ok_.store(true);
  if (diarizer_) {
    DiarizationWorker::Params dp;
    dp.threshold = config_.diar_threshold;
    dp.merge_gap_sec = config_.diar_merge_gap_sec;
    dp.deliver_interval_sec = config_.diar_deliver_interval_sec;
    dp.sample_rate = config_.sample_rate;
    dp.onset = config_.diar_onset;
    dp.offset = config_.diar_offset;
    dp.pad_onset = config_.diar_pad_onset;
    dp.pad_offset = config_.diar_pad_offset;
    dp.min_dur_on = config_.diar_min_dur_on;
    dp.min_dur_off = config_.diar_min_dur_off;
    dp.reset_period_sec = config_.diar_reset_period_sec;
    diar_worker_ = std::make_unique<DiarizationWorker>(
        diarizer_.get(), dp, common_time_base(), diar_stream_);
    diar_worker_->set_speaker_sink(
        [this](const std::vector<core::DiarSegment>& segs) {
          HandleSpeakerSink(
              comp_, comp_mutex_, last_segments_, protocol_timeline_.get(),
              diar_handle_.get(),
              [this](const std::string& json) { EmitLocked(json); }, segs);
        });
    diar_worker_->set_frame_sink(
        [this](const core::DiarizationFrames& frames, int local_offset) {
          ComprehensiveTimeline::DiarFrameBlock block;
          block.start = frames.t_start_sec;
          block.frame_period_sec = frames.frame_period_sec;
          block.num_frames = frames.num_frames;
          block.num_speakers = frames.num_speakers;
          block.local_speaker_offset = local_offset;
          block.probabilities = frames.probs;
          const auto result = comp_.DepositDiarFrameBlock(block);
          if (result == ComprehensiveTimeline::DepositResult::kConflict ||
              result == ComprehensiveTimeline::DepositResult::kInvalid) {
            LOG_ERROR("[diar] rejected raw frame block at %.3f\n",
                      frames.t_start_sec);
          }
        });
    // Spec 010: resolve global voiceprint identities on the segment view before
    // it is delivered. The worker depends only on a std::function (Art. III);
    // identity needs no VAD -- Sortformer's confidence + no-overlap already
    // mark clean single-speaker spans.
    if (speaker_id_stage_) {
      diar_worker_->set_segment_processor(
          [this](std::vector<core::DiarSegment>& segs) {
            speaker_id_stage_->Process(segs);
          });
    }
    diar_audio_ = MakeAudioCache();
    diar_thread_ = std::thread([this] {
      std::vector<float> chunk;
      long span_start_abs = 0;
      while (diar_audio_->WaitAndRead(&chunk, &span_start_abs)) {
        if (speaker_id_stage_) {
          speaker_id_stage_->AppendAudio(chunk.data(),
                                         static_cast<int>(chunk.size()));
        }
        diar_worker_->ProcessSpan(chunk.data(), static_cast<int>(chunk.size()));
        progress_cv_.notify_all();
      }
      diar_worker_->Finalize();
      progress_cv_.notify_all();
    });
  }
  if (asr_) {
    AsrWorker::Params p;
    p.sample_rate = config_.sample_rate;
    p.segment_sec = config_.asr_segment_sec;
    p.asr_vad_gate = config_.asr_vad_gate && config_.vad_stream &&
                     !config_.vad_model.empty();
    p.asr_vad_lead_ms = config_.asr_vad_lead_ms;
    p.asr_vad_gate_chunk_ms = config_.asr_vad_gate_chunk_ms;
    p.asr_vad_trail_sec = config_.asr_vad_trail_sec;
    p.asr_vad_min_overlap_sec = config_.asr_vad_min_overlap_sec;
    p.max_audio_tokens = config_.asr_max_audio_tokens;

    asr_worker_ = std::make_unique<AsrWorker>(
        asr_.get(), p, [this](const std::string& json) { EmitLocked(json); },
        common_time_base(), asr_stream_, &comp_);
    asr_worker_->set_text_sink([this](long id, double start, double end,
                                      const std::string& text, bool is_final) {
      HandleTextSink(comp_, protocol_timeline_.get(), asr_handle_.get(), id,
                     start, end, text, is_final);
    });
    asr_audio_ = MakeAudioCache();
    asr_thread_ = std::thread([this] {
      std::vector<float> chunk;
      long span_start_abs = 0;
      // Drain the whole (possibly flooded) backlog each read and run it at the
      // device's max speed. The ASR's VAD gate correlates speech boundaries by
      // AUDIO time (not by read size), so transcription is identical whether
      // audio arrived in real time or all at once -- no throttling here.
      while (asr_audio_->WaitAndRead(&chunk, &span_start_abs)) {
        asr_worker_->ProcessSpan(chunk.data(), static_cast<int>(chunk.size()));
        progress_cv_.notify_all();
      }
      progress_cv_.notify_all();
    });
  }
  if (aligner_) {
    align_audio_ = std::make_unique<RetainedAudioBuffer>(
        common_time_base(), config_.align_retain_sec);
    AlignWorker::Params ap;
    ap.sample_rate = config_.sample_rate;
    ap.language = config_.align_language;
    ap.max_segment_sec = config_.align_max_segment_sec;
    align_worker_ = std::make_unique<AlignWorker>(
        aligner_.get(), align_audio_.get(), ap, common_time_base());
    align_worker_->set_sink([this](long id, double seg_start, double seg_end,
                                   const std::vector<core::AlignUnit>& units) {
      HandleAlignSink(
          comp_, protocol_timeline_.get(), align_handle_.get(),
          [this](const std::string& j) { EmitLocked(j); }, id, seg_start,
          seg_end, units);
    });
    align_worker_->Start();
  }
  if (config_.vad_stream && !config_.vad_model.empty()) {
    vad_stream_ = scheduler_.Register("vad", /*priority_index=*/1,
                                      /*background=*/false,
                                      /*create_stream=*/true);
    GpuVad::Params vp;
    vp.sample_rate = config_.sample_rate;
    vp.silero_model_path = config_.vad_model;
    vp.silero_threshold = config_.vad_threshold;
    vp.silero_min_speech_ms = config_.vad_min_speech_ms;
    vp.silero_min_silence_ms = config_.vad_min_silence_ms;
    vp.silero_speech_pad_ms = config_.vad_speech_pad_ms;
    vp.stream = vad_stream_;
    auto vad_params_ptr = std::make_shared<GpuVad::Params>(vp);
    core::Registry<core::IVad>::Instance().Register(
        "silero_vad",
        [vad_params_ptr] { return std::make_unique<GpuVad>(*vad_params_ptr); });
    vad_detector_ = core::Registry<core::IVad>::Instance().Create("silero_vad");
    vad_audio_ = MakeAudioCache();
    vad_thread_ = std::thread([this] {
      const core::TimeBase tb = common_time_base();
      std::vector<float> chunk;
      std::vector<core::VadSegmentResult> segs;
      auto drain = [this, &tb, &segs](bool finalize) {
        HandleVadDrain(
            vad_detector_.get(), comp_, protocol_timeline_.get(),
            vad_handle_.get(),
            [this](const std::string& json) { EmitLocked(json); }, tb, &segs,
            finalize);
      };
      // Publish only the detector-owned stable silence frontier. Its lookback
      // covers onset confirmation and configured padding, so later speech can
      // never revise audio that ASR has already consumed.
      constexpr double kMinAdvanceSec = 0.25;
      double last_horizon = -1e9;
      long span_start_abs = 0;
      bool vad_speech_emitted = false;
      bool first_vad_state = true;
      while (vad_audio_->WaitAndRead(&chunk, &span_start_abs)) {
        vad_detector_->Push(chunk.data(), static_cast<int>(chunk.size()));
        const long fed = span_start_abs + static_cast<long>(chunk.size());
        drain(/*finalize=*/false);
        const core::VadStateResult state = vad_detector_->state();
        const double active_start =
            state.active_start_sample >= 0
                ? tb.SecondsAt(state.active_start_sample)
                : -1e9;
        const double active_horizon =
            state.active_stable_until_sample >= 0
                ? tb.SecondsAt(state.active_stable_until_sample)
                : -1e9;
        comp_.UpdateVadState(state.in_speech,
                             tb.SecondsAt(state.observed_until_sample),
                             active_start, active_horizon);
        if (!state.in_speech && state.silence_stable_until_sample >= 0) {
          const double h = tb.SecondsAt(state.silence_stable_until_sample);
          if (h >= last_horizon + kMinAdvanceSec) {
            PublishVadProgress(comp_, protocol_timeline_.get(),
                               vad_handle_.get(), h);
            last_horizon = h;
          }
        }
        // Emit vad_state only on a speech<->silence TRANSITION, not every
        // frame. A per-frame emit (~10/s) is the dominant outbound-message
        // source and was the root of the WS backpressure that dropped long
        // connections; the UI only needs the transitions (it drives a speech
        // LED).
        {
          const bool sp = state.in_speech;
          if (first_vad_state || sp != vad_speech_emitted) {
            char buf[64];
            std::snprintf(buf, sizeof(buf),
                          "{\"type\":\"vad_state\",\"speech\":%s}",
                          sp ? "true" : "false");
            EmitLocked(buf);
            vad_speech_emitted = sp;
            first_vad_state = false;
          }
        }
        vad_processed_samples_.store(fed);
        progress_cv_.notify_all();
      }
      drain(/*finalize=*/true);
      const core::VadStateResult final_state = vad_detector_->state();
      comp_.UpdateVadState(
          final_state.in_speech,
          tb.SecondsAt(final_state.observed_until_sample),
          final_state.active_start_sample >= 0
              ? tb.SecondsAt(final_state.active_start_sample)
              : -1e9,
          final_state.active_stable_until_sample >= 0
              ? tb.SecondsAt(final_state.active_stable_until_sample)
              : -1e9);
      // Final horizon = everything fed is now decided; lets ASR skip any
      // trailing silence before its own finalize.
      const long final_processed =
          span_start_abs + static_cast<long>(chunk.size());
      PublishVadProgress(comp_, protocol_timeline_.get(), vad_handle_.get(),
                         tb.SecondsAt(final_processed));
      vad_processed_samples_.store(final_processed);
      progress_cv_.notify_all();
    });
  }
  if (config_.gpu_telemetry_interval_sec > 0.0) {
    telemetry_stop_ = false;
    telemetry_thread_ = std::thread([this] {
      const auto interval = std::chrono::ceil<PeriodicDeadline::Duration>(
          std::chrono::duration<double>(config_.gpu_telemetry_interval_sec));
      PeriodicDeadline schedule(PeriodicDeadline::Clock::now(), interval);
      for (;;) {
        {
          std::unique_lock<std::mutex> lk(telemetry_mutex_);
          telemetry_cv_.wait_until(lk, schedule.next(),
                                   [this] { return telemetry_stop_; });
          if (telemetry_stop_) break;
        }
        const std::string msg = SerializeGpuTelemetry();
        if (!msg.empty()) EmitLocked(msg);
        schedule.AdvancePast(PeriodicDeadline::Clock::now());
      }
    });
  }

  // Cursor progress telemetry thread
  if (config_.cursor_telemetry_interval_sec > 0.0) {
    cursor_telemetry_stop_ = false;
    cursor_telemetry_thread_ = std::thread([this] {
      const auto interval =
          std::chrono::duration<double>(config_.cursor_telemetry_interval_sec);
      for (;;) {
        {
          std::unique_lock<std::mutex> lk(cursor_telemetry_mutex_);
          cursor_telemetry_cv_.wait_for(
              lk, interval, [this] { return cursor_telemetry_stop_; });
          if (cursor_telemetry_stop_) break;
        }
        const std::string msg = SerializeCursorTelemetry();
        if (!msg.empty()) EmitLocked(msg);
      }
    });
  }

  if (speaker_evidence_stage_) {
    speaker_evidence_stage_->StartPrecompute(&comp_, [this] {
      const long target = total_samples_.load();
      const bool diar_ready =
          !diar_worker_ || diar_worker_->processed_samples() >= target;
      const bool asr_ready =
          !asr_worker_ || asr_worker_->processed_samples() >= target;
      const bool vad_ready =
          !vad_detector_ || vad_processed_samples_.load() >= target;
      return diar_ready && asr_ready && vad_ready;
    });
  }
  running_ = true;
}

void AuditoryStream::StopWorkers() {
  if (!running_) return;
  const auto finalize_begin = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lk(telemetry_mutex_);
    telemetry_stop_ = true;
  }
  telemetry_cv_.notify_all();
  if (telemetry_thread_.joinable()) telemetry_thread_.join();

  {
    std::lock_guard<std::mutex> lk(cursor_telemetry_mutex_);
    cursor_telemetry_stop_ = true;
  }
  cursor_telemetry_cv_.notify_all();
  if (cursor_telemetry_thread_.joinable()) cursor_telemetry_thread_.join();

  if (diar_audio_) diar_audio_->Close();
  if (asr_audio_) asr_audio_->Close();
  if (vad_audio_) vad_audio_->Close();
  if (diar_thread_.joinable()) diar_thread_.join();
  if (asr_thread_.joinable()) asr_thread_.join();
  if (vad_thread_.joinable()) vad_thread_.join();
  // The ASR collector never waits for VAD. Once both independent producers
  // have stopped, drain its pending tail from the final typed VAD snapshot.
  // The aligner remains alive to consume the finals emitted here.
  if (asr_worker_) {
    asr_worker_->Finalize();
    progress_cv_.notify_all();
  }
  const auto producers_drained = std::chrono::steady_clock::now();
  // Stop the aligner last: ASR Finalize() deposits final typed records which
  // the comprehensive-timeline subscription enqueues. The aligner must drain
  // after the ASR thread has joined.
  if (align_worker_) {
    align_worker_->Stop();
    align_worker_->FinalizeExtent(total_samples_.load());
  }
  const auto align_drained = std::chrono::steady_clock::now();
  if (speaker_evidence_stage_) {
    const auto primary_build_begin = std::chrono::steady_clock::now();
    const auto primary =
        speaker_evidence_stage_->BuildPrimarySpeaker(comp_.SnapshotTracks());
    // Primary top-1 is an independent typed track and can revise exact
    // activity ties. Voiceprint intervals must be generated from that revised
    // business view, otherwise coarse pre-primary source ranges overwrite the
    // more precise projection during the following evidence update.
    comp_.DepositPrimarySpeaker(primary);
    const auto primary_deposited = std::chrono::steady_clock::now();
    // Drain after the primary view is deposited so any business intervals it
    // revises are included in the final acoustic-only precomputation pass.
    speaker_evidence_stage_->StopPrecompute(/*drain=*/true);
    const auto precompute_drained = std::chrono::steady_clock::now();
    const auto voiceprint = speaker_evidence_stage_->BuildVoiceprint(
        comp_.SnapshotSpeakerEvidenceInputs());
    const auto voiceprint_built = std::chrono::steady_clock::now();
    comp_.DepositSpeakerVoiceprint(voiceprint);
    const auto voiceprint_deposited = std::chrono::steady_clock::now();
    LOG_INFO("[speaker-fusion] final evidence: primary=%zu voiceprint=%zu\n",
             primary.size(), voiceprint.size());
    const auto millis = [](auto end, auto begin) {
      return std::chrono::duration<double, std::milli>(end - begin).count();
    };
    LOG_INFO(
        "[finalize] producers=%.3fms align=%.3fms "
        "primary_build_and_deposit=%.3fms precompute_drain=%.3fms "
        "voiceprint_build=%.3fms "
        "voiceprint_deposit=%.3fms precomputed=%zu cached=%zu\n",
        millis(producers_drained, finalize_begin),
        millis(align_drained, producers_drained),
        millis(primary_deposited, primary_build_begin),
        millis(precompute_drained, primary_deposited),
        millis(voiceprint_built, precompute_drained),
        millis(voiceprint_deposited, voiceprint_built),
        speaker_evidence_stage_->precomputed_span_count(),
        speaker_id_stage_ ? speaker_id_stage_->cached_embedding_count() : 0);
  }
  if (business_speaker_pipeline_) {
    business_speaker_pipeline_->Finalize(total_samples_.load());
  }

  // Spec 010 D10: persist the speaker registry (+ name sidecar) once the diar
  // thread has joined, so no enrollment races the write. Skip an empty registry
  // so a session that enrolled nobody never clobbers a populated file on disk
  // (every stream Loads the registry at start, so non-empty state is
  // preserved).
  if (speaker_db_ && !config_.speaker_registry_path.empty() &&
      speaker_db_->Size() > 0) {
    if (speaker_db_->Save(config_.speaker_registry_path)) {
      LOG_INFO("[speaker-id] saved registry: %s (%d speakers)\n",
               config_.speaker_registry_path.c_str(), speaker_db_->Size());
    } else {
      LOG_WARN("[speaker-id] failed to save registry: %s\n",
               config_.speaker_registry_path.c_str());
    }
  }
  running_ = false;
}

double AuditoryStream::audio_sec() const {
  return common_time_base().Duration(total_samples_.load());
}

std::vector<AuditoryStream::TrackExtent> AuditoryStream::track_extents() const {
  const long total = total_samples_.load();
  std::vector<TrackExtent> extents;
  auto append = [&](const char* pipeline, long processed) {
    extents.push_back({pipeline, processed, total,
                       core::TimeBase::ReconcileExtent(processed, total)});
  };

  append("ws_input", total);
  if (diar_worker_) append("diarization", diar_worker_->processed_samples());
  if (speaker_id_stage_ && diar_worker_) {
    append("speaker_identity", diar_worker_->processed_samples());
  }
  if (asr_worker_) append("asr", asr_worker_->processed_samples());
  if (vad_detector_) append("vad", vad_processed_samples_.load());
  if (align_worker_) append("align", align_worker_->processed_samples());
  if (business_speaker_pipeline_) {
    append("business_speaker", business_speaker_pipeline_->processed_samples());
  }
  return extents;
}

void AuditoryStream::ReconcileFinalExtents() {
  bool ok = true;
  for (const auto& extent : track_extents()) {
    if (extent.gap_samples == 0) continue;
    ok = false;
    LOG_ERROR(
        "[timebase] %s processed %ld samples vs common total %ld (gap %ld)\n",
        extent.pipeline.c_str(), extent.processed_samples,
        extent.common_total_samples, extent.gap_samples);
  }
  timebase_ok_.store(ok);
  timebase_reconciled_.store(true);
}

double AuditoryStream::diar_compute_sec() const {
  return diar_worker_ ? diar_worker_->compute_sec() : 0.0;
}

double AuditoryStream::asr_compute_sec() const {
  return asr_worker_ ? asr_worker_->compute_sec() : 0.0;
}

std::string AuditoryStream::SerializeCursorTelemetry() const {
  const long total = total_samples_.load();
  std::string json = "{\"type\":\"cursor_progress\",\"total_samples\":";
  json += std::to_string(total);

  // Per-pipeline cache progress: read position (absolute) and unread backlog.
  // With per-pipeline caches there is no shared base sample; each cache frees
  // its consumed prefix independently, so "lag" is each pipeline's own backlog.
  const std::pair<const char*, PipelineAudioCache*> caches[] = {
      {"diar", diar_audio_.get()},
      {"asr", asr_audio_.get()},
      {"vad", vad_audio_.get()},
  };

  json += ",\"cursors\":[";
  bool first = true;
  for (const auto& c : caches) {
    if (!c.second) continue;
    if (!first) json += ",";
    json += "{\"id\":\"" + std::string(c.first) + "\",\"position\":";
    json += std::to_string(c.second->read_position());
    json += ",\"pending\":" + std::to_string(c.second->pending_samples()) + "}";
    first = false;
  }
  json += "]";

  // Add lag warnings if configured.
  if (config_.cursor_lag_warn_samples > 0 ||
      config_.cursor_lag_critical_samples > 0) {
    json += ",\"lags\":{";
    bool first_lag = true;
    for (const auto& c : caches) {
      if (!c.second) continue;
      const long lag = c.second->pending_samples();
      if (lag <= 0) continue;
      if (!first_lag) json += ",";
      json += "\"" + std::string(c.first) + "\":" + std::to_string(lag);
      first_lag = false;
      if (config_.cursor_lag_critical_samples > 0 &&
          lag >= static_cast<long>(config_.cursor_lag_critical_samples)) {
        json += ",\"status\":\"critical\"";
      } else if (config_.cursor_lag_warn_samples > 0 &&
                 lag >= static_cast<long>(config_.cursor_lag_warn_samples)) {
        json += ",\"status\":\"warn\"";
      }
    }
    json += "}";
  }

  json += "}";
  return json;
}

void AuditoryStream::PushAudio(const float* samples, int n) {
  if (n <= 0) return;
  double expected_start = 0.0;
  const auto now = std::chrono::system_clock::now();
  const double now_sec =
      std::chrono::duration<double>(now.time_since_epoch()).count();
  session_start_wall_sec_.compare_exchange_strong(expected_start, now_sec);
  // Fan the frame out to every active pipeline cache on the common clock, then
  // advance the shared clock head. Each cache holds the same audio at the same
  // absolute sample index, so the time base stays valid across pipelines.
  if (diar_audio_) diar_audio_->Append(samples, n);
  if (asr_audio_) asr_audio_->Append(samples, n);
  if (vad_audio_) vad_audio_->Append(samples, n);
  if (align_audio_) align_audio_->Append(samples, n);
  total_samples_.fetch_add(n, std::memory_order_relaxed);
}

void AuditoryStream::WaitForBarrier(long target) {
  std::unique_lock<std::mutex> lock(progress_mutex_);
  progress_cv_.wait(lock, [&] {
    if (!running_) return true;
    const bool diar_ok =
        !diar_worker_ || diar_worker_->processed_samples() >= target;
    const bool asr_ok =
        !asr_worker_ || asr_worker_->processed_samples() >= target;
    const bool vad_ok =
        !vad_detector_ || vad_processed_samples_.load() >= target;
    return diar_ok && asr_ok && vad_ok;
  });
}

void AuditoryStream::EmitLocked(const std::string& json) {
  std::lock_guard<std::mutex> lock(emit_mutex_);
  if (emit_) emit_(json);
}

void AuditoryStream::EmitTimeline(bool finalize) {
  if (finalize) {
    const bool was_running = running_;
    StopWorkers();
    if (was_running) {
      const auto exit_wall = std::chrono::system_clock::now();
      const double exit_sec =
          std::chrono::duration<double>(exit_wall.time_since_epoch()).count();
      const double entry_sec = session_start_wall_sec_.load();
      const double audio = audio_sec();
      bool wall_ok = true;
      if (audio > 0.0) {
        const double elapsed = exit_sec - entry_sec;
        // Spec 013's real-time gate is >=0.98x at 1.0x input pacing. This
        // compares physical elapsed time with the common audio clock without
        // imposing an unrelated fixed one-second budget on every duration.
        wall_ok = entry_sec > 0.0 && elapsed > 0.0 && audio / elapsed >= 0.98;
      }
      wall_clock_ok_.store(wall_ok);
      ReconcileFinalExtents();
    }
  } else {
    WaitForBarrier(total_samples_.load());
  }
  if (config_.timebase_check) {
    for (const auto& extent : track_extents()) {
      LOG_INFO(
          "[timebase] %s processed %ld samples vs common total %ld (gap %ld)\n",
          extent.pipeline.c_str(), extent.processed_samples,
          extent.common_total_samples, extent.gap_samples);
    }
  }
  const auto serialize_begin = std::chrono::steady_clock::now();
  std::string timeline = Serialize();
  const auto serialize_end = std::chrono::steady_clock::now();
  EmitLocked(timeline);
  const auto emit_end = std::chrono::steady_clock::now();
  LOG_INFO(
      "[finalize] serialize=%.3fms emit=%.3fms bytes=%zu\n",
      std::chrono::duration<double, std::milli>(serialize_end - serialize_begin)
          .count(),
      std::chrono::duration<double, std::milli>(emit_end - serialize_end)
          .count(),
      timeline.size());
}

// Serialize/SerializeRevision/SerializeGpuTelemetry in serialize.cc.

void AuditoryStream::Reset() {
  const bool was_running = running_;
  StopWorkers();
  if (was_running) ReconcileFinalExtents();
  if (session_store_ && session_store_->enabled()) {
    std::string timeline_json = Serialize();
    const auto now = std::chrono::system_clock::now();
    double wall_sec =
        std::chrono::duration<double>(now.time_since_epoch()).count();
    char session_id_buf[64];
    std::snprintf(session_id_buf, sizeof(session_id_buf), "%08x%08x",
                  static_cast<unsigned>(static_cast<long long>(wall_sec)),
                  static_cast<unsigned>(::getpid()));
    session_store_->Save(session_id_buf, timeline_json);
  }
  if (diar_audio_) diar_audio_->Reset();
  if (asr_audio_) asr_audio_->Reset();
  if (vad_audio_) vad_audio_->Reset();
  total_samples_.store(0);
  last_segments_.clear();
  last_transcript_.tokens.clear();
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    comp_.Clear();
  }
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  if (speaker_id_stage_) speaker_id_stage_->Reset();
  session_start_wall_sec_.store(0.0);
  wall_clock_ok_.store(true);
  timebase_reconciled_.store(false);
  timebase_ok_.store(true);
  StartWorkers();
}

}  // namespace pipeline
}  // namespace orator
