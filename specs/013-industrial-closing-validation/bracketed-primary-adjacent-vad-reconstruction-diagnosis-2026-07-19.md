# FR37 Bracketed-Primary Adjacent/VAD Reconstruction Diagnosis (2026-07-19)

## Scope and authority

This frozen-evidence diagnosis addresses the T111/T123 speaker-attribution
partition regression associated with `ref-0478`. It does not score ASR text or
run a new audio path. `test/data/reference/test.txt` remains the authoritative
human-listened reference. Complete conversational reading identifies Zhu Jie
as the speaker of `我向国家交` between Shi Yi's preceding question and Shi's
following clarification.

No program, script, query, formula, metric, or interval operation assigns
correctness, aggregates accuracy, selects FR37, or issues a product verdict.
Shell tools only display frozen typed evidence on the common source clock. A
complete chronological and reverse contextual review of every changed
conversation decides whether the resulting candidate is retained.

## Frozen partition evidence

The activity, primary-speaker, VAD, and identity tracks are identical in T111
and T123. T111 preserves `我向国家交` as a short punctuation phrase and the
existing initial-slot/VAD challenge assigns it to Zhu Jie. T123 removes the
punctuation phrase boundary and exposes only a `3075.096-3075.496` regular
`business_interval`; ordinary direct evidence assigns that interval to Tang
Yunfeng. The spoken response and common-clock region remain present.

The T123 interval exposes three independent evidence relationships:

- its base label and exact interval galleries select current identity A, Tang
  Yunfeng, in one primary local slot;
- that A primary run is a short island with gapless preceding and following
  primary runs carrying the same third identity C, Shi Yi; activity A and C
  both cover the interval, and A's activity slot has initial identity B, Zhu
  Jie; and
- the immediately preceding source-adjacent punctuation phrase and the unique
  containing VAD both rank B first in their session and robust galleries. Both
  adjacent-phrase views pass the unchanged short score gate, exactly one passes
  the unchanged short margin, and both VAD views pass their unchanged regular
  score and margin gates.

The target interval itself ranks A first in both short views and passes their
unchanged score and margin gates. Its session runner-up is C and its robust
runner-up is B. No single
conflicting view is sufficient to repaint the interval; the value lies in the
complete boundary, slot-history, primary-bracket, activity, phrase, and VAD
topology. The defect is therefore a partition-dependent loss of an existing
challenge object, not a changed model output, time base, or TOML value.

## FR37 contract

FR37 adds one short `business_interval` reconstruction with current identity A,
initial identity B, and primary/activity competitor C. Every condition is
conjunctive:

1. the interval is embedding-backed, robust-gallery complete, within the
   existing primary-consensus and short duration bounds, and contains the
   existing minimum aligned-unit count;
2. every base source label is unprotected primary-speaker provenance and
   uniformly names A;
3. exactly one primary run in A's local slot contains the interval; its
   duration remains below the existing short bound, and its immediately
   preceding and following primary runs are gapless, each meets the existing
   primary-consensus minimum, and both name the same C distinct from A;
4. activity exposes exactly the A local slot and one C local slot over the
   interval, both cover it completely, and no other activity overlaps;
5. A's activity slot has the unique non-empty initial identity B, distinct from
   A and C, and B is not a current activity identity over the interval;
6. both exact interval galleries rank A first and pass the unchanged short
   score and margin gates; the session view ranks C second and the robust view
   ranks B second;
7. exactly one embedding-backed, robust-complete punctuation phrase for the
   same text ends at the interval's source start and common-clock start within
   the existing alignment boundary tolerance. Both phrase galleries rank B
   first and pass the unchanged short score gate, with exactly one passing the
   unchanged short margin; and
8. exactly one embedding-backed, robust-complete VAD contains the interval.
   Both VAD galleries rank B first and pass their unchanged duration-class
   score and margin gates.

The rule writes only the target interval's exact source and forced-alignment
range. It adds no TOML field, threshold, future lookahead, identity,
transcript, timestamp, reference lookup, or fitted constant. Missing,
duplicate, non-gapless, differently ranked, differently gated, unaligned,
protected, additional-activity, or source/time-inconsistent evidence preserves
ordinary behavior. Existing specialized interval challenges retain
precedence.

## Verification and decision boundary

Focused tests must cover the complete positive topology and independent
abstention for duration, alignment, provenance, current primary, primary
brackets, activity identities and coverage, initial identity, target ranks and
gates, adjacent phrase uniqueness/boundary/ranks/gates, VAD uniqueness/ranks/
gates, and protected source labels. After a warning-clean build and complete
CTest pass, the production C++ projector replays the frozen T123 and T111
producer packages at least twice.

Automation may verify immutable input hashes, deterministic output hashes,
source reconstruction, monotonic time, and raw change scope only. Every changed
complete conversation is then read chronologically and in reverse against
`test.txt`. That contextual semantic review alone retains or removes FR37 and
updates any manually derived ledger. A retained frozen replay is not a new
real-WebSocket result and cannot close the speaker business by itself.
