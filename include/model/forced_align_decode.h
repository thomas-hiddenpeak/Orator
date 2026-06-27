#pragma once

// Forced-alignment CPU post/pre-processing for the Qwen3 Forced Aligner.
//
// These are the torch-free pieces of the HF `Qwen3ASRProcessor`, ported exactly
// (validated against tools/reference/aligner_oracle.py, which calls the real
// transformers functions):
//
//   - SplitWordsForAlignment: the default CJK/space word tokenizer used by
//     `prepare_forced_aligner_inputs` (CJK chars individually, space-delimited
//     words otherwise, punctuation dropped; letters/numbers/apostrophes kept).
//   - FixTimestamps: the monotonic repair (`_fix_timestamps`) applied inside
//     `decode_forced_alignment` -- longest non-decreasing subsequence kept,
//     short anomaly blocks snapped to the nearer good neighbour, long blocks
//     linearly interpolated.
//   - PairWordTimestamps: pair the 2K repaired timestamps with K words
//     (word k: start = ms[2k], end = ms[2k+1]), in seconds.
//
// Pure C++20, no CUDA, no third-party deps. Unicode handling covers the
// languages the aligner supports (zh/en/yue/fr/de/it/ja/ko/pt/ru/es); it is a
// pragmatic classifier, not a full Unicode category table.

#include <string>
#include <vector>

#include "core/stages.h"

namespace orator {
namespace model {

using core::AlignUnit;

// Word-level tokens for forced alignment. `language` is the full name (e.g.
// "Chinese", "English") or empty; only ja/ko would use external morphology in
// the reference, which falls back to the default tokenizer here.
std::vector<std::string> SplitWordsForAlignment(const std::string& text,
                                                const std::string& language = "");

// Number of audio placeholder tokens for `mel_frames` log-mel frames, mirroring
// the reference `_get_audio_token_length` (CNN downsampling + 13 tokens per full
// n_window*2 chunk). Uses Python floor division semantics.
int AudioTokenLength(int mel_frames, int n_window = 50);

// Assemble the forced-aligner input id sequence:
//   audio_start, audio_pad x n_audio, audio_end,
//   then for each word: <its token ids> timestamp timestamp.
// Matches the model's chat template (words joined by two <timestamp> markers,
// with a trailing pair). `word_token_ids[k]` are the BPE ids of word k.
std::vector<int> BuildAlignerInputIds(
    const std::vector<std::vector<int>>& word_token_ids, int n_audio,
    int audio_start_id, int audio_pad_id, int audio_end_id, int timestamp_id);

// Monotonic repair of predicted timestamps (milliseconds). Mirrors
// transformers `_fix_timestamps` exactly (LIS + snap/interpolate), returning
// truncated-to-integer milliseconds.
std::vector<long> FixTimestamps(const std::vector<double>& raw_ms);

// Pair repaired timestamps (2 per word: start,end) with words; times in seconds.
std::vector<AlignUnit> PairWordTimestamps(const std::vector<std::string>& words,
                                          const std::vector<long>& fixed_ms);

}  // namespace model
}  // namespace orator
