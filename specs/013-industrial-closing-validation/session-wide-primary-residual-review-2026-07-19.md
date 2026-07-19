# Session-Wide Primary Residual Review (2026-07-19)

## Scope and authority

This FR46 record completes the contextual semantic review of every critical
speaker-business residual that remains after retained FR32-FR45. The sole
product authority is the human-listened
`test/data/reference/test.txt`. The reviewer read each complete conversation
chronologically and then read the same conversation in reverse, while
inspecting the frozen T123 business, ASR, forced-alignment, activity, primary,
VAD, and capture-faithful primary-run identity displays on the common clock.

No code, script, query, formula, metric, or algorithm assigned correctness,
grouped a product result, ranked a candidate, selected a rule, or issued the
decision below. Tools only captured immutable rows, arranged raw evidence,
checked mechanical contracts, and computed hashes. The labels and conclusions
in this document were written manually after the complete contextual review.

The frozen slot map is `spk_0 = Zhu Jie`, `spk_1 = Tang Yunfeng`,
`spk_2 = Xu Zijing`, and `spk_3 = Shi Yi`.

## Reviewed artifacts

| Artifact | SHA-256 |
|---|---|
| Frozen FR45 T123 business view | `5a595ca1aa5816612b2603062d8467ee60bc3a342219cf5eda066cfddc3bb61a` |
| Original full reference-oriented display | `ebb49f090f5cb7271099efc5ed5a0f40dda464cb313002fa9b5143ee98d1927f` |
| All-primary exact-clock query list | `0747c0ca48c61d26450d34978873688ec5c605a43a58dee3e1482201c14e5169` |
| Repeated all-primary dual-gallery display | `6c7b4c9b1e17a08895a3c4a88eab855acf8e42fb530fef90c093ce7f6b88d366` |
| Repeated final identity replay | `b1f42d0085adcafaf6564479bbc88895518adfa80d84891cd8be3dde843467fa` |

The complete human-reference windows read for the forward pass were
`test.txt` lines 72-176, 180-252, 480-528, 588-684, 690-940, and 980-1038.
The reverse pass traversed those same conversations from their final response
back to their opening premise. No isolated reference line was accepted as
sufficient context.

## Complete critical review

### 0-600 seconds

- `ref-0049`, Tang's short `对`: the retained ASR/alignment business span
  `432.012-433.452` is Shi. Primary is Shi across `428.80-433.44`, activity
  remains Shi through `433.60`, the containing VAD displays Shi first, and both
  complete primary-run galleries display Shi first. Forward and reverse
  conversation require Tang between Shi's 50-billion proposal and Zhu's
  response, but no inspected machine track supplies that identity at the
  source. This is producer-wrong evidence, not a fusion omission.
- `ref-0058`, Tang's `相差0.7`: the source-aligned text at
  `461.244-462.044` is the different phrase `二十八` and the business view is
  Shi. Primary and both primary-run galleries display Shi, as does the
  containing VAD. Activity contains a concurrent Tang interval from
  `461.44-463.12`, but also Shi. The listener-verified contribution is not
  preserved as source text, and the available speaker tracks disagree.
- `ref-0066`, Tang's repeated `你们俩可以`: T123 ASR has no second source
  phrase to attribute. Activity and primary contain a Tang return around
  `478.40-479.36`, but the exact primary-run galleries display Xu while the
  containing VAD displays Shi. This is a source-absent contribution with
  mutually inconsistent identity evidence; fusion cannot synthesize its text
  or choose Tang safely.

### 600-1200 seconds

- `ref-0099`, Shi's governance consequence: ASR retains the complete source.
  The business view writes only the middle clause
  `661.276-662.076` to Zhu. One Shi primary run covers
  `660.16-662.48`; matching activity, the containing VAD, and both complete
  primary-run galleries all independently display Shi. The complete forward
  and reverse exchange also requires Shi. This is the only reviewed critical
  residual in which complete, mutually corroborating native evidence is
  present and a later fusion decision overwrites it.
- `ref-0102`, Tang's `就不用开会了`: ASR retains the phrase at
  `669.116-669.676`, but business, the long `663.92-672.40` primary run,
  both primary-run galleries, and VAD display Shi. Activity has a Tang interval
  overlapping `666.96-670.08` as well as Shi support. The correct identity is
  visible in only one conflicting producer view, so the evidence does not
  authorize a shared write.
