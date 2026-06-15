#include "pipeline/asr_preprocessor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace orator {
namespace pipeline {

namespace {

std::string ShellQuote(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('\'');
  for (char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

bool WriteF32(const std::filesystem::path& p, const float* in, int n) {
  std::ofstream os(p, std::ios::binary);
  if (!os.good()) return false;
  os.write(reinterpret_cast<const char*>(in), static_cast<std::streamsize>(n) *
                                               static_cast<std::streamsize>(sizeof(float)));
  return os.good();
}

bool ReadF32(const std::filesystem::path& p, std::vector<float>* out) {
  if (out == nullptr) return false;
  std::ifstream is(p, std::ios::binary | std::ios::ate);
  if (!is.good()) return false;
  const std::streamsize bytes = is.tellg();
  if (bytes < 0 || (bytes % static_cast<std::streamsize>(sizeof(float))) != 0) {
    return false;
  }
  is.seekg(0, std::ios::beg);
  out->resize(static_cast<size_t>(bytes / static_cast<std::streamsize>(sizeof(float))));
  if (!out->empty()) {
    is.read(reinterpret_cast<char*>(out->data()), bytes);
  }
  return is.good() || is.eof();
}

std::filesystem::path ResolveToolPath(const char* rel) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path cwd = fs::current_path(ec);
  if (!ec) {
    const fs::path p1 = cwd / rel;
    if (fs::exists(p1, ec)) return p1;
    if (!ec) {
      const fs::path p2 = cwd.parent_path() / rel;
      if (fs::exists(p2, ec)) return p2;
    }
  }
  fs::path exe = fs::read_symlink("/proc/self/exe", ec);
  if (!ec) {
    const fs::path p3 = exe.parent_path().parent_path() / rel;
    if (fs::exists(p3, ec)) return p3;
  }
  return fs::path();
}

std::filesystem::path MakeTempPath(const char* suffix) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path base = fs::temp_directory_path(ec);
  const long long nonce = static_cast<long long>(std::rand());
  std::ostringstream oss;
  oss << "orator_preproc_" << std::to_string(static_cast<long long>(::getpid()))
      << "_" << nonce << suffix;
  return (ec ? fs::path("/tmp") : base) / oss.str();
}

}  // namespace

AsrPreprocessor::AsrPreprocessor(const Params& params)
    : params_(params), mode_(ParseMode(params.mode)) {}

AsrPreprocessor::Mode AsrPreprocessor::ParseMode(const std::string& mode) const {
  std::string m = mode;
  std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (m.empty() || m == "none" || m == "off") return Mode::kNone;
  if (m == "classical") return Mode::kClassical;
  if (m == "frcrn") return Mode::kFrcrn;
  if (m == "tfgridnet") return Mode::kTfGridNet;
  return Mode::kNone;
}

void AsrPreprocessor::Process(const float* samples, int n,
                              std::vector<float>* out) const {
  if (out == nullptr) return;
  out->clear();
  if (samples == nullptr || n <= 0) return;

  switch (mode_) {
    case Mode::kNone:
      out->assign(samples, samples + n);
      return;
    case Mode::kClassical:
      ClassicalEnhance(samples, n, out);
      return;
    case Mode::kFrcrn:
      if (RunFrcrnModelScope(samples, n, out)) return;
      WarnOnce("[asr_preproc] FRCRN backend unavailable, fallback to classical.\n");
      ClassicalEnhance(samples, n, out);
      return;
    case Mode::kTfGridNet:
      // Model inference backend will use converted safetensors in a follow-up
      // implementation. Keep the processing active today via the deterministic
      // classical path to preserve the VAD->preproc->ASR wiring.
      WarnOnce("[asr_preproc] TF-GridNet backend not wired yet, using classical.\n");
      ClassicalEnhance(samples, n, out);
      return;
  }
}

bool AsrPreprocessor::RunFrcrnModelScope(const float* in, int n,
                                         std::vector<float>* out) const {
  namespace fs = std::filesystem;
  const fs::path infer_py = ResolveToolPath("tools/asr_preproc_infer.py");
  const fs::path torchenv_sh = ResolveToolPath("tools/torchenv.sh");
  if (infer_py.empty() || torchenv_sh.empty()) return false;

  const fs::path in_f32 = MakeTempPath("_in.f32");
  const fs::path out_f32 = MakeTempPath("_out.f32");
  const fs::path err_log = MakeTempPath("_err.log");

  if (!WriteF32(in_f32, in, n)) return false;

  const std::string model = params_.frcrn_model_path.empty() ||
                                    params_.frcrn_model_path.find(".safetensors") != std::string::npos
                                ? "damo/speech_frcrn_ans_cirm_16k"
                                : params_.frcrn_model_path;
  std::ostringstream cmd;
  cmd << "bash -lc "
      << ShellQuote(std::string("source ") + torchenv_sh.string() +
                    " && python3 " + infer_py.string() + " --mode frcrn --in-f32 " +
                    in_f32.string() + " --out-f32 " + out_f32.string() + " --sample-rate " +
                    std::to_string(params_.sample_rate) + " --frcrn-model " + model + " 2>" +
                    err_log.string());

  const int rc = std::system(cmd.str().c_str());
  std::vector<float> enhanced;
  bool ok = (rc == 0) && ReadF32(out_f32, &enhanced);
  if (ok && static_cast<int>(enhanced.size()) == n) {
    out->swap(enhanced);
  } else {
    ok = false;
  }

  std::error_code ec;
  fs::remove(in_f32, ec);
  fs::remove(out_f32, ec);
  fs::remove(err_log, ec);
  return ok;
}

void AsrPreprocessor::WarnOnce(const char* msg) const {
  if (warned_) return;
  warned_ = true;
  std::fputs(msg, stderr);
}

void AsrPreprocessor::ClassicalEnhance(const float* in, int n,
                                       std::vector<float>* out) const {
  out->assign(in, in + n);
  if (n <= 1) return;

  // 1) Estimate a stable noise floor from lower quantiles.
  std::vector<float> mags;
  mags.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) mags.push_back(std::fabs((*out)[i]));
  const size_t q = mags.size() / 5;  // p20
  std::nth_element(mags.begin(), mags.begin() + q, mags.end());
  const float floor = std::max(1e-6f, mags[q]);
  const float shrink = floor * 1.5f;

  // 2) Soft-threshold denoise.
  for (int i = 0; i < n; ++i) {
    const float x = (*out)[i];
    const float mag = std::fabs(x);
    const float y = (mag > shrink) ? (mag - shrink) : 0.0f;
    (*out)[i] = (x >= 0.0f ? y : -y);
  }

  // 3) Light de-reverb emphasis.
  const float beta = 0.65f;
  float prev = (*out)[0];
  for (int i = 1; i < n; ++i) {
    const float cur = (*out)[i];
    (*out)[i] = cur - beta * prev;
    prev = cur;
  }

  // 4) Preserve peak level to avoid excessive attenuation.
  float in_peak = 1e-6f;
  float out_peak = 1e-6f;
  for (int i = 0; i < n; ++i) {
    in_peak = std::max(in_peak, std::fabs(in[i]));
    out_peak = std::max(out_peak, std::fabs((*out)[i]));
  }
  const float gain = std::min(2.0f, in_peak / out_peak);
  for (int i = 0; i < n; ++i) (*out)[i] *= gain;
}

}  // namespace pipeline
}  // namespace orator
