# How the solver chooses a word

Wowdle filters possible answers using exact Wordle feedback, evaluates a pool of legal guesses, and
ranks those guesses with a selected strategy. The browser extension uses the adaptive strategy. The
CLI also exposes seven baselines so the adaptive choices can be compared with simpler objectives.

This document describes the implemented algorithms. Build instructions and controls are in the
[usage guide](guide.md); experiment settings and result interpretation are in
[benchmarking](benchmarks.md).

[Feedback](#feedback-comes-before-strategy) · [Probabilities](#probabilities-describe-answers) · [Guess pool](#which-guesses-are-evaluated) · [Strategies](#the-eight-strategies) · [Adaptive score](#the-adaptive-score) · [Worked example](#reading-a-recorded-recommendation) · [Complexity](#cost-of-a-recommendation)

## The state and the code

These terms need to stay separate:

| Term | Meaning |
| --- | --- |
| Answer dictionary | Words the solver initially considers possible answers. |
| Candidates, `C` | Answers consistent with every feedback row received so far. |
| Valid guesses, `V` | Words available to guess, including words already ruled out as answers. |
| Guess pool, `G` | The subset evaluated for the next recommendation. |
| Answer probability, `P(w)` | A candidate's normalized prior weight. |
| Guess score | A strategy's estimate of how useful playing a word would be. |

A word in `V` but outside `C` is an **information-only guess**, also called a probe. It may reveal
letters or distinguish answers, but it cannot solve the puzzle on that turn. A word in `C` can serve
both purposes: collect information and possibly win.

The default five-letter dictionary supplies both initial answers and valid guesses. The CLI can
receive separate dictionaries. The extension keeps its five-letter product scope; the feedback,
filtering, and scoring functions work with other equal, nonzero word lengths.

| Responsibility | Implementation |
| --- | --- |
| Duplicate-safe feedback | [`core/feedback.cpp`](../src/core/feedback.cpp), `evaluate_feedback` |
| Exact candidate filtering | [`core/candidate_filter.cpp`](../src/core/candidate_filter.cpp), `filter_candidates` |
| Candidate probabilities | [`probability/prior.cpp`](../src/probability/prior.cpp), `rank_candidates` |
| Shared bucket metrics and six fixed objectives | [`strategies/scoring.cpp`](../src/strategies/scoring.cpp) |
| Guess pool and adaptive ranking | [`strategies/adaptive.cpp`](../src/strategies/adaptive.cpp) |
| Binary and turn-adjusted probe risk | [`strategies/risk.cpp`](../src/strategies/risk.cpp) |
| Random baseline, common endgame rules, computer games | [`evaluation/benchmark.cpp`](../src/evaluation/benchmark.cpp) |
| Extension state and JSON analysis | [`wasm/solver_bridge.cpp`](../src/wasm/solver_bridge.cpp) |

In the formulas below, `N = |C|`, `K = |G|`, `L` is word length, and `r` is the number of guesses
remaining **before** the next guess.

## Feedback comes before strategy

`evaluate_feedback(guess, answer)` returns one mark per letter: `g` for green, `y` for yellow, and
`x` for gray. It makes two passes:

1. Mark exact position matches green. Count the letters in unmatched answer positions.
2. Visit the remaining guess positions from left to right. Give a yellow only when an unmatched copy of that letter remains, then consume that copy.

Consider `ALLEE` against `APPLE`:

```text
guess:    A L L E E
answer:   A P P L E
feedback: g y x x g
```

The first `A` and final `E` are green. The unmatched answer letters are now `P`, `P`, and `L`. The
first guessed `L` consumes the one available `L`; the second gets gray. The earlier guessed `E` is
gray because the answer's only `E` was already used by the green match.

This allocation matters when filtering. A gray duplicate does not necessarily mean the letter is
absent from the answer.

`filter_candidates` therefore does not maintain a separate collection of green/yellow/gray rules. It
keeps a candidate exactly when:

```text
evaluate_feedback(guess, candidate) == received_feedback
```

For candidates `{apple, allee, ample, apply}`, the row `allee / gyxxg` keeps `apple` and `ample`.
`allee` would produce `ggggg`; `apply` would produce `gyxxx`. Both are rejected.

The feedback engine is the source of truth for filtering and all partition-based scores. The
duplicate examples are covered in [feedback tests](../tests/test_feedback.cpp), [filter
tests](../tests/test_candidate_filter.cpp), and [shared scoring tests](../tests/test_scoring.cpp).

## Probabilities describe answers

For a candidate `w`, let its effective weight be:

```text
a(w) = supplied weight(w), if that weight is positive
       10^-12, otherwise
```

Missing and zero weights receive the small fallback rather than removing a word. The loaders reject
negative weights; recommendation entry points also reject nonfinite weights. Filtering alone decides
whether a word remains possible.

The current candidate probability is:

```text
P(w) = a(w) / sum[a(c) for c in C]
```

After filtering, the surviving weights are normalized again. With no frequency data, every word
receives the same fallback, giving the uniform model `P(w) = 1/N`.

The bundled weights come from English `wordfreq` values generated by
[`generate_frequency_data.py`](../tools/generate_frequency_data.py). They estimate word commonness.
They are not a calibrated model of an official Wordle editor's answer choices.

The ranked answer list shows these probabilities. The recommendation list shows guess usefulness. A
low-probability candidate can be recommended because its feedback is useful; a probe has zero answer
probability regardless of its information value.

### Two probability assumptions are in use

The active bucket metrics treat the remaining candidates as **uniformly likely**. Entropy, expected
survivors, minimax, and distance do not use frequency weights. Frequency priors affect answer
likelihood, shortlist construction, and final-turn selection.

That distinction is deliberate in the current implementation and makes the baseline calculations
easy to verify. It also means “expected candidates” is an expectation under the uniform model, even
when the answer list displays nonuniform probabilities. A future frequency-weighted metric would be
a separate experimental change, not a reinterpretation of these values.

## Which guesses are evaluated?

Evaluating every valid guess against every candidate can be expensive. `make_guess_pool` uses a
default cap of 100:

1. Rank candidates by prior probability, breaking equal probabilities alphabetically.
2. Add up to 50 candidates: generally `min(N, max(1, floor(limit/2)))`.
3. Rank valid guesses by their own prior weights. Add distinct words in that order until the cap is reached.
4. If space remains, add more ranked candidates.

Duplicates do not consume slots. If the union contains fewer words than the cap, the pool is
smaller. A one-word cap keeps the most likely candidate. A zero cap includes the full distinct union
of candidates and valid guesses.

The second half is not reserved exclusively for probes: a valid word that is also a candidate may
fill it. Initially the default valid and answer dictionaries are identical, so every shortlisted
word is a candidate. After feedback removes answers, some of those same valid words become probes.

The cap is a heuristic. “Best guess” means best under the selected objective **among evaluated
guesses**, subject to endgame overrides. The shortlist can miss a stronger guess. Priors also
influence a uniform-metric baseline through this shared shortlist; the complete pipeline is not
prior-free merely because its entropy formula is uniform.

The CLI evaluator removes played words from its available-guess list. Adaptive ranking separately
guarantees that the highest-probability candidate is considered, including when a caller supplies a
probe-only pool. Tiny and final states override the normal pool as described below.

## One guess partitions the candidates

For a guess `g`, simulate its feedback against every candidate. Answers producing the same pattern
form a bucket:

```text
B_f(g) = { a in C : feedback(g, a) = f }
b_f    = |B_f(g)|
q_f    = b_f / N
```

Only nonempty buckets contribute to sums. The feedback pattern itself contains `L` marks, so there
are at most `3^L` patterns; some patterns are impossible in practice.

The following small example is also a regression test:

```text
candidates: {aa, bb, cc}
guess:      aa

pattern gg: {aa}      size 1
pattern xx: {bb, cc}  size 2
```

There is a `1/3` chance of retaining one candidate and a `2/3` chance of retaining two under the
uniform model.

| Metric | Calculation | Value |
| --- | --- | --- |
| Entropy | `-(1/3) log2(1/3) - (2/3) log2(2/3)` | `0.918296` bits |
| Expected candidates | `(1/3)*1 + (2/3)*2` | `5/3` |
| Worst-case candidates | `max(1, 2)` | `2` |
| Feedback Hamming separation | Two separated pairs out of three; both marks differ in each | `2/3` |
| Bucket separation | Two pairs receive different patterns out of three | `2/3` |

The all-green bucket contains the answer, so it has size one. Expected candidates counts the
candidate set after feedback; it does not assign zero to a solved game and is not an estimate of
future turns required.

## The eight strategies

The six fixed scoring objectives are implemented by `rank_baseline_guesses`. It sorts higher scores
first, using alphabetical order for equal scores. Expected-candidate and minimax scores are negated
so the same descending sort can be used.

The shared analysis structure calls its ranking field `combined_score`, even for a fixed strategy.
For a baseline, that field contains its own objective or shuffle rank. Its units and scale differ
between strategies; compare game results rather than those raw scores.

### Random

Random shuffles the current candidates with the seeded generator and chooses the first. It samples
candidates uniformly, not according to frequency weights and not from the full valid-guess
dictionary. It gives a simple reference for whether more expensive scoring helps.

Its displayed ordering score is just the shuffle rank. It has no information-theoretic meaning.

### Letter frequency

Let `F(c)` be the fraction of candidates containing letter `c` at least once. The score is:

```text
frequency(g) = sum[F(c) for each distinct letter c in g]
```

Each letter counts once per candidate and once per guess. This rewards coverage without giving
`AAAAA` five copies of the same frequency credit. It is cheap, but it ignores positions and the
actual feedback buckets a complete word produces.

### Positional frequency

Let `F_i(c)` be the fraction of candidates with letter `c` at position `i`:

```text
positional(g) = sum[F_i(g[i]) for i = 1..L]
```

This rewards letters in plausible answer positions. Repeated letters can contribute at different
positions because those are separate observations. The score is still a sum of individual letter
statistics; it does not model the joint feedback pattern.

### Entropy

```text
H(g) = -sum[q_f * log2(q_f)]
```

Higher entropy favors a guess whose possible feedback outcomes are informative under the uniform
model. Balanced buckets tend to score well. Entropy does not directly reward guessing the answer,
and maximizing one turn's information does not necessarily minimize the number of turns needed to
finish.

### Expected remaining candidates

```text
E(g) = sum[q_f * b_f] = sum[b_f^2] / N
```

Lower is better. Larger buckets receive more weight because they are both a larger outcome and a
more likely one. This optimizes expected candidate-set size after one guess. It does not bound the
worst outcome or solve a multi-turn decision tree.

### Minimax

```text
W(g) = max[b_f]
```

Lower is better. Minimax chooses the smallest largest feedback bucket, limiting the worst remaining
set. It can sacrifice good average outcomes to improve a rare bad branch. The implementation is
one-step minimax over bucket size, not recursive game-tree search.

### Feedback distance

For feedback vectors `u` and `v`, Hamming distance counts positions where their marks differ. The
active distance score is the average normalized distance across distinct candidate pairs:

```text
D(g) = sum[Hamming(feedback(g,a), feedback(g,b)) for unordered a != b]
       / (choose(N,2) * L)
```

`D` lies between zero and one. Entropy depends on bucket probabilities and ignores the geometry of
their pattern labels. Distance also uses how many feedback positions differ: patterns that disagree
in one tile contribute less than patterns that disagree in every tile.

This is an experimental objective. A larger geometric difference between feedback vectors does not
by itself prove that a guess will solve the game faster.

### Adaptive hybrid

Adaptive ranks guesses using entropy, feedback distance, candidate reduction, answer likelihood, and
probe risk. Candidate count selects a set of weights; remaining turns changes probe risk and
activates the final-turn rule. The exact formula and defaults follow below.

### Common endgame rules

`recommend_guesses` in the CLI/evaluator applies two rules to **all eight strategies**:

- With one candidate, guess it directly.
- With one turn remaining, choose the candidate with highest prior probability. Do not play a probe.

These states delegate to adaptive ranking. The final state returns at most five candidates in
probability order and calculates their analysis metrics; it does not evaluate expensive feedback
metrics for every remaining answer. Equal probabilities use alphabetical order.

The random and fixed baselines therefore share practical endgame behavior rather than following
their named objective blindly to the last turn. This matters when interpreting comparisons.

## Distance without enumerating every pair

The active scorer obtains `D(g)` from per-position mark counts while it builds the buckets. Let
`n_i,s` be the number of candidates whose feedback at position `i` is mark `s`, and let `T =
choose(N,2)`.

At that position, the number of pairs with the same mark is `sum_s choose(n_i,s,2)`. All other pairs
disagree. Therefore:

```text
D(g) = sum_i [T - sum_s choose(n_i,s,2)] / (T * L)
```

This is the exact pairwise average, not a sample. It needs one pass over candidate feedback rather
than two feedback evaluations for every pair. For `N < 2`, the implementation returns zero
separation.

[`distance.cpp`](../src/strategies/distance.cpp) retains independently testable metrics: raw word Hamming distance, raw feedback Hamming distance, weighted feedback distance with values `x=0, y=1, g=2`, a direct pairwise average, and bucket separation. Weighted distance divides the sum of absolute mark-value differences by `2L`.

The active scorer uses the efficient **unweighted feedback Hamming average**. It does not use word
Hamming or weighted feedback distance in the adaptive score. The direct pairwise implementation is
an independent reference checked by [scoring tests](../tests/test_scoring.cpp).

### Bucket separation is already candidate reduction

Bucket separation is the fraction of candidate pairs assigned different whole feedback patterns:

```text
S(g) = 1 - sum_f choose(b_f,2) / choose(N,2)
     = 1 - (E(g)-1)/(N-1)
     = (N-E(g))/(N-1), for N > 1
```

This is exactly the adaptive normalized reduction score under the uniform model. Treating bucket
separation as an additional independent reward would duplicate that component. The active distance
component is feedback Hamming separation instead. Its relationship with solving performance still
needs measurement; the metrics are not assumed to be independent evidence.

The earlier [`entropy.cpp`](../src/strategies/entropy.cpp) also retains standalone entropy and a
fixed 70/30 entropy/likelihood combination. They have tests, but the current adaptive recommendation
path uses `scoring.cpp` and `adaptive.cpp`.

## The adaptive score

All component scores are scaled before combining them:

| Symbol | Implemented value | Purpose |
| --- | --- | --- |
| `I` | `H / min(log2(N), L*log2(3))` | Entropy relative to its candidate/pattern-count upper bound. |
| `D` | Normalized feedback Hamming average | Degree of feedback separation. |
| `Q` | `P(g) / max[P(c) for c in C]` | Likelihood relative to the most likely remaining answer. |
| `R` | `0` for a candidate, otherwise `1/r` | Cost of spending a turn on a known-impossible answer. |
| `C_red` | `(N-E)/(N-1)` | Fraction of achievable one-step candidate reduction. |

For a singleton, information and reduction are defined as zero to avoid division by zero. The
implementation clamps small numerical excursions of information/reduction to `[0,1]`. A probe has
`P(g)=0`, so `Q=0`.

With coefficients `wI`, `wD`, `wQ`, `wR`, and `wC`:

```text
score(g) = (wI*I + wD*D + wQ*Q - wR*R + wC*C_red)
           / (wI + wD + wQ + wR + wC)
```

Reduction is a positive reward in this implementation. It represents lower expected survivors
through `C_red`; the code does not subtract the raw survivor count.

The default stages and coefficients live in [`AdaptiveConfig`](../src/strategies/adaptive.hpp):

| Stage | Condition after singleton/final checks | Information | Distance | Likelihood | Risk | Reduction |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `many` | `N > 500` | 0.65 | 0.20 | 0.05 | 0.10 | 0.10 |
| `moderate` | `50 < N <= 500` | 0.40 | 0.15 | 0.10 | 0.20 | 0.35 |
| `small` | `5 < N <= 50` | 0.15 | 0.05 | 0.45 | 0.65 | 0.35 |
| `tiny` | `2 <= N <= 5` | 0.05 | 0.00 | 0.80 | 1.00 | 0.15 |
| `single` / `final` | `N = 1`, or otherwise `r = 1` | 0 | 0 | 1 | 0 | 0 |

The tiny stage evaluates candidates only, including candidates outside the normal shortlist. The
other nonfinal stages can consider probes. Adaptive ties are resolved by higher answer probability
and then alphabetical order.

The thresholds and coefficients are configurable, validated heuristics. They are not fitted
constants or a claim of optimal play. The denominator includes the risk coefficient even when a
particular guess has zero risk. For example, the `many` coefficients total `1.10`, so they should
not be read as literal percentages of a final probability.

### What risk means here

Risk is a **probe penalty**, not the probability of an incorrect guess or a lost game. Every
candidate gets risk zero, whether its answer probability is 80% or 1%. Likelihood handles that
distinction separately.

A probe gets risk `1/6` with six turns left, `1/5` with five, and `1/2` with two. With one turn left
it is excluded altogether. Frequent zero values in a played game's analysis simply mean the selected
words remained possible answers. Probes may still have been evaluated and ranked below them.

## Reading a recorded recommendation

A saved second-turn analysis for `TRIED` records 607 candidates and five turns remaining. Its
metrics describe the decision **before** the recorded feedback `ggxyx` was submitted:

| Field | Recorded value | Interpretation |
| --- | ---: | --- |
| Entropy | `4.57363289728` bits | Uniform-bucket information. |
| Expected candidates | `44.4596375618` | Uniform average after this guess, not a promised next count. |
| Worst case | `120` | Largest possible feedback bucket. |
| Distance | `0.429353907384` | Average fraction of differing feedback marks across pairs. |
| Answer probability | `0.0167156233594` | About 1.67% under the current frequency priors. |
| Risk | `0` | `TRIED` was still a candidate. |

Since `607 > 500`, the stage is `many`. The entropy bound is `min(log2(607), 5*log2(3)) = 7.9248125`
bits. The normalized values in the export are:

```text
I     = 0.577128215361
Q     = 0.0677570093458
C_red = 0.928284426466
```

The likelihood component `Q` is smaller than one because another candidate has a higher prior. The
displayed 1.67% is raw probability; `Q` is its ratio to the largest candidate probability.

Substituting the exported values reproduces the score:

```text
(0.65*0.577128215361 + 0.20*0.429353907384
 + 0.05*0.0677570093458 - 0.10*0
 + 0.10*0.928284426466) / 1.10
= 0.506564013251
```

That score is usefulness under these weights and the evaluated pool. It is not a 50.66% chance of
solving. Nor does the expected survivor count guarantee that a particular feedback row will leave
roughly 44 words.

## Cost of a recommendation

The dedicated [time and space analysis](complexity.md) derives the per-strategy costs, dictionary
preparation, temporary storage, and whole-game/benchmark bounds, with links to the implementation.

For five-letter adaptive scoring, evaluating `K` guesses against `N` candidates costs `O(KN)`.
The complete recommendation also sorts the valid dictionary and ranks priors. With `M` valid
words and a prior map of size `O(M)`, its upper bound is `O(M log(M + 1) + KN)`.

The default 100-word cap limits scoring work, not dictionary preparation. Feedback buckets are
processed one guess at a time, so the scorer does not store a `K` by `N` feedback matrix. See the
[space breakdown](complexity.md#space-for-one-recommendation-and-session) for the full accounting.

## What the tests establish

[Scoring tests](../tests/test_scoring.cpp) verify hand-counted buckets, duplicate-aware letter counts, positional counts, fixed-strategy ordering, and agreement with the independent entropy/distance functions. [Adaptive tests](../tests/test_adaptive.cpp) check stage boundaries, configurable weights, normalized components, urgency, candidate-only endgames, and the large final-turn shortlist. [Benchmark tests](../tests/test_benchmark.cpp) check common endgame behavior, candidate/probe accounting, and reproducibility.

These tests establish implementation properties and arithmetic on controlled cases. They do not
establish that the adaptive heuristic outperforms another strategy. That question belongs to
measured, clearly configured [benchmarks](benchmarks.md).