- `ref-0118`, Tang's `对啊`: the retained source is split into Shi `对` and
  Tang `呀`. Primary and activity contain a Tang return around
  `806.64-807.36`, bracketed by Shi, but the exact `0.32 s` primary run is below
  the embedding floor and has no complete dual-gallery evidence. The broad VAD
  includes surrounding Shi speech. Context establishes the turn, but the
  complete independent evidence gate cannot be satisfied.

### 1200-1800 seconds

- `ref-0252`, Zhu's `对啊，两位老板决定啊`: the retained source is split
  between Shi and Tang. Primary changes from Shi to Tang across
  `1768.80-1773.92`; activity and VAD show the same two identities, and the
  primary-run galleries do not supply Zhu. Forward and reverse context require
  Zhu's separate decision-ownership response, but all substantive acoustic
  evidence is producer-wrong.

### 1800-2400 seconds

- `ref-0313`, Shi's equity-transfer `对`: no business source remains in the
  reference window. The nearby short primary evidence maps to Tang, no activity
  segment covers the exact response, and the available VAD displays Tang. The
  boundary span has no complete primary-run gallery. This is source absence
  without a corroborated Shi island.
- `ref-0327`, Shi's licensing `对`: business assigns the retained token to
  Tang. A short primary run at `2344.40-2344.80` displays Shi, but it is too
  short for complete dual-gallery evidence; activity and VAD display Tang.
  One uncorroborated primary micro-run cannot override the other tracks.
- `ref-0331`, Shi's terminal `对`: no business entry remains in the source
  window. A `2362.80-2363.20` Shi primary micro-run is present, but activity
  ends with Tang before the boundary, no exact VAD covers it, and the run has
  no embedding. This is a source-edge absence with incomplete controls.
- `ref-0333`, Shi's next terminal `对`: ASR appends the token to Tang's
  preceding phrase rather than preserving a separate response. Primary,
  activity, and VAD display Tang, and the primary-run gallery supplies no
  independent Shi evidence. This is producer-wrong evidence, not an ignored
  island.

### 2400-3000 seconds

- `ref-0354`, Zhu's `对，独立公司`: business assigns the phrase to Tang.
  Primary, activity, VAD, and both primary-run galleries also display Tang
  across the complete phrase. No inspected track contains a Zhu return.
- `ref-0375`, Xu's `可以啊`: the only retained aligned token is the ASR
  mistranscription `不要`, displayed as Xu, followed by Shi's continuation.
  Primary, activity, and both exact primary-run galleries support Xu, although
  the robust gallery does not clear the existing margin; the broad VAD displays
  Shi because it includes the following turn. The listener-verified response
  is not preserved as a complete business contribution, and the independent
  gate is incomplete.
- `ref-0390`, Tang's negotiation `对`: ASR appends the token to Shi's phrase.
  A Tang primary return begins later around `2690.80`, after the source
  coordinate, and is too short for an embedding. Activity is Shi at the source;
  the preceding VAD is mixed and the following VAD displays Tang. This is a
  delayed source/primary alignment topology without an exact identity query.
- `ref-0426`, Shi's B/C-package proposal: business fragments the substantive
  source across Tang and Xu. Activity contains two Shi intervals inside the
  exchange, but primary alternates Tang and Xu, the containing VAD displays Xu,
  and primary-run gallery evidence does not corroborate Shi over the complete
  contribution. The correct activity view is partial and conflicts with the
  other producers.
- `ref-0442`, Zhu's first `什么价格`: the listener-verified phrase is absent
  or mistranscribed at the source edge. Primary and activity show Tang and Shi;
  the short containing VAD's session and robust galleries disagree between Xu
  and Shi. No inspected evidence supplies Zhu.
- `ref-0444`, Zhu's repeated price question: business fragments the retained
  source across Shi, Tang, and Xu. Primary, activity, VAD, and primary-run
  galleries provide no Zhu identity over the question. The complete context
  establishes Zhu, but the machine evidence is producer-wrong.
