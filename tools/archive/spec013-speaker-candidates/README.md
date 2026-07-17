# Archived Spec 013 Speaker Candidates

This directory preserves one-off Python candidate generators from the v2.1
speaker-business investigation. They are historical source records only:

- CMake and CTest do not register them.
- They are not part of the runtime, release, or production validation surface.
- Their original paths, companion tests, and TOML inputs may no longer resolve.
- They must not be used to judge correctness, calculate accuracy, rank/select a
  candidate, tune a parameter, or issue an acceptance verdict.

The accepted runtime behavior lives in `SpeakerFusionPolicy` and is protected by
`test_business_speaker_pipeline` plus full frozen-output equivalence checks.
New speaker work must start from the production C++ path and the checked-in root
`orator.toml`, not from these archived experiments.
