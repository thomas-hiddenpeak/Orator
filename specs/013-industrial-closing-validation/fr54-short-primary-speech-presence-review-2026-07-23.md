# FR54 Short Primary Speech-Presence Review (2026-07-23)

## Status

FR54 evidence capture and all four independent contextual readings are
complete. They agree that VAD contributes useful speech-presence evidence but
does not provide one shared, reference-free topology for both material
contexts. T247 therefore stops this branch without a runtime or TOML change.
FR50 remains the real-path baseline.

## Evaluation Authority

Every finding in this document is made by direct contextual-semantic reading
against the human-listened `test/data/reference/test.txt`. The capture and
packet tools only preserve source identity, validate mechanical contracts, and
display raw evidence. No program labels speech, judges a speaker, aggregates an
accuracy value, ranks a candidate, selects a parameter, or issues a verdict.

## Frozen Evidence

| Item | Value |
|---|---|
| Runtime baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| Run A source | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-a.json` |
| Run B source | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-b.json` |
| Run A worksheet | `/tmp/orator-spec013/release-a6f0d33-fr54-vad-evidence/run-full-a-worksheets` |
| Run B worksheet | `/tmp/orator-spec013/release-a6f0d33-fr54-vad-evidence/run-full-b-worksheets` |
| Run A content manifest SHA-256 | `d8b309de64ef877d16d7e17ccf7263fa90e2116888fb0c95c2da5cb9253f2e73` |
| Run B content manifest SHA-256 | `08c0d5a036f7dd408189ed5319e6267d26cb7360bfae9d3147c1f7b77e45c72d` |
| Raw VAD window source SHA-256 | `0d1bc0f7d4a5905330f649080afd56dc2756fe6bd7c4e003a3b6ff9d956a0b93` |
| Raw VAD endpoint source SHA-256 | `c2e3cf145dcf44e973c5f783da1c4e026fcdea7a05141f99f95ebcfd451a5b01` |
| FR54 context table SHA-256 | `2c5e4a64c3d4466b75c3d471b102f5e1e3ceacf95eebf5b68f811e9b238bbf7c` |
| FR54 review-context SHA-256 | `b5e4df43208c00b968f4e16e1f86f6d70783609add1cea9b77bcfcbdd584f5f3` |

Both packet manifests pass every listed payload hash. Run A and Run B retain
their independent terminal timelines and Sortformer rows. Their raw VAD inputs
come from the same exact frozen PCM/config/model identity because the VAD path
is deterministic and independent of the restarted business-fusion run.

## Pass 1: Run A Chronological

| Context | Direct complete-context reading |
|---|---|
| `ref-0030` | Tang's long turn ends before the `235.200-235.920` Shi-labelled primary island. The finalized VAD turn ends at `235.100`, raw speech evidence falls away before the island, and no reference response exists there. The island is not speech by Shi. |
| `ref-0049` | Tang's short `对` is audible in the human reference and has a distinct raw VAD burst, but no Tang primary interval survives; the current business view assigns the retained text to Shi. Speech presence cannot reconstruct the missing speaker identity here. |
| `ref-0066` | Tang's overlapping `你们俩可以` is omitted from ASR while a Tang primary interval survives at `478.640-479.120`. It lies inside continuous finalized VAD speech, overlaps Shi's aligned phrase, and the Sortformer rows retain simultaneous Tang and Shi activity. This is real overlapping Tang speech. |
| `ref-0118` | Tang and Shi give adjacent/overlapping `对呀` responses. VAD remains continuous and ASR retains both responses, while alignment and primary ownership cross at the phrase boundary. Adding a text-free activity record cannot repair the existing phrase-ownership split. |
| `ref-0165` | The `1101.680-1102.240` Xu-labelled island starts in silence and covers only the onset of Shi's following `嗯，好`; raw VAD activity begins late and continues into the restored Shi primary interval. It is an onset identity transient, not Xu speech. |
| `ref-0238` | The `1673.680-1674.320` Shi-labelled island lies between Tang's sentence and Xu's following reaction. It is outside both finalized VAD turns. A short raw probability rise does not become a speech segment and the complete conversation contains no Shi response. |
| `ref-0264` | The `1889.760-1890.240` Shi-labelled island is inside the pause between two parts of Tang's turn. Finalized VAD ends before it, raw speech evidence decays through it, and no Shi response exists. |
| `ref-0305` | The `2189.280-2189.680` Shi-labelled island interrupts one continuous Xu source, finalized VAD turn, and aligned sentence. Xu primary evidence surrounds it immediately. VAD confirms speech but cannot distinguish the false Shi island from Xu's ongoing speech. |
| `ref-0313` | Shi's omitted `对` has a clear isolated raw VAD burst and a matching native primary island, but that local label resolves to Tang in this epoch. Speech presence is established; speaker identity is not. |
| `ref-0331` | Shi's omitted `对` produces an isolated raw VAD burst aligned with the `2362.800-2363.200` Shi primary island between Tang sources. The burst is too short to survive endpoint finalization, while the primary identity is unique in its source context. This is real Shi speech. |
| `ref-0390` | Tang's `对` has a raw VAD burst, but the matching primary onset resolves first to Xu; the next speech onset briefly resolves to Tang before Shi's number phrase. VAD proves speech events, not which conflicting identity owns them. |
| `ref-0479` | The `3101.280-3101.760` Tang-labelled island is embedded in Shi's uninterrupted explanation, continuous VAD turn, and long ASR source; Shi primary evidence surrounds it. VAD alone cannot reject this false overlap. |

