# FR50 Full-Residual Evidence Capture (2026-07-23)

## Scope and governance

This record completes T223-T225 only. It arranges immutable FR49 evidence for
complete contextual human review. No code or command in this stage assigns a
speaker-business result, groups a cause, counts an accuracy class, ranks a
repair, selects a candidate, or issues a verdict. T226 and T227 remain the
first phases permitted to interpret this evidence against the complete
conversation in `test.txt`.

The manually frozen residual scope is:

- all 33 residual references: `ref-0049`, `ref-0058`, `ref-0063`,
  `ref-0066`, `ref-0099`, `ref-0102`, `ref-0118`, `ref-0135`, `ref-0171`,
  `ref-0221`, `ref-0239`, `ref-0241`, `ref-0252`, `ref-0298`, `ref-0313`,
  `ref-0327`, `ref-0331`, `ref-0333`, `ref-0341`, `ref-0354`, `ref-0390`,
  `ref-0409`, `ref-0417`, `ref-0426`, `ref-0442`, `ref-0444`, `ref-0457`,
  `ref-0461`, `ref-0499`, `ref-0503`, `ref-0505`, `ref-0506`, and
  `ref-0537`;
- the separately signed 20-critical subset: `ref-0049`, `ref-0058`,
  `ref-0066`, `ref-0099`, `ref-0102`, `ref-0118`, `ref-0252`, `ref-0313`,
  `ref-0327`, `ref-0331`, `ref-0333`, `ref-0354`, `ref-0390`, `ref-0426`,
  `ref-0442`, `ref-0444`, `ref-0461`, `ref-0499`, `ref-0503`, and
  `ref-0505`.

This transcription freezes scope only. It does not complete T032 or establish
a product total.

## Immutable sources

| Source | SHA-256 |
|---|---|
| FR49 full A | `64abe31baf51185b685c91a58529096b25d281540afe21c3bbc2354cffb5432e` |
| FR49 full B | `0ac66dbfc7dd95d21fcb271ad3b3a020d79c565b4c62ccc0a97f5e9a14f63813` |
| Full A manifest | `12fda94f6408a081bf3ff5f5158d0e11d0d3c9c2ab70294ab8d24aeeec9bd0e4` |
| Full B manifest | `6574a7e87ff53ac055ecbe60b5cd52a3e6a5145a9f1e9e8ddd62af2cf1ceef03` |
| Exact streamed PCM | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Lossless PCM16 WAV wrapper | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Checked-in `orator.toml` | `b5ecc7d84e8b711b48cad3a7b4a90f4820cf28e1d1c9f4e2ee3b301d00ebf210` |
| Sortformer v2.1 weights | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| FR50 context table | `e9f9d3bae46cb509fed303d1ebfcf22153cde6d33e1a2d120565403cdfbfe356` |
| Worksheet tool | `b284cc97b2d0d6592fe105d80cd2949f01c5c5002153fa96d40d5bf2350ded7f` |
| Focused test | `f635d714be5c4f756f9091883f0cb00c689146488326b091fc360c78d2fb5fa5` |

Both FR49 artifacts identify clean implementation commit `1f09052`, resolved
configuration `2c7b4a71818aabe54b7a4be15b0c0bc3e22ebaa8e0d0935eae4bad7fb6590eba`,
57,841,920 samples, 16 kHz, and the same exact PCM hash.

## Current-config posterior provenance

The historical T191 TOML hash
`6a92c582a2cba7e26542f38a60516bb53929dc5d109ba331bbf4b5b614eb9b22`
does not equal the current checked-in TOML hash. FR50 therefore does not merely
assume reuse. `diar_evidence_probe` was rebuilt and run twice against the exact
lossless WAV with `ORATOR_CONFIG=orator.toml`. The probe binary hash is
`ca3f187de6d4c5d922edcbf4fdb53f4d3adf9df764d413d98189ada17eca5df0`.

Both executions load v2.1 with `188/340/188/1/1/3/188`, one continuous
session, and no reset. Each emits 45,189 four-channel frames from `0.000000`
through `3615.039919 s` and 755 onset/offset segments. Compute times are
`37.2949 s` and `37.1476 s`; these are component timings, not streaming
performance results.

| Repeated raw artifact | Run A SHA-256 | Run B SHA-256 |
|---|---|---|
| Four-channel frames | `79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41` | `79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41` |
| Onset/offset segments | `94a2a758ef9771e1646d27eb56a6257421ae620870e3bf467eb9fed9976264c0` | `94a2a758ef9771e1646d27eb56a6257421ae620870e3bf467eb9fed9976264c0` |

The repeated files are byte-identical. They also happen to reproduce T191
byte-for-byte, but FR50 provenance rests on the current-config reruns above.

The worksheet tool applies only the resolved existing
`speaker_fusion.frame_activity_threshold=0.5` producer contract. Because the
CSV has six-decimal frame times, it permits `2e-6 s` only when reconstructing
adjacent frame continuity. It then compares all produced local slots, order,
bounds, and mean probabilities to each FR49 `primary_speaker` entry with a
`1e-6` field tolerance. Full A and B independently reproduce all 1,348 runs.
This establishes producer identity only, not speaker correctness.

## Worksheet trees

The retained roots are:

- `/tmp/orator-spec013/release-36fdd5b-fr50-residual-audit/run-full-a-worksheets`;
- `/tmp/orator-spec013/release-36fdd5b-fr50-residual-audit/run-full-b-worksheets`.

Each root contains 33 context directories. Every directory contains the
manually authored context row, complete intersecting `test.txt` sections,
unmodified final comprehensive/business entries and decision audit, all seven
typed tracks, all intersecting or source-related TitaNet evidence, observed
local identity epochs, and exact intersecting four-channel posterior rows.
Each tree has 201 files and no empty file. `sha256sum -c content.sha256`
passes for every listed file.

| Tree | Content-manifest SHA-256 | Packet-manifest SHA-256 |
|---|---|---|
| Full A | `437398654dbe01bfa6fc1a142da87ca2ff2433a8a08c83c71b02ab8984221417` | `7b76b19f0e63d746f2f84952387a89c219e54a48145cdb91ad03a18fa4a65107` |
| Full B | `88be67f837fc4e2e59964dfd998f0a2800144480f1a1496b731f0bb709091632` | `6a3e8fc4e7dc81cab17643196acf257e7d7c354ac1f016dadce9aff5f202ba07` |

Independent repeated exports reproduce each tree's content-manifest hash
exactly. A and B local-identity epoch files are byte-identical. These are file,
ordering, and producer contracts only.

The focused five-test module passes directly and through CTest. The complete
build finishes without a compiler warning, and all `72/72` CTest entries pass
in `53.10 s`, including the real WebSocket contract test. These establish
engineering consistency only.

T223, T224, and T225 are complete. T226 must now read all A contexts in
chronological order and all B contexts independently in chronological order.
T227 must repeat both complete reviews in reverse. Until those four readings
are reconciled manually, FR50 has no causal finding and authorizes no product
change, TOML change, new audio run, ledger change, or closure claim.
