# Session-Wide Primary Evidence Capture (2026-07-19)

## Scope and governance

This FR46 artifact capture displays source-independent TitaNet evidence for
every frozen T123 primary-speaker run. It does not compare the evidence with
`test.txt`, assign speaker correctness, aggregate accuracy, rank a topology,
select a candidate, or issue a verdict. The human reference is not read while
constructing or replaying the query set.

Automation is limited to a mechanical primary-track transformation, exact-PCM
identity replay, raw evidence serialization, row-count validation, and hashing.
All product interpretation remains reserved for complete forward and reverse
contextual semantic review.

## Query contract

The frozen T123 primary track contains 1,348 positive-duration rows. The
temporary query TSV preserves every row in original order and copies only its
exact `start_sec` and `end_sec`. Each query ID is the zero-based row ordinal
`primary_run:N`; `kind` is `primary_run`; the source fields are fixed
non-product placeholders required by the existing diagnostic probe. The query
contains no primary identity, local slot, reference row, expected speaker,
correctness field, score threshold, or selection label.

The output contains one evidence row for every input query. Spans below the
existing embedding floor remain present with
`embedding_available=0`, `session_gallery_complete=0`, and
`robust_gallery_complete=0`; for example, `primary_run:17` preserves the exact
`81.439998180-81.599998176` interval. No result-dependent filtering occurs.

## Capture-faithful replay

Both independent runs use:

- exact streamed PCM wrapped losslessly as WAV;
- the original 3,575 chronological T123 diar snapshots;
- a genuinely empty `SpeakerDatabase`;
- the frozen T123 TOML with `speaker.retain_sec=4000.0`;
- the same TitaNet weights as the captured production run.

Each run processes all 3,575 snapshots, enrolls four identities, and displays
all 1,254,049 captured identity strings equal to the corresponding replayed
identity strings, with zero differing values. This establishes producer/input
reproduction only.

## Immutable artifacts

| Artifact | SHA-256 |
|---|---|
| Primary query TSV, 1,348 rows | `0747c0ca48c61d26450d34978873688ec5c605a43a58dee3e1482201c14e5169` |
| Repeated final identity TSV | `b1f42d0085adcafaf6564479bbc88895518adfa80d84891cd8be3dde843467fa` |
| Repeated 1,348-row primary evidence TSV | `6c7b4c9b1e17a08895a3c4a88eab855acf8e42fb530fef90c093ce7f6b88d366` |
| Exact streamed PCM s16le | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Lossless PCM16 WAV wrapper | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Chronological diar snapshot TSV | `1c39108da56011e921056fa5876532181c738aa3d7a4ad0dc8960a859f60c08a` |
| Frozen T123 TOML | `6a92c582a2cba7e26542f38a60516bb53929dc5d109ba331bbf4b5b614eb9b22` |
| TitaNet weights | `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |

The repeated evidence files are byte-identical. Every eligible row explicitly
contains session/robust completeness and both raw score lists; every ineligible
row remains explicit. These facts authorize the manual T189 evidence review,
not a production policy or ledger change.