- `ref-0461`, Tang's financial-responsibility statement: business and the
  substantive primary interval `2984.48-2986.56` display Shi. Activity includes
  Tang only at the leading edge through about `2985.36`, then Shi; VAD and both
  primary-run galleries display Shi. The partial activity overlap cannot
  support rewriting the complete statement.

### 3000-3615 seconds

- `ref-0499`, Shi's `哦，没区别`: business splits the source between Tang
  and unknown. Primary and activity display Tang; the VAD galleries are weak
  and disagree, and neither displays Shi first. No correct native evidence is
  available.
- `ref-0503`, Zhu's sustained nominee-ownership proposal: activity and primary
  display Zhu only at the opening around `3268-3269`, then display Shi through
  the substantive proposal. The successive VAD and primary-run galleries also
  display Shi. This is a sustained upstream diarization/identity error, not a
  boundary-only fusion defect.
- `ref-0505`, Tang's Hangzhou-stake statement: its ASR source is displaced
  earlier to `3301.404-3303.964` and business assigns it to Shi. Primary,
  activity, VAD, and both primary-run galleries also display Shi there. There
  is no Tang evidence at the retained source coordinate.
- `ref-0507`, Tang's `5.6个亿`: business, primary, and activity display Zhu.
  The exact primary query does not agree internally: the session gallery
  displays Shi first while the robust gallery displays Tang first. The
  containing VAD displays Shi. Correct identity appears in only one gallery,
  so independent agreement is absent.
- `ref-0509`, Tang's strategy `对`: ASR merges the response into Shi's
  continuing phrase. Primary, activity, VAD, and both primary-run galleries
  remain Shi through the source interval; no Tang island exists to project.

## Accepted neighboring controls

The forward and reverse passes also retained the surrounding correctly
attributed turns as abstention controls. These controls show that the frozen
slot map remains locally usable and prevent interpreting a disputed residual
as a broad identity-map reversal.

| Conversation cluster | Retained neighboring controls |
|---|---|
| `ref-0049` to `ref-0066` | `0048`, `0050`, `0051`, `0056`, `0057`, `0060`, `0062`, `0064`, `0065`, `0067`, `0068` |
| `ref-0099` to `ref-0118` | `0097`, `0098`, `0100`, `0101`, `0103`, `0116`, `0117`, `0119`, `0120` |
| `ref-0252` | `0251`, `0253`, `0254` |
| `ref-0313` to `ref-0333` | `0311`, `0314`, `0326`, `0328`, `0329`, `0330`, `0332`, `0334`, `0335` |
| `ref-0354` to `ref-0461` | `0352`, `0353`, `0355`, `0356`, `0374`, `0376`, `0389`, `0391`, `0425`, `0427`, `0441`, `0443`, `0445`, `0460`, `0462` |
| `ref-0499` to `ref-0509` | `0498`, `0500`, `0501`, `0502`, retained `0504`, `0508`, `0510`, `0511` |

Reading backward from each control through the disputed turn and into the
preceding control produced the same speaker order as the chronological pass.
It did not reveal a second `ref-0099`-style case hidden by forward phrasing.

## T190 decision boundary

The complete review does not establish one source-free topology that repairs
multiple critical residuals under independent abstention controls.
`ref-0099` alone has complete agreement among source, alignment, primary,
activity, VAD, and both identity galleries while the final fusion output is
wrong. The remaining contexts have at least one structurally different hard
boundary: missing or displaced source, a subminimum acoustic island,
incomplete or disagreeing galleries, partial activity only, or upstream
speaker evidence that supports the wrong identity.

FR46 therefore ends as an evidence diagnosis. It authorizes no production
fusion rule, TOML change, model change, audio run, frozen-ledger change, or
speaker-business closure claim. A rule fitted only to `ref-0099` would repeat
the single-residual patch sequence that FR46 explicitly stopped and would not
address the dominant evidence failures.

The next speaker-closing phase must work upstream: recover usable alternative
diarization evidence when top-1 activity/primary is wrong, preserve short-turn
evidence without inventing source text, and establish whether the two
orthogonal speaker models expose complementary raw evidence before proposing
another fusion policy. That work requires a new SDD evidence gate and the same
complete contextual semantic review; this document does not pre-authorize its
implementation.
