# FR55 Gallery-Independence Context Review (2026-07-23)

## Status

FR55 evidence capture and all four independent contextual readings are
complete. One retained TitaNet reference materially reuses audio from a
misattributed `ref-0503` query, but none of the other three focus contexts has
that dependency. T252 therefore stops this branch without a runtime, TOML,
model, product-run, ledger, or baseline change. FR50 remains the accepted
real-path baseline.

## Evaluation Authority

Every speaker and dependency judgment in this document comes from direct,
complete contextual-semantic reading against the human-listened
`test/data/reference/test.txt`. The tools only copy frozen inputs, replay the
production identity stage, expose retained-reference provenance, check
mechanical equality, and display raw scores. No code, script, notebook,
formula, query, metric, or algorithm labels a speaker, assigns correctness,
aggregates accuracy, ranks a repair, selects behavior, or issues a product
verdict.

## Frozen Evidence

| Item | Value |
|---|---|
| Runtime baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| Run A source SHA-256 | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| Run B source SHA-256 | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Exact streamed PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Exact WAV wrapper SHA-256 | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Checked-in TOML SHA-256 | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Sortformer v2.1 SHA-256 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| TitaNet-Large SHA-256 | `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |
| Run A final / Run B input registry SHA-256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| FR55 context table SHA-256 | `4a81d06398d0af37597764484c1dffa880262036ec5b5b714a9c6e54de97efde` |
| Run A packet content manifest SHA-256 | `3764dd93174870eee8006f57f5be8b090ccf4b6106c0517930fccd6843a1d8cc` |
| Run B packet content manifest SHA-256 | `3d19004450d79c0a57dc33a5521d6f71f5b7c28fb5e4897a5fed59aba985d87b` |

Both packet manifests pass every listed payload hash. The final provenance fix
was replayed against both frozen event streams. Run A reproduced all 1,254,063
captured identity rows and Run B reproduced all 1,253,630 rows with no identity
difference. The final identity output, retained-reference TSV, and query
evidence are byte-identical to the pre-fix evidence and between A/B:

| Mechanical artifact | SHA-256 |
|---|---|
| Final identity replay | `b1f42d0085adcafaf6564479bbc88895518adfa80d84891cd8be3dde843467fa` |
| Retained-reference provenance | `3de7965957be162f453d70dfdbe5bb31bd7734bf6997dd984c62d7c7ecf26335` |
| Inclusive and overlap-excluded query evidence | `704ec2e80eecc173e3301f59b2f9eea52e2cf2f875aad2003d439f9960bb427e` |

These are producer-contract facts only. They do not evaluate a speaker result.

## Complete Reference Reading

Every one of the 38 final retained references was read with its surrounding
human transcript in Run A chronological order, Run B chronological order, Run
A reverse order, and Run B reverse order. All four readings independently
established the session identity map used only for this review:

| Global ID | Human speaker |
|---|---|
| `spk_0` | 朱杰 |
| `spk_1` | 唐云峰 |
| `spk_2` | 徐子景 |
| `spk_3` | 石一 |

The readings found one material reference-identity contradiction:

- `identity_ref:0:1:1` is assigned to `spk_3` and embeds
  `3274.379937500-3277.939937500`, wholly inside the human-reviewed continuous
  Zhu Jie turn `ref-0503` (`3268-3299`). It is therefore a Zhu Jie embedding
  retained under Shi Yi's identity.

Two boundary cases remain semantically coherent and do not establish another
material contamination topology:

- `identity_ref:0:1:0` crosses the coarse `ref-0500/ref-0501` boundary, but its
  substantive embedded content is Shi Yi and agrees with `spk_3`.
- `identity_ref:1:3:0` crosses the coarse `ref-0510/ref-0511` boundary by only
  the turn edge; its substantive embedded content is Tang Yunfeng and agrees
  with `spk_1`.

The other retained references remain coherent with their complete human
contexts in all four readings. This statement is a manual reconciliation, not
an automated label or calculated rate.

## Pass 1: Run A Chronological

| Context | Direct complete-context reading |
|---|---|
| `ref-0102` | Tang repeats `就不用开会了`, but the business interval is Shi. Sortformer retains Tang as secondary activity while Shi remains primary across the exchange. No retained reference intersects the focus query, so gallery self-reference cannot explain it. |
| `ref-0354` | Zhu says `对，独立公司`, while the business view assigns Tang. The active Sortformer slots both resolve toward Tang in this interval. No retained reference intersects the focus query; the conflict is already present in producer/identity evidence. |
| `ref-0499` | Shi says `哦，没区别`, while the business view fragments the response across Zhu, Tang, and an unassigned interval. The focus ends before `identity_ref:0:1:0` begins, so the retained gallery did not create this focus error. |
| `ref-0503` | Zhu speaks continuously through the nominee proposal. The opening Sortformer interval maps to Zhu, then Shi-mapped local slots dominate and the business view becomes mostly Shi. `identity_ref:0:1:1` is cut from this same Zhu turn and retained as Shi, so its robust TitaNet support for Shi is not independent. Excluding it lowers that raw robust score, but the session gallery and non-overlapping Shi evidence remain and do not establish Zhu. |

## Pass 2: Run B Chronological

Run B was read independently without importing Run A's context decisions. It
reproduced the same four complete conversations and reached these findings:

| Context | Direct complete-context reading |
|---|---|
| `ref-0102` | Tang's repeated phrase is again owned by Shi because the primary producer remains Shi despite secondary Tang activity. The focus has no gallery intersection. |
| `ref-0354` | Zhu's brief confirmation is again assigned to Tang by contradictory producer/identity evidence, with no overlapping retained reference. |
| `ref-0499` | Shi's response is again fragmented before the later Shi reference begins. Gallery overlap is absent from the focus. |
| `ref-0503` | Zhu's continuous turn is again overtaken by Shi-mapped local activity, and the same Zhu audio is retained as `spk_3`. Removing that direct reference weakens only the robust view; remaining upstream evidence still points to Shi and supplies no reusable Zhu decision. |

## Pass 3: Run A Reverse

The reverse reading started after the local-slot changes and read back through
the controls and retained-reference contexts:

| Context | Reverse complete-context reading |
|---|---|
| `ref-0503` | The following Tang turn and preceding Xu turn confirm that `3268-3299` is one Zhu turn. The contaminated reference is real, but non-overlapping Sortformer and identity evidence still contradict the human speaker. |
| `ref-0499` | The later clean Shi control cannot be projected backward onto the earlier fragmented response; the focus itself remains reference-independent. |
| `ref-0354` | Surrounding Zhu/Tang turns preserve Zhu's short confirmation, while the producer slots remain Tang-mapped. No circular gallery path appears. |
| `ref-0102` | Reading back from Shi's continuation preserves Tang's repeated interjection and the simultaneous secondary activity, but no retained reference touches the focus. |

## Pass 4: Run B Reverse

The independent Run B reverse reading confirmed the same boundary: only
`ref-0503` contains a materially wrong retained reference drawn from its own
query audio. `ref-0102`, `ref-0354`, and `ref-0499` remain genuine speaker
errors whose reviewed focus queries have no retained-reference intersection.
Their surrounding controls also preserve the same producer conflicts and turn
sequence found in the other three passes.

## Control Reconciliation

The named controls prevent a generic leave-overlap-out runtime rule:

- Queries around `ref-0501` legitimately intersect
  `identity_ref:0:1:0`, whose substantive audio is correctly Shi. Removing
  direct overlap lowers raw robust Shi support even though the control remains
  semantically coherent.
- `ref-0504` starts correctly as Tang and later follows Shi-dominant producer
  evidence. The contaminated Zhu reference does not directly overlap that
  later query and cannot explain the transition.
- The controls surrounding `ref-0102` and `ref-0354` show valid short turns and
  simultaneous activity under the same mature gallery even though their focus
  errors have no circular dependency.

Thus overlap is provenance that must be visible, not a sufficient reason to
discard a reference or choose a different identity.

## T252 Decision

FR55 authorizes no leave-overlap-out runtime experiment. The manual gate
required at least two material contexts with one shared circular-reference
topology and explicit abstention from every accepted control. Only `ref-0503`
meets the first condition, while coherent controls demonstrate that direct
overlap can also be legitimate.

The evidence also narrows the model relationship: Sortformer and TitaNet are
separate models, but TitaNet's robust support is not independent when its
gallery contains the same mislabelled audio being queried. Removing that one
reference does not repair `ref-0503`, because the Sortformer/local-identity
producer path and remaining session evidence still carry the wrong identity.

The provenance fields, strict snapshot exporter, replay output, and packet
display remain as engineering evidence tooling. No runtime consumer calls the
overlap-excluded diagnostic view, and no TOML key is introduced.

## Result Boundary

This is a stop decision for FR55, not a speaker-closure verdict and not an
accuracy change. It leaves the four reviewed FR50 residuals unchanged and
moves the unresolved work back to the producer boundary: future work must
derive a reusable identity or local-slot correction from multiple independently
reviewed material contexts before any product experiment is specified.
