# FR38 Cross-VAD Phrase-Tail Reconstruction Diagnosis (2026-07-19)

## Scope and authority

This frozen-evidence diagnosis addresses the `ref-0504` attribution split in
the retained T123-based candidate. It does not score ASR wording and does not
run a new audio path. `test/data/reference/test.txt` remains the authoritative
human-listened reference. Complete conversational reading identifies Tang
Yunfeng's ownership correction before the following Hangzhou-stake statement.

No program, script, query, formula, metric, or interval operation assigns
correctness, aggregates accuracy, selects FR38, or issues a product verdict.
Shell tools only display frozen typed evidence on the common source clock. A
complete chronological and reverse contextual review of every changed
conversation decides whether the resulting candidate is retained.

## Corrected problem boundary

The disabled FR16ABO future-epoch rule is not the current repair boundary. It
changed the T111 `3299.900-3300.620` phrase from Zhu Jie to Tang Yunfeng, while
the retained T123 package already assigns its corresponding leading interval
to Tang. Re-enabling the 120-second lookahead would therefore add no useful
T123 correction and would restore a previously rejected broad experiment.

T123 instead partitions one leading punctuation phrase across two VAD regions:

- embedding-backed `business_interval:280:0` covers source `[0,5)` at
  `3299.724-3300.364`; its native activity/primary identity is A, while both
  exact interval galleries select B and ordinary short direct evidence writes
  B;
- `punctuation_phrase:280:0` covers source `[0,7)` at
  `3299.724-3301.244`; its final visible character is a single aligned unit in
  a second VAD, followed only by configured punctuation;
- no-embedding `business_interval:280:1` covers that tail `[5,7)` at
  `3301.164-3301.244` and inherits native identity C; and
- the next embedding-backed interval starts at source `7`, remains C in
  activity and primary, and occupies the same second VAD. It is a separate
  clause and must not be repainted.

A, B, and C are pairwise different. The leading interval galleries rank B
first and A second under the existing short gates. The complete punctuation
phrase ranks A/B in the session view and B/A in the robust view; both fail the
unchanged regular score gate and exactly one passes the unchanged regular
margin. The first VAD ranks B/A in both galleries with exactly one short margin
pass. The second VAD ranks C/B in both galleries and passes both short gates;
it starts within the configured alignment tolerance of the one-character tail
and extends into the following C interval. This is a source-partition boundary
defect: the second VAD's following-clause evidence owns the isolated phrase
tail before final projection.

## FR38 contract

FR38 reconstructs only the no-embedding tail of one source-leading punctuation
phrase. Every condition is conjunctive:

1. the punctuation phrase is embedding-backed, robust-complete, starts at
   source zero, lies within the existing regular phrase bounds, and contains
   exactly one leading embedded interval plus one source-contiguous
   no-embedding tail interval;
2. the tail contains exactly one positive-duration aligned visible character,
   with only configured punctuation after it, and remains below the existing
   primary-consensus minimum;
3. a separately embedded following interval starts exactly at the phrase
   source end, both of its galleries rank C first, and the leading, tail, and
   following intervals are ordered on the common clock without overlap;
4. the leading labels are uniformly ordinary `voiceprint_direct_short` B over
   uniform native A, while the tail and following labels are unprotected
   `sole_diar_support` C; A, B, and C are pairwise distinct;
5. activity and primary each expose one completely covering A local slot over
   the leading interval and one completely covering C local slot over the tail
   and following interval, with no competing overlap in any component;
6. both leading interval galleries rank B first and A second and pass the
   unchanged short score and margin gates;
7. the phrase session gallery ranks A then B, the robust gallery ranks B then
   A, both fail the unchanged regular score gate, and exactly one passes the
   unchanged regular margin;
8. exactly one VAD contains the leading interval and ranks B then A in both
   galleries under the unchanged short score gate with exactly one margin
   pass; and
9. exactly one different VAD contains the tail, starts within the existing
   alignment boundary tolerance, extends into the following interval, and
   ranks C then B in both galleries while passing both unchanged short gates.

The rule writes only the exact tail source/alignment range. It adds no TOML
field, score, margin, duration, future lookahead, identity, transcript,
speaker name, timestamp, reference lookup, or fitted constant. Missing,
duplicate, differently ranked, differently gated, mixed, protected,
unaligned, non-leading, non-punctuation, competing-activity, or source/time-
inconsistent evidence preserves ordinary behavior.

## Verification and decision boundary

Focused tests must cover the complete positive topology and independent
abstention for source shape, alignment, provenance, identity distinctness,
native activity/primary coverage, each interval and phrase rank/gate, both VAD
relationships, uniqueness, and following-clause preservation. After a
warning-clean build and complete CTest pass, the production C++ projector
replays the frozen T123 and T111 producer packages at least twice.

Automation may verify immutable input hashes, deterministic output hashes,
source reconstruction, monotonic time, and raw change scope only. Every changed
complete conversation is then read chronologically and in reverse against
`test.txt`. That contextual semantic review alone retains or removes FR38 and
updates any manually derived ledger. A retained frozen replay is not a new
real-WebSocket result and cannot close the speaker business by itself.
