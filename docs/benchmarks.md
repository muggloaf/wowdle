# Benchmarks

The recorded 100-game comparison gives **94 wins for adaptive and 99 for expected remaining
candidates**. The adaptive defaults did not beat the strongest baselines in this run. These are
measurements from the program, using the shipped dictionary and one seed; they are a starting point
for experiments.

The run was recorded on **10 November 2026**, with seed **42**, six guesses per game, and a
guess-pool limit of **100**. Each of the eight strategies played the same 100 sampled answers: **800
games and 3,701 turns** altogether. Sampling was uniform with replacement; these 100 draws contained
**98 distinct answers**. This was a sample from the full dictionary, not an exhaustive test of every
word.

## Results

Each strategy played 100 games, so its win count also gives its win percentage. Failed games used
all six guesses. Values below come from the preserved [summary CSV](results/seed42-summary.csv);
winning means are rounded to three decimals here.

| Strategy | Wins | Failures | Mean, all games | Median, all games | Mean, wins only | Worst |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Random | 90 | 10 | 4.81 | 5 | 4.678 | 6 |
| Letter frequency | 97 | 3 | 4.53 | 4 | 4.485 | 6 |
| Positional frequency | 91 | 9 | 4.75 | 5 | 4.626 | 6 |
| Entropy | 98 | 2 | 4.46 | 4 | 4.429 | 6 |
| Expected remaining candidates | 99 | 1 | 4.44 | 4 | 4.424 | 6 |
| Minimax | 98 | 2 | 4.64 | 5 | 4.612 | 6 |
| Distance | 99 | 1 | 4.68 | 5 | 4.667 | 6 |
| Adaptive | 94 | 6 | 4.70 | 5 | 4.617 | 6 |

The all-games mean counts the attempts actually made, including six attempts for each failure. A
failed game is not scored as a hypothetical seventh guess. The median uses that same set of 100
attempt counts. The wins-only mean excludes failures, so it should be read alongside the win rate.
The worst count includes both wins and failures; a worst count of six does not mean every game was
solved within six guesses.

| Strategy | Mean runtime, ms/game | Candidate guesses | Information-only guesses | Total guesses |
| --- | ---: | ---: | ---: | ---: |
| Random | 348.54 | 481 | 0 | 481 |
| Letter frequency | 179.89 | 451 | 2 | 453 |
| Positional frequency | 192.67 | 466 | 9 | 475 |
| Entropy | 220.59 | 334 | 112 | 446 |
| Expected remaining candidates | 228.38 | 332 | 112 | 444 |
| Minimax | 244.51 | 331 | 133 | 464 |
| Distance | 264.82 | 358 | 110 | 468 |
| Adaptive | 252.20 | 456 | 14 | 470 |

A **candidate guess** belonged to the remaining possible-answer set immediately before it was
played. An **information-only guess**, or probe, did not. These are total turn counts across the 100
games, including failed games, rather than counts of games that used a probe.

Expected remaining candidates and distance each won 99 games. Expected remaining candidates used
fewer guesses on average: 4.44 against 4.68, counting all games. Adaptive used 14 probes, compared
with 112 for expected remaining candidates and entropy. That describes the decisions made by these
defaults; it does not establish that using fewer probes caused adaptive's six failures.

## What was compared

The answer and valid-guess inputs were both the shipped `data/wordlist_fives.txt`. It contains
16,152 tokens. Loading skips 21 entries containing punctuation, leaving **16,131 unique five-letter
alphabetic words**. This broad word list includes obscure words and is not an official curated
Wordle answer list. A word accepted by this benchmark may be rejected by a particular browser game.

Answer sampling was uniform, but the solver used `data/word_frequencies_fives.txt` for answer
probabilities and shortlist construction. English usage frequency is a prior estimate, not the
distribution used to draw the benchmark secrets. Exact feedback matching still determines which
candidates survive.

The seven scored strategies used the frequency-guided pool with a limit of 100. Pool construction
reserves up to half the slots for the most likely remaining candidates, adds highly ranked valid
guesses, then fills any remaining slots with candidates. Guesses already played are removed. This is
a heuristic shortlist, so an entropy or minimax result here is not a search over all 16,131 valid
guesses on every turn. Adaptive also restricts itself to candidates in its tiny-set stage.

