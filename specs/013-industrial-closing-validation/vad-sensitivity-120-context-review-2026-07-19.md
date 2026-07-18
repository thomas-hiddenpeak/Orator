# FR30 120-Second Context Review (2026-07-19)

## Scope and authority

This report records the T131 promotion gate for the single-variable FR30
candidate at commit `5046bccf7ea239902747b5bdf85b420085b646b7`.
`test/data/reference/test.txt` is the authoritative human-listened transcript.
No code, script, query, formula, metric, or model score assigned correctness,
aggregated accuracy, ranked the candidate, or issued the decision below.
Automation captured the real-WebSocket sessions, checked mechanical contracts,
hashed immutable artifacts, and arranged source and candidate text for reading.

T131 is a bounded regression gate. Passing it permits one 600-second capture;
it does not accept FR30, replace T111, or establish full-session accuracy.

## Real-stream evidence

All five sessions used the production `orator_ws` path at 1x, direct `end`,
Sortformer v2.1, the checked-in behavioral TOML with `vad.threshold = 0.3`, an
isolated empty speaker registry, telemetry, an independent observer, and an
unchanged clean source tree. Each manifest binds the client, server binary,
resolved configuration, source audio, and terminal artifact to commit
`5046bccf7ea2`.

| Session | Artifact ID | Artifact SHA-256 | Direct-end wait |
|---|---|---|---:|
| Silence 1 | `orator-20260718T183553Z-5046bccf7ea2-30.000s` | `8a66f846220b67df5595d929a48b4a23ccd2ebb2c1d0b9806e06333f5f1cf8b8` | `0.263 s` |
| Silence 2 | `orator-20260718T183712Z-5046bccf7ea2-30.000s` | `26d5e676c274f59051c116de172ddbfe79d048d5dd9788158662ed7be0e0c60e` | `0.268 s` |
| Silence 3 | `orator-20260718T183805Z-5046bccf7ea2-30.000s` | `607bebf45d5c2cf3e817014726a4d7074fcd4571f17061368c159953fc5fa7d9` | `0.262 s` |
| 120 s Run A | `orator-20260718T184036Z-5046bccf7ea2-120.000s` | `6424f126a38d0086546fd3b72bc1233a5a2725e8249dd4d72fc4d84c1d0d4e67` | `1.214 s` |
| 120 s Run B | `orator-20260718T184309Z-5046bccf7ea2-120.000s` | `0f3428b930b358c55d1a664f3d3a47875f01ad300d928c746ad5a53d9c420962` | `1.209 s` |

The three silence sessions each contain zero diarization, primary-speaker,
ASR, VAD, forced-alignment, voiceprint, business-speaker, and comprehensive
records. All five artifacts have empty mechanical issue lists, exact common
track extents, reconciled time bases, valid wall clocks, matching observer
terminal hashes, and complete required telemetry coverage.

Run A and Run B each contain 23 diarization, 27 primary-speaker, 10 ASR, 35
VAD, 10 alignment, zero voiceprint, 33 business-speaker, and 33 comprehensive
records. Their normalized seven-track entry bundles are exactly equal at
SHA-256
`b126460042468940ed83c6294a8eafb844151ce1db965194d9378195ca90a373`.
This equality is repeatability evidence only; it does not judge speaker
correctness.

## Complete contextual review

Run A and Run B were each read independently against all 18 in-scope
`test.txt` contributions, first from `ref-0001` through `ref-0018`, then in
reverse 30-second blocks from `90-120 s` back to `0-30 s`. The review followed
the complete question, response, interruption, and continuation context rather
than treating timestamp overlap or short label fragments as automatic errors.

Both reads preserve the same natural-turn behavior as the accepted T125
120-second gate. Existing cold-start unknown evidence at the beginning of
`ref-0001` and the known short interjection and handoff fragments around
`ref-0005`, `ref-0008`, `ref-0009`, and `ref-0017` remain visible; T131 does
not claim to repair them.

The assignment-only display against T125 contains one changed context,
`ref-0008`. Both versions place the substantive reply on the same local
speaker. FR30 removes an intervening `0.080 s` local-label fragment and changes
the text partition, but it does not introduce a new speaker turn, change the
question/answer ownership, or create a new contextual regression. This
judgment was made by reading the surrounding `ref-0003` through `ref-0011`
exchange in both directions; the display tool did not assign it.

## Decision boundary

T131 is retained. FR30 may proceed to exactly one clean 600-second production
real-WebSocket capture, followed by complete chronological and reverse
contextual review of all 93 in-scope contributions. FR30 remains transitional,
T111 remains the accepted full-session baseline, and no full A/B capture is
authorized until T132 is manually retained.
