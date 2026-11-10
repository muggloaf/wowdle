# Time and space complexity

Wowdle has a definable cost for choosing a guess, updating candidates, playing a game, and running
a benchmark. These measure computation and storage. They do not predict how many guesses a
particular strategy will need to find an answer.

The adaptive and partition-based recommendation paths build a shortlist, then evaluate each
shortlisted guess against every surviving answer. The shortlist limits scoring work; its
construction still visits and sorts the dictionaries.

[Notation](#notation-and-assumptions) · [Core operations](#core-operations) · [Strategies](#strategy-costs) · [Full recommendation](#a-complete-recommendation) · [Space](#space-for-one-recommendation-and-session) · [Games and benchmarks](#whole-games-and-benchmarks) · [Report summary](#report-ready-summary)

For the score definitions, see [Algorithms](algorithms.md). For the file map, see
[Code and architecture](architecture.md). Build and play instructions are in the [guide](guide.md).

## Notation and assumptions

| Symbol | Meaning |
| --- | --- |
| `N` | Candidate answers remaining at the current turn |
| `N0` | Initial number of possible answers |
| `M` | Number of words in the valid-guess dictionary |
| `W` | Number of entries in the prior-weight map |
| `K` | Distinct guesses actually scored this turn, after any endgame override |
| `L` | Word length |
| `B` | Maximum number of nonempty feedback buckets for an evaluated guess |
| `T` | Turn limit for one game |
| `G` | Games per strategy in a benchmark |
| `S` | Strategies included in a benchmark |

Every answer must also be a valid guess, so `N <= N0 <= M`. The bucket bound is
`B <= min(N, 3^L)`. The implementation stores only observed patterns; it does not allocate or
enumerate all `3^L` patterns.

The default extension has `L = 5`, `T = 6`, and a shortlist limit of 100. Tiny states score all
remaining candidates, at most five by default. A final-turn state scores at most five candidates.
Thus, `K` is the actual evaluated count, not automatically 100 in every state.

The analysis uses the usual word-RAM model: counters, floating-point operations, random-engine
steps, and component weights have constant cost. The alphabet has 26 letters. A word copy or a
worst-case string comparison costs `O(L)`. Map and set bounds include those string comparisons.
Bounds concern valid inputs and worst-case computational work, not network or animation delays.

Unless stated otherwise, word-map keys have length `O(L)`. If a custom frequency file contains
longer keys, its storage is proportional to the actual total key length instead of simply `WL`.
All logarithms below use `log(x + 1)` where needed to cover empty maps and singleton sets.

## Core operations

### Feedback: `O(L)` time

[`evaluate_feedback`](../src/core/feedback.cpp) makes two passes through a word. The first marks
greens and counts unmatched answer letters. The second allocates yellows from those counts.

```text
time:              O(L + 26) = O(L)
auxiliary space:   O(26) = O(1), excluding the result
returned feedback: O(L)
```

Duplicates do not create nested searches. They use the same fixed-size count array.

### Candidate filtering: `O(NL)` time

[`filter_candidates`](../src/core/candidate_filter.cpp) simulates that feedback once per candidate
and compares it with the received pattern:

```text
time:                       O(NL)
temporary feedback storage: O(L)
returned candidate vector:  O(NL) in the worst case
```

The output contains copied strings, not just indices. While filtering, the old and new candidate
vectors can coexist. This changes the constant factor, not the `O(NL)` bound. If the returned vector
is excluded from auxiliary space, filtering itself needs only `O(L)` temporary space.

### Prior ranking

[`rank_candidates`](../src/probability/prior.cpp) looks up weights, normalizes them, and sorts the
result. With `N` candidates and `W` prior entries, a conservative bound is:

```text
time:   O(NL log(W + 1) + NL log(N + 1))
space:  O(NL) for the ranked words and sorting storage
```

The first term comes from `std::map` lookups. The second allows alphabetical string comparisons
when needed by callers' tie-breaking sorts. The probability-only sort in `rank_candidates` itself
compares numeric values. The bound safely covers both passes through the words and the additional
deduplication and tie ordering in `sorted_probabilities`.

The recommendation entry points also validate all supplied weight values, adding an `O(W)` scan.

### One feedback-bucket calculation

[`calculate_guess_metrics`](../src/strategies/scoring.cpp) evaluates one guess against `N`
candidates. For each answer it creates a length-`L` feedback string, updates a
`map<string, size_t>`, and increments one of three mark counters at each position.

A bucket insertion needs `O(log(B + 1))` tree comparisons. Each pattern comparison may inspect
`L` characters. Building the buckets therefore costs:

```text
O(NL(1 + log(B + 1))) time
```

Walking the completed buckets costs `O(B)`; aggregating the per-position counts costs `O(L)`.
Both are covered by the bound above. This single calculation returns entropy, expected survivors,
worst-case survivors, feedback Hamming separation, and bucket separation.

Its temporary space is:

```text
O(BL + L)
```

There are `B` stored pattern strings of length `L`, three counters per position, and one temporary
feedback vector. Input candidates are not copied by this function.

## Strategy costs

The table gives the work of the objective itself, before the common prior/shortlist preparation
and final recommendation sorting. These functions are in
[`scoring.cpp`](../src/strategies/scoring.cpp),
[`adaptive.cpp`](../src/strategies/adaptive.cpp), and
[`benchmark.cpp`](../src/evaluation/benchmark.cpp).

| Strategy | Objective work | Reason |
| --- | --- | --- |
| Random | `O(NL)` | Copy candidate words, shuffle them, and build their ordering; the shuffle alone is `O(N)` |
| Letter frequency | `O(NL + KL)` | Count distinct letters per candidate once, then score each guess |
| Positional frequency | `O(NL + KL)` | Build per-position letter counts once, then score each guess |
| Entropy | `O(KNL(1 + log(B + 1)))` | Build feedback buckets for every evaluated guess |
| Expected candidates | `O(KNL(1 + log(B + 1)))` | Use the same bucket construction and sum squared bucket sizes |
| Minimax | `O(KNL(1 + log(B + 1)))` | Use the same buckets and find the largest |
| Feedback distance | `O(KNL(1 + log(B + 1)))` | Build buckets and per-position mark counts |
| Adaptive | `O(KNL(1 + log(B + 1)))` | Calculate the shared metrics and combine normalized components |

Frequency and positional scoring do not recalculate candidate frequencies separately for each
guess in the ranked path. The standalone single-guess functions do build their own counts.

Sorting `K` scored suggestions, including string tie-breaking and duplicate tracking, adds a
conservative `O(KL log(K + 1))`. The random ordering is already determined by its shuffle.

Adaptive's candidate prior ranking and validation are accounted for in the preparation cost
below. Stage selection has a fixed number of comparisons. Applying five weights is constant work
per guess. [`urgency_adjusted_risk`](../src/strategies/risk.cpp) searches candidates linearly, so
all membership checks can add `O(KNL)`; that is absorbed by the existing metric bound.

### Displayed analysis is additional work

The complete evaluator also calculates full bucket metrics for up to five returned suggestions,
including random, frequency, and positional recommendations. For `D <= 5` displayed words, that
adds `O(DNL(1 + log(B + 1)))`.

Consequently, the random and frequency results in the benchmark include more work than their
named objectives alone. Their reported runtime is not a timing of just a shuffle or letter count.

### Why the active distance calculation is not quadratic in candidates

The reference [`average_feedback_separation`](../src/strategies/distance.cpp) visits every
candidate pair, costing `O(N^2 L)` **per guess**. It does not store an `N` by `N` distance matrix;
its temporary feedback storage is `O(L)`.

The active scorer instead counts how many feedback marks agree at each position. From those
counts it calculates the number of disagreeing pairs exactly. That takes `O(NL)` feedback/counting
work per guess, with map insertion adding the bucket factor already shown. It does not enumerate
the pairs. The derivation is in [the distance notes](algorithms.md#distance-without-enumerating-every-pair).

## A complete recommendation

### Preparing the pool still touches the full dictionaries

[`make_guess_pool`](../src/strategies/adaptive.cpp) validates words, deduplicates them, looks up
weights, sorts candidates and valid guesses, and fills the shortlist. Adaptive ranking performs
some candidate ranking again. A conservative bound covering this shared preparation is:

```text
P = O(W + L[(M + N) log(W + 1)
            + M log(M + 1)
            + N log(N + 1)])
```

The `W` term is prior validation. Map lookups give the `log(W + 1)` term. String-keyed sets and
sorting give the `M log(M + 1)` and `N log(N + 1)` terms.

Reducing `K` does not remove this work. The current code sorts the full valid dictionary again on
each recommendation, rather than caching a global prior ordering.

### Adaptive and partition-based recommendations

Combining preparation, metric calculations, and result sorting gives:

```text
R = O(P + KNL(1 + log(B + 1)) + KL log(K + 1))
```

This is the bound for the current adaptive/partition-based path, not a claim that the cheaper
letter-frequency objective intrinsically requires bucket scoring. It includes candidate risk
lookups. Filtering received feedback adds `O(NL)` after the choice is played.

### Simplifying for the five-letter implementation

With `L = 5`, there are at most `243` feedback patterns. Both word length and the bucket-map factor
are bounded constants. Assuming `W = O(M)`, and using `N <= M` and `K <= M + N`:

```text
metric scoring:         O(KN)
complete recommendation: O(M log(M + 1) + KN)
```

| Configuration | Scoring | Complete recommendation |
| --- | --- | --- |
| Default shortlist, `K <= 100` | `O(N)` | `O(M log(M + 1) + N)` |
| Full pool, `K` of order `M` | `O(MN)` | `O(M log(M + 1) + MN)` |
| Full first-turn search, `N0` of order `M` | `O(M^2)` | `O(M^2)` |

For the shipped dictionary, scoring 100 guesses against 16,131 answers requires approximately
**1.61 million guess-answer feedback evaluations**. Evaluating all 16,131 guesses would require
approximately **260 million**. These are operation counts, not measured milliseconds.

### Endgame overrides

The tiny stage scores at most five candidates by default. The final-turn rule sorts by prior
probability and computes metrics for only the five displayed candidates. A singleton needs one
candidate's analysis. However, their callers still build the guess pool, so these shortcuts do
not make the complete recommendation `O(1)`.

## Space for one recommendation and session

Time spent evaluating `K` guesses does not imply storing `K` copies of every feedback bucket.
The scorer finishes one guess, destroys its temporary buckets, and moves on to the next.

| Data | Space |
| --- | --- |
| Input answer and valid-guess words | `O((N0 + M)L)` |
| Prior map | `O(WL)` when keys have length `O(L)` |
| Current candidates | `O(NL)` |
| Deduplicated/sorted words during pool construction | `O((M + N)L)` |
| Pool and ranked guess analyses | `O(KL)` |
| One guess's temporary metrics | `O(BL + L)` |
| Accepted game history | `O(TL)` |

A conservative upper bound for **auxiliary space during one complete recommendation**, excluding
the input dictionaries/prior map and existing history, is:

```text
O((M + N + K + B)L)
```

The resident data plus peak single-session workspace are bounded by:

```text
O((M + N0 + W + K + T)L)
```

Here `B <= N <= N0`, so the bucket term is absorbed. With five letters, six turns, the default
shortlist, `N0 <= M`, and `W = O(M)`, total logical session space simplifies to **`O(M)`**.

The frequency ranker itself needs `O(KL + 26)` result/counting space; positional ranking needs
`O(KL + 26L)`. The bucket-based rankers need `O(KL + BL + L)` once their inputs are available.
Random's copied/shuffled ordering uses `O(NL)`. Adaptive additionally holds candidate probability
and ranking structures, covered by the complete recommendation bound.

These bounds include strings and container entries, not a byte estimate for a particular STL.
Tree nodes, vector capacity, sorting scratch space, and string representation affect actual
memory. Browser process memory, DOM nodes, and reserved WASM heap capacity are also outside this
logical solver-space count.

There is no stored `K x N` feedback matrix and no active `N x N` distance matrix. A capped scoring
pass can take millions of feedback evaluations while retaining only the current guess's buckets.

## Whole games and benchmarks

### One game

Let `R_t` be the complete recommendation cost at turn `t`, and `N_t`, `M_t` the candidate and
available-guess counts at that turn. The game cannot exceed its turn limit, but no fixed candidate
reduction factor is guaranteed. The general cost is a sum over the actual states.

The native [`ComputerGame`](../src/evaluation/benchmark.cpp) also copies its allowed dictionary
on construction and checks guess validity by linear search. `play_game` copies the initial
candidates and available guesses; removing a played word can scan the available list.

A conservative native-game bound is:

```text
O((N0 + M)L + sum over turns t of [R_t + (M + N_t)L])
```

The linear terms include game validation, candidate membership, filtering, and removal of played
guesses. `M` is a safe bound on `M_t`. Game construction's dictionary copy contributes another
`O(ML)`, already absorbed by the expression. Some of these operations are outside the benchmark's
per-game timer; the accounting is explained in [Benchmarks](benchmarks.md#runtime-accounting).

If `R_max` bounds a recommendation throughout the game, this is at most:

```text
O((N0 + M)L + T[R_max + (M + N0)L])
```

Six turns are a constant multiplier in the default game. Practical games often get cheaper as
`N_t` shrinks, but that observation does not justify assuming `N_t = N0 / 2^t`, or claiming a
logarithmic number of turns.

History adds `O(TL)` space. Per-turn candidate and scoring workspaces are replaced or reused, not
retained for every turn, so the working dictionary space is not multiplied by `T`.

### Replaying browser history

[`wowdle_restore`](../src/wasm/solver_bridge.cpp) starts with the original answers, filters each
historical row, and calculates recommendations only for the final restored state. For `H <= T`
rows, replay costs `O(L * sum of replay candidate counts)`, bounded by `O(HN0L)`, plus the final
recommendation. Its candidate/history storage is `O(N0L + HL)`, plus recommendation workspace.

Undo and board synchronization use this replay path. They do not retain every historical
candidate set or compute inverse filters.

### A benchmark batch

For `S` strategies, `G` games each, and at most `T` turns, a coarse computational upper bound is:

```text
O(SGT[R_max + (M + N0)L] + SG log(G + 1))
```

This covers game construction/copies and per-turn work. The final term comes from sorting `G`
attempt counts for each strategy's median. Initial dictionary loading and one-time validation
also have input-reading/set costs. The bound above conservatively counts opening scoring for
each game; in the implementation, each nonrandom strategy calculates its opening once and reuses
it across the batch. Random generates a fresh choice in each game.

The batch runs one game at a time, but [`run_benchmark`](../src/evaluation/benchmark.cpp) keeps
every `GameResult` for the returned reports. Each holds up to `T` played words and feedback
strings. Consequently, retained result storage is:

```text
O(SGT(L + 1))
```

There are also `O(G)` sampled-answer indices and `O(G)` temporary attempt counts during summary
calculation. Add those to the input dictionaries/prior map and **one** active game's workspace.
The game workspace is not multiplied by `G`; the saved results are.

In the CLI file-stream path, trace CSV rows are written to disk rather than accumulated as a
second in-memory table. The trace occupies disk space proportional to the turns and row width.
A caller supplying an in-memory stream instead would also retain that text in RAM. This
distinction matters when increasing a batch from 100 to 10,000 games.

## Report-ready summary

For the five-letter adaptive implementation, assuming the prior map has size proportional to the
valid dictionary:

```text
Per recommendation:
    time  O(M log(M + 1) + KN)
    space O(M) for the default capped, single-session configuration

With the default K <= 100:
    scoring alone is O(N); full-dictionary preparation still costs O(M log(M + 1))

With exhaustive K of order M and first-turn N of order M:
    time O(M^2), while session storage remains O(M)
```

These statements describe one recommendation's computation and one session's stored data. A
whole game adds the turn-wise sum; a benchmark adds its games, strategies, and retained histories.
The adaptive controller performs one-step scoring at each turn. It does not search every future
game tree, and it does not assume that candidate sets shrink at a fixed rate.