Random normally chooses uniformly from the entire remaining candidate set, rather than from the
scored shortlist. All eight strategies share two endgame rules: guess a sole remaining candidate
immediately, and choose the highest-probability candidate on the final turn. Thus, the random result
includes that final-turn safeguard. Ties are resolved by the implementation, not by an extra source
of answer information.

The adaptive run used the default candidate thresholds of 500, 50 and 5 and the default stage
weights. Its actual stage, component scores and weights are recorded on each turn. See [the
algorithms document](algorithms.md) for score definitions and configuration details.

## Runtime accounting

These elapsed-time measurements are separate from the
[asymptotic time and space bounds](complexity.md). The bounds describe how work and storage grow;
the measurements include this implementation's constants and instrumentation.

These are native CLI measurements of this implementation's instrumented game loop. Each nonrandom
strategy computes its opening recommendation once and reuses it across the 100 games. The opening
computation is charged once to the first game, so the average includes one hundredth of that shared
cost. Random generates a fresh recommendation in each game.

The per-game timer includes recommendation calculations, feedback generation, filtering, guess
bookkeeping and trace writes within the loop. Dictionary loading, initial candidate/guess vector
copies, game construction and final summary output happen outside that timer. The recommendation
path also calculates analysis metrics, including for random. Its runtime therefore includes work
that a minimal random solver would not need.

These numbers describe this CLI run, not browser-extension latency or the speed of an isolated
mathematical objective. Compiler, standard-library and hardware details were not saved with the
original artifacts, and the trace does not contain per-game timings. The original runtime summary is
preserved as recorded.

## Saved evidence and checks

The versioned artifacts are:

- [Summary CSV](results/seed42-summary.csv): the original eight summary rows, without rounding or edits.
- [Compressed turn trace](results/seed42-turns.csv.gz): all 3,701 original turn rows. Gzip reduces the 529,034-byte trace to 108,125 bytes; decompression restores the original bytes.
- [Recorded command](results/seed42-command.txt): the original command and defaults note.
- [Metadata JSON](results/seed42-metadata.json): settings, the sampled answers derived from solved traces, artifact hashes and the input/source snapshot hashes collected during documentation.

The hashes identify the files available when the completed run was audited. No contemporaneous
source or compiler archive was recorded at benchmark start, so the metadata does not claim one.

The audit reconstructed each game's candidates with duplicate-safe feedback and verified all 800
games against the shipped dictionary. It checked every trace candidate count, candidate/probe
classification, risk and answer probability; confirmed that all strategies faced the same answer for
each game; and recomputed the reported game counts, win/failure rates, means, medians and worst
counts. Every failed game exhausted six turns, and no game repeated a guess. At least one strategy
solved each sampled answer, allowing the common answer sequence to be checked against every
strategy's feedback. Summary runtimes cannot be independently reconstructed from the turn trace.

The automated tests also cover small, manually checkable metric calculations, duplicate letters,
game budgets, deterministic choices for a fixed seed, shared answer samples and endgame behavior.
These checks support the implementation and accounting; they do not establish how a strategy
performs on other dictionaries or answer distributions.

## Reproduce and extend

After building the CLI as described in [the usage guide](guide.md), the recorded comparison can be
run from the repository root with:

```powershell
New-Item -ItemType Directory -Force benchmark_results | Out-Null
.\.build\wowdle.exe --benchmark 100 --strategy all --seed 42 --csv benchmark_results/full_dictionary_seed42_summary.csv --trace benchmark_results/full_dictionary_seed42_turns.csv
```

The executable now builds under `.build/`; its arguments match the recorded command.
The archived command preserves the original executable location. Use a new filename to retain earlier experiments. `--pool 0`
evaluates the full available guess pool for scored strategies and changes the experiment.
`--uniform` removes frequency-based priors and also changes shortlist ordering. Larger batches,
different seeds, separate answer/guess lists, thresholds and stage weights are described in the
guide.

Seeded results are repeatable with the same inputs, implementation and random-distribution behavior.
C++ standard-library implementations can differ in how they map random-engine outputs to samples and
shuffles, so the same numeric seed alone is not a portable guarantee of the same games. Runtime will
vary between runs.

This is one seed with no confidence intervals or repeated timing trials. It does not establish
statistical significance, a universal strategy ranking, a validated choice of adaptive weights, or a
novel algorithm. A useful next experiment would compare several seeds and larger held-out samples
while recording the full setup and keeping the candidate dictionary, guess pool and endgame policy
fixed. Parameter tuning and final evaluation should use separate samples.
