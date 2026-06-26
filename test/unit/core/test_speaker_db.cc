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

  std::cout << "\nAll speaker database tests passed!" << std::endl;
  return 0;
}