## Pass 2: Run B Chronological

Run B was read from `ref-0030` through `ref-0479` without importing Run A
labels. The independent restart preserves the following complete-context
findings:

| Context | Direct complete-context reading |
|---|---|
| `ref-0030` | The Shi-labelled island follows Tang's completed sentence and the end of finalized VAD; it occurs in the conversational pause and is false. |
| `ref-0049` | The short VAD burst corresponds to Tang's real `对`, but the primary track stays on Shi. There is no identity-bearing Tang interval to preserve. |
| `ref-0066` | Tang's omitted overlapping phrase is present as a Tang primary island during continuous speech and concurrent Shi/Tang Sortformer activity. |
| `ref-0118` | Both short responses are represented lexically inside continuous VAD, but their phrase boundary crosses the primary transition. The defect is ownership, not missing speech presence. |
| `ref-0165` | The Xu island begins before speech and hands the same VAD onset to Shi; the reference contains only Shi here. It is a false onset identity. |
| `ref-0238` | The Shi island remains between finalized speech turns and has no Shi response in the full exchange. The raw rise alone is insufficient evidence of a speaker turn. |
| `ref-0264` | The Shi island lies wholly in Tang's mid-turn pause after VAD has ended. It is false. |
| `ref-0305` | The Shi island remains inside Xu's one continuous sentence, VAD span, ASR source, and aligned-word pause, with Xu primary evidence on both sides. |
| `ref-0313` | Raw VAD confirms Shi's short response exists, but the matching native primary label resolves to Tang. Identity remains contradictory. |
| `ref-0331` | The isolated raw VAD burst and unique Shi primary island coincide with the omitted human-listed response between Tang sources. |
| `ref-0390` | Separate VAD bursts exist, but the first candidate maps to Xu and the next onset maps briefly to Tang before Shi. Neither identity assignment can be trusted. |
| `ref-0479` | The Tang island again interrupts Shi's otherwise continuous source and primary sequence while VAD remains active; it is not an independent Tang turn. |

## Pass 3: Run A Reverse

Run A was re-read from `ref-0479` back to `ref-0030`, this time testing whether
voiceprint, alignment, identity epochs, or raw posterior evidence could refute
the chronological findings.

| Context | Reverse complete-context reading |
|---|---|
| `ref-0479` | The short Tang island has an embedding, but its weak scores do not establish an independent voice. The complete source strongly supports Shi, the island sits in a long alignment gap, and Shi surrounds it in the same VAD turn. It remains false. |
| `ref-0390` | Neither short primary onset has an embedding. The first maps to Xu and the second maps to Tang before Shi's phrase, so the two VAD bursts still cannot be assigned safely. |
| `ref-0331` | The short Shi island has no embedding, but local label 3 has one stable global identity and the isolated raw VAD burst matches the human-listed response. The evidence still supports real Shi speech. |
| `ref-0313` | The response burst and native island are real, but the island has no embedding and its local label resolves to Tang. Nothing in the reverse read repairs identity. |
| `ref-0305` | The island has no embedding; the complete source strongly supports Xu. Its split contribution at the aligned-word gap does not establish a second utterance. It remains false. |
| `ref-0264` | The island embedding contains only weak residual scores and appears after finalized speech. The full turn remains Tang with no Shi response. |
| `ref-0238` | The island embedding is weak, and the raw rise remains isolated outside endpoint-qualified speech. The conversation still has no Shi turn there. |
| `ref-0165` | The island embedding is weak; the raw speech onset continues directly into Shi. It remains a false Xu onset rather than a separate response. |
| `ref-0118` | Tang's short primary run has no embedding while Shi's following run does. The retained words already cross that boundary, confirming an ownership defect rather than absent activity. |
| `ref-0066` | The Tang island embedding is itself too weak and points nowhere reliably, but the native primary island, simultaneous posterior activity, continuous speech, aligned overlap, stable global mapping, and human context still agree on real Tang overlap. |
| `ref-0049` | Raw VAD and secondary Tang posterior activity show a real short response, but Tang never becomes primary and no short embedding can identify it. The evidence remains insufficient for a primary-activity repair. |
| `ref-0030` | The island embedding is only low-level residual similarity; VAD has already ended and Tang's sentence is complete. It remains false. |

