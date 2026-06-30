#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>

#include "core/stages.h"
#include "model/speaker_database.h"

using namespace orator;

static std::vector<float> OneHot(int dim, int idx) {
  std::vector<float> v(static_cast<size_t>(dim), 0.0f);
  v[idx] = 1.0f;
  return v;
}

int main() {
  std::cout << "Testing speaker database..." << std::endl;

  const int dim = 256;
  model::SpeakerDatabase db(2000, dim);

  // Use the interface type to verify decoupling.
  core::ISpeakerRegistry& reg = db;

  auto alice = OneHot(dim, 0);
  auto bob = OneHot(dim, 1);
  assert(reg.Enroll("alice", alice.data()));
  assert(reg.Enroll("bob", bob.data()));
  assert(reg.Size() == 2);
  std::cout << "Enrolled 2 speakers via interface" << std::endl;

  // Duplicate enroll rejected.
  assert(!reg.Enroll("alice", alice.data()));

  float score = 0.0f;
  int idx = reg.Match(alice.data(), 0.5f, &score);
  assert(idx == 0);
  assert(reg.SpeakerIdAt(idx) == "alice");
  assert(score > 0.99f);
  std::cout << "Exact match works (score=" << score << ")" << std::endl;

  // Below-threshold query returns -1.
  auto noise = OneHot(dim, 100);
  int no_match = reg.Match(noise.data(), 0.5f, &score);
  assert(no_match == -1);
  std::cout << "Threshold rejection works" << std::endl;

  // Save and load round-trip.
  const char* path = "/tmp/orator_speaker_db.bin";
  assert(db.Save(path));
  model::SpeakerDatabase db2(2000, dim);
  assert(db2.Load(path));
  assert(db2.Size() == 2);
  int idx2 = db2.Match(bob.data(), 0.5f, &score);
  assert(db2.SpeakerIdAt(idx2) == "bob");
  std::remove(path);
  std::cout << "Save/load round-trip works" << std::endl;

  // Display-name hook + sidecar persistence (Spec 010 R6).
  db.SetDisplayName("alice", "Alice Smith");
  assert(db.DisplayName("alice") == "Alice Smith");
  assert(db.DisplayName("bob").empty());
  const std::string npath = "/tmp/orator_speaker_db_named.bin";
  assert(db.Save(npath));
  model::SpeakerDatabase db3(2000, dim);
  assert(db3.Load(npath));
  assert(db3.DisplayName("alice") == "Alice Smith");
  assert(db3.DisplayName("bob").empty());
  std::remove(npath.c_str());
  std::remove((npath + ".names").c_str());
  std::cout << "Display-name hook + sidecar round-trip works" << std::endl;

  // Remove (registry de-duplication): deleting one speaker keeps the others
  // matchable and frees the id; the dense buffer stays correct.
  {
    model::SpeakerDatabase rdb(2000, dim);
    auto a = OneHot(dim, 0), b = OneHot(dim, 1), c = OneHot(dim, 2);
    rdb.Enroll("spk_0", a.data());
    rdb.Enroll("spk_1", b.data());
    rdb.Enroll("spk_2", c.data());
    assert(rdb.Size() == 3);
    assert(rdb.Remove("spk_1"));        // remove a middle entry
    assert(rdb.Size() == 2);
    assert(!rdb.Contains("spk_1"));
    assert(rdb.Remove("spk_1") == false);  // already gone
    // The survivors still match correctly after the swap-with-last compaction.
    float s = 0.0f;
    int i0 = rdb.Match(a.data(), 0.5f, &s);
    assert(i0 >= 0 && rdb.SpeakerIdAt(i0) == "spk_0");
    int i2 = rdb.Match(c.data(), 0.5f, &s);
    assert(i2 >= 0 && rdb.SpeakerIdAt(i2) == "spk_2");
    // The removed speaker no longer matches its embedding.
    assert(rdb.Match(b.data(), 0.5f, &s) == -1);
    std::cout << "Remove (dedup) works" << std::endl;
  }

  std::cout << "\nAll speaker database tests passed!" << std::endl;
  return 0;
}
