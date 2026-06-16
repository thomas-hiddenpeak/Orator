#pragma once

// TimeBase (Spec 005): the project's reusable common time base.
//
// A lightweight, copyable value type that maps absolute sample positions to
// seconds on ONE shared clock. It is the single conversion used by every stream
// consumer so that all pipelines report times that align by construction
// (rather than by the coincidence of each counting from 0 independently).
//
// Model:
//   - One common clock per session. `origin_sample` is the absolute sample that
//     corresponds to t = 0 (the start of the stream; 0 today).
//   - A consumer that counts its own samples from 0 but begins at absolute
//     sample S derives a child base anchored at S: `base.Derive(S)`, then reports
//     `LocalSeconds(i)` for its local index i. This serves consumers that produce
//     data only intermittently (data sometimes present, sometimes not): when data
//     appears at absolute sample S, anchor there and map local counts onto the
//     common clock.
//
// It carries no data buffers and owns no threads; it is safe to copy and pass by
// value. Instantiate one per session (e.g. from SharedAudioBuffer::time_base())
// and derive sub-stream bases as needed.

#include <cmath>

namespace orator {
namespace core {

class TimeBase {
 public:
  // Default-constructed base is INVALID (sample_rate 0) -- a guard for "not set".
  TimeBase() = default;

  explicit TimeBase(int sample_rate, long origin_sample = 0)
      : sample_rate_(sample_rate), origin_sample_(origin_sample) {}

  bool valid() const { return sample_rate_ > 0; }
  int sample_rate() const { return sample_rate_; }
  long origin_sample() const { return origin_sample_; }
  long anchor_sample() const { return anchor_sample_; }

  // Absolute sample position on the common clock -> seconds.
  double SecondsAt(long abs_sample) const {
    return sample_rate_ > 0
               ? static_cast<double>(abs_sample - origin_sample_) / sample_rate_
               : 0.0;
  }

  // Seconds on the common clock -> nearest absolute sample.
  long SampleAt(double seconds) const {
    return origin_sample_ +
           static_cast<long>(std::llround(seconds * sample_rate_));
  }

  // Duration of `n_samples` samples in seconds.
  double Duration(long n_samples) const {
    return sample_rate_ > 0 ? static_cast<double>(n_samples) / sample_rate_ : 0.0;
  }

  // Derive a sub-stream base on the SAME clock (same rate + origin), anchored at
  // `anchor_abs_sample` -- where this sub-stream's local data begins. The child
  // maps a local sample index (counted from the anchor) onto the common clock.
  TimeBase Derive(long anchor_abs_sample) const {
    TimeBase t(sample_rate_, origin_sample_);
    t.anchor_sample_ = anchor_abs_sample;
    return t;
  }

  // For a derived base: local sample index (from the anchor) -> common seconds.
  //   LocalSeconds(i) == SecondsAt(anchor_sample() + i)
  double LocalSeconds(long local_sample) const {
    return SecondsAt(anchor_sample_ + local_sample);
  }

  // Reconcile a consumer's processed sample extent against the common clock's
  // total. Returns the signed gap in samples (processed - common_total); 0 means
  // the consumer is exactly aligned to the common clock end-point.
  static long ReconcileExtent(long processed_samples, long common_total) {
    return processed_samples - common_total;
  }

 private:
  int sample_rate_ = 0;     // 0 => invalid
  long origin_sample_ = 0;  // absolute sample of t = 0 on the common clock
  long anchor_sample_ = 0;  // where a derived sub-stream's local data begins
};

}  // namespace core
}  // namespace orator
