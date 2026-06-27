#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "io/bpe_tokenizer.h"

using namespace orator;

int main() {
  std::printf("Testing BPE tokenizer vs Qwen2 reference...\n");
  const std::string model = "models/asr/Qwen/Qwen3-ASR-1.7B";
  if (!std::ifstream(model + "/vocab.json").good()) {
    std::printf("[skip] tokenizer files not found\n");
    return 0;
  }

  io::BpeTokenizer tk;
  if (!tk.Load(model)) {
    std::printf("FAIL: load\n");
    return 1;
  }
  std::printf("  vocab=%d\n", tk.VocabSize());

  int fails = 0;
  auto check_encode = [&](const std::string& s, const std::vector<int>& exp) {
    auto got = tk.Encode(s);
    bool ok = got == exp;
    std::printf("  encode(%-18s) -> ", ("\"" + s + "\"").c_str());
    for (int t : got) std::printf("%d ", t);
    std::printf("%s\n", ok ? "OK" : "MISMATCH");
    if (!ok) ++fails;
  };
  // References from the HF tokenizer (oracle).
  check_encode("language Chinese", {11528, 8453});
  check_encode("language English", {11528, 6364});

  // Decode the dumped generation ids; must equal transcript.txt.
  std::ifstream gf("models/reference/asr/gen_ids.i32",
                   std::ios::binary | std::ios::ate);
  if (gf) {
    std::streamsize n = gf.tellg();
    gf.seekg(0);
    std::vector<int> ids(n / sizeof(int));
    gf.read(reinterpret_cast<char*>(ids.data()), n);
    std::string text = tk.Decode(ids, true);
    std::ifstream tf("models/reference/asr/transcript.txt");
    std::string ref((std::istreambuf_iterator<char>(tf)),
                    std::istreambuf_iterator<char>());
    std::printf("  decode(gen_ids) = \"%s\"\n", text.c_str());
    std::printf("  reference       = \"%s\"\n", ref.c_str());
    if (text != ref) {
      std::printf("  decode MISMATCH\n");
      ++fails;
    }
  }

  if (fails) {
    std::printf("FAIL: %d mismatches\n", fails);
    return 1;
  }
  std::printf("BPE tokenizer test PASSED\n");
  return 0;
}