## Pass 4: Run B Reverse

Run B was re-read independently from `ref-0479` back to `ref-0030`. The reverse
pass again found the long-source false islands embedded in continuous incumbent
speech, the silent false islands outside endpoint-qualified VAD, the two real
but identity-conflicted short responses, and the two materially missing
speaker contributions. It also independently confirmed that short-island
TitaNet evidence is either absent or too weak/contradictory to establish the
speaker in these cases.

| Context | Reverse complete-context reading |
|---|---|
| `ref-0479` | Strong complete-source Shi evidence surrounds a weak Tang island inside an alignment gap; the island remains false. |
| `ref-0390` | Two speech bursts remain visible, but neither short primary onset has an embedding and their mapped identities conflict with the conversational speakers. |
| `ref-0331` | The isolated subminimum VAD burst, unique Shi primary identity, and human-listed response remain mutually coherent. |
| `ref-0313` | Speech is present, but the response island still aliases to Tang and has no independent embedding. |
| `ref-0305` | Strong complete-source Xu evidence and aligned source continuity still surround the no-embedding Shi island. |
| `ref-0264` | The weak island embedding remains inside a VAD-qualified pause in Tang's turn, with no Shi contribution. |
| `ref-0238` | The raw probability rise and weak island embedding still do not establish the absent Shi response. |
| `ref-0165` | The weak Xu island remains the leading portion of a VAD onset that continues as Shi. |
| `ref-0118` | The short Tang run remains unembeddable and the retained phrase still crosses into Shi; lexical ownership is the defect. |
| `ref-0066` | The overlapping Tang island remains real in context, while its short embedding is not a reliable identity view and the containing source remains Shi-dominant. |
| `ref-0049` | The real Tang response remains secondary-only under Shi primary ownership; VAD cannot create the missing identity. |
| `ref-0030` | The weak island remains after speech has ended and does not represent Shi. |

## Four-Pass Reconciliation

All four readings agree on three evidence boundaries:

1. Finalized VAD and raw endpoint state safely expose several silent or onset
   false islands, including `ref-0030`, `ref-0165`, `ref-0238`, and
   `ref-0264`.
2. VAD cannot distinguish a true overlap such as `ref-0066` from false
   speaker islands inside continuous incumbent speech such as `ref-0305` and
   `ref-0479`. Forced-alignment edge placement and source continuity are
   necessary, but the reviewed set contains only one material overlap of that
   exact form.
3. Raw pre-endpoint VAD proves that `ref-0331`, `ref-0313`, and `ref-0390`
   contain short speech that final VAD omits. It does not establish identity;
   `ref-0313` and `ref-0390` remain alias conflicts. The structurally nearest
   non-speech control, `ref-0238`, also contains a short raw rise. Choosing a
   new probability, duration, or timing boundary to separate the single
   `ref-0331` material example would be a fitted one-case rule.

The two material contexts therefore require different corroboration paths:
`ref-0066` is simultaneous speech inside an existing VAD/source span, while
`ref-0331` is an isolated subminimum speech burst between source spans. A
disjunction tailored to those two examples is not the one shared topology
required by FR54. Short-run TitaNet evidence does not unify them.

## T247 Decision

FR54 authorizes no producer experiment. In particular, it does not authorize:

- a VAD-overlap gate, because that admits continuous-speech false islands;
- a raw-probability or timing threshold selected from these examples;
- a voiceprint-short-island gate, because the material and control embeddings
  are absent, weak, or contradictory;
- a union of separate `ref-0066` and `ref-0331` special cases.

The raw VAD capture and packet extension are retained as evidence tooling. The
runtime, checked-in TOML, models, final business view, and manually retained
FR50 result remain unchanged. The unresolved boundary moves back to the
speaker producer: future work must find a second independently reviewed
material occurrence for either overlap provenance or isolated subminimum
speech identity before proposing a general rule.

## Result Boundary

This is a stop decision for FR54, not a speaker-closure verdict and not an
accuracy change. It leaves `ref-0066` and `ref-0331` unresolved in the accepted
FR50 product output and prevents the rejected FR53 activity branch from
returning under a different name.
