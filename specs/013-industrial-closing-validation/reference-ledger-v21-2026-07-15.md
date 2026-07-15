# v2.1 Reference Ledger Progress - 2026-07-15

## Status

Reference-ledger adjudication is in progress for the pushed v2.1 closing
baseline commit `43523bad75d39134f44adb0851d1062af95508f4`. The immutable
556-row ledger is initialized and structurally validated. No audible boundary,
criticality, ambiguity, or reviewer signature has been filled yet, so this
artifact is not an accuracy denominator and cannot support a closing claim.

## Frozen Inputs

| Item | SHA-256 |
|---|---|
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| initialized ledger | `66cd37642365d70e3d3c6f129283aa071df0cb395be549951da8e1aba73ebf60` |

The ledger contains 556 stable reference IDs over 3615.12 seconds. Source-row
counts are Tang Yunfeng 188, Xu Zijing 73, Zhu Jie 83, and Shi Yi 212.

## Source Timestamp Audit

Mechanical inspection, without assigning any correctness judgment, found:

- 22 duplicate-timestamp groups covering 46 rows;
- 25 rows whose next-source-timestamp provisional interval is zero or negative;
- one explicit backward timestamp pair: `ref-0446` is stamped 2933 seconds and
  `ref-0447` is stamped 2932 seconds.

These rows remain in the denominator. Their exact audible intervals and overlap
relationships must be decided by listening in conversational context; scripts
must not repair or reorder them.

## Review Batches

The complete session is prepared as the six constitutional 600-second blocks
plus the final 15.12-second partial block. Each directory under
`/tmp/orator-spec013/closing-v21-43523ba/` contains continuous mono 16 kHz WAV,
the matching immutable reference rows as JSON/TSV, and an `unreviewed` manifest.

| Block | Rows | Continuous WAV SHA-256 |
|---|---:|---|
| 0-600 s | 93 | `aa1a7df54459e6f1e5961090b48cf9b63a30cf004f337677999bfcd74309487f` |
| 600-1200 s | 84 | `a4c6b2efee9c2b7e0ce657e114be8601779a4939c3ce65075e9211e4f24643e1` |
| 1200-1800 s | 80 | `61694afe5f5669ec0b7931ccb7ea1d773adc5ea8608dbd674cb0061f84830197` |
| 1800-2400 s | 80 | `a97666109b1a1e589aa2ceec2b04482725d1e5339aecf3bbb095d0d6a9d8803d` |
| 2400-3000 s | 129 | `8cac020ca0ce2df06cae096de05d5072bb11feebc0feacc75e115960a8063dda` |
| 3000-3600 s | 87 | `8d8e9f4f8ac88d5ac4017933b4aaf357c028828e8f4e57afe3cf6eaa5e58107c` |
| 3600-3615.12 s | 3 | `8e90066710142a95c35cc7e5e011334256ea3071f57b24586a8acacf13ae8953` |

The batch hash index is
`8789a54fead5159492e7bd83df2078e74cdca12e376f1222385f680e13981453`.
The 16-second `ref-0446/ref-0447` backward-timestamp context clip is stored in
the 2400-3000-second block at SHA-256
`d1c7dd26773739c385d27a4f4b3ee636a5038e74a08211b8b539e27793b23018`.

All seven batches remain `unreviewed`. VAD, forced alignment, and existing
v2.1 timeline data may be displayed as evidence, but none may assign an audible
boundary, overlap, criticality, ambiguity, or signature.
