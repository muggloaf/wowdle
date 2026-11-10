# wowdle

A five-letter Wordle solver in C++, with a Chrome side panel. C++ handles feedback,
candidate filtering, probabilities, scoring, and game state. JavaScript handles
the interface and reads or enters guesses on the visible game board.

## Build and test

Requires a C++17 compiler (`g++`), PowerShell, and Node.js 18 or newer for the
extension tests. Run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
```

Rebuild the WebAssembly backend after C++ changes. The script uses `em++` from
PATH, or the existing `../emsdk/upstream/emscripten/em++.exe` installation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_wasm.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Test
```

`-Test` runs the core, strategy, benchmark, bridge, browser-adapter, interface,
and compiled-WASM suites. The WASM integration tests load the actual generated
loader and binary; rebuild those artifacts before testing a C++ change.
Test executables go in `.build/`. Both build scripts check compiler failures.

## Chrome extension

1. Open `chrome://extensions` and enable Developer mode.
2. Load unpacked, selecting this repository's `extension` folder.
3. Open a five-letter game at `https://wordleunlimited.com/`,
   `https://wordleunlimited.org/`, or NYT Wordle.
4. Click the Wowdle toolbar icon to open its side panel.
5. Reload the extension after updating its source or generated WASM files.

**Manual:** play a suggested word, enter five `g`, `y`, or `x` feedback marks,
then submit. Click a suggestion to use it, or press `c` and choose its number.
The guess field also accepts a word played outside the recommendations.
Undo removes the last turn. Manual history survives reopening the side panel.
Reset starts a fresh solver session.

**Auto:** selecting auto reads the visible board and replays completed turns in
C++. Review the recommendation, then press Play to enter **one** guess and read
its colors. Press Play again for the following turn. Sync board catches up after
you play in the game yourself. If the board changes before Play, the solver syncs
and waits for another Play click so you can review the new guess. Reset affects
the solver; start a new puzzle in the game tab before syncing a new game.

Auto supports `.com`'s WordPlay board, `.org`'s legacy board, and modern/legacy
Wordle tiles, including open shadow roots. It checks the exact played history and row letters,
waits for color animations, and reports rejected guesses. Clear a rejected or
partially typed word in the game before choosing a different suggestion. An
unsupported layout produces an error. The extension never reads the secret
answer or the game's internal answer state.

Adapter behavior is tested against DOM fixtures derived from the public site
implementation. `.org` also has an optional live headless Chrome check; a site
update can change its DOM. The C++ backend fails visibly if it cannot load; no
mock guesses or simulated candidate counts are used.

The analysis panel shows real metrics and the guess type (candidate or
information-only) for the selected suggestion, active
weights, likely answers with probabilities, and played turns. Download analysis
exports the session and recorded turn decisions as JSON. A custom word outside
the suggested list has no precomputed analysis, so those fields show a dash.
Reopened or board-synced sessions retain their history; earlier decision metrics
are not reconstructed.

For a manual browser preview, serve the repository and open the extension page:

```powershell
python -m http.server 8000 --bind 127.0.0.1
```

Open `http://localhost:8000/extension/index.html`. Auto requires the installed
extension. The preview still uses the real C++ WASM backend.

To rerun the live `.org` check with an installed Chrome browser:

```powershell
npm install --prefix .build/browser-check --no-save --package-lock=false playwright-core
node tools/check_unlimited.cjs
```

This opens an isolated headless browser, checks board sync and rejection handling,
then plays a real game using the compiled C++ solver and visible tile feedback.
It does not use your browser profile or inspect the hidden answer.

## CLI and experiments

```powershell
.\wowdle.exe
.\wowdle.exe --computer --seed 42
.\wowdle.exe --benchmark 100 --strategy all --seed 42 --csv results.csv --trace turns.csv
.\wowdle.exe --benchmark 1000 --strategy adaptive --seed 42
.\wowdle.exe --benchmark 10000 --strategy entropy --seed 42
.\wowdle.exe --help
```

Strategies: `random`, `frequency`, `positional`, `entropy`, `expected`, `minimax`,
`distance`, and `adaptive`. All modes default to six guesses. Manual input accepts
spaced feedback, `c` to select another displayed suggestion, and `q` or EOF to quit.
Invalid or contradictory feedback leaves the current candidates and turn intact.
Computer mode keeps the secret in the game engine; strategy functions receive
only candidate words, remaining turns, and prior weights.

Use separate answer and valid-guess dictionaries when you have them:

```powershell
.\wowdle.exe --benchmark 100 --strategy all --answers data/my_answers.txt --guesses data/my_guesses.txt --uniform
```

Every answer must also be a valid guess. Without `--guesses`, the answer dictionary
is used for both roles. The CLI and core accept other equal-length alphabetic word
lists; the extension is intentionally five-letter only. Data files are not edited
by the loaders. The shipped list has 16,152 rows, of which 21 punctuation entries
are skipped, leaving 16,131 usable words. It contains obscure words, proper names,
and words an external game may reject; it is not a curated official answer list.

The default guess pool is a **100-word heuristic shortlist**, shared by CLI,
extension, and benchmarks. It reserves half the places for likely candidates,
then fills from likely valid guesses, allowing information-only probes. It is
not an exhaustive optimum. `--pool 0` evaluates every available word and is much
slower. Played words are removed from future CLI guess pools. All strategies
guess the sole candidate directly; on their last turn they choose the candidate
with highest prior probability, including the random baseline. These shared
endgame rules are part of the benchmark definition.

Benchmarks sample answers **uniformly with replacement**, reuse the same answer
sample for each strategy, and use a separate seeded guess generator. Frequency
priors guide the solver but do not change the answer sampling distribution. The
summary reports mean and median attempts across all games including failures,
mean guesses among wins, win/failure rates, worst attempts, solver runtime per
game, and candidate/probe counts. With no wins, the winning mean is `n/a` (blank
in CSV). Runtime includes strategy calculation, filtering, and trace writing;
fixed-strategy opening scores are calculated once and amortized over the batch.
Runtime is machine-specific and excludes initial dictionary loading.

Keep the command, dictionaries, seed, pool size, priors, and adaptive settings
with experiment results. `--trace` records every turn's candidate count, turns
left, selected guess/stage, raw metrics, normalized components, and weights.
No strategy superiority or distance novelty is assumed; the program generates
the measurements needed to compare them.

## Adaptive score

The adaptive strategy combines:

```text
(information_weight * normalized_entropy
 + distance_weight * feedback_separation
 + likelihood_weight * relative_answer_probability
 + reduction_weight * normalized_candidate_reduction
 - risk_weight * urgency_adjusted_risk) / sum_of_weights
```

Feedback buckets use a uniform distribution over surviving candidates, matching
the entropy baseline. Word-frequency priors drive answer likelihood and final
guess selection. These are deliberately separate quantities.

| Component | Definition |
| --- | --- |
| Entropy | Shannon entropy of feedback buckets, in bits |
| Expected candidates | `sum(bucket_size²) / candidate_count`, including the solved bucket |
| Minimax | Largest feedback bucket |
| Feedback separation | Mean pairwise Hamming distance between feedback vectors, divided by word length |
| Candidate reduction | `(N - expected_candidates) / (N - 1)` for `N > 1` |
| Information score | Entropy divided by `min(log2(N), word_length * log2(3))` |
| Likelihood score | Candidate probability divided by the largest candidate probability |
| Risk | Zero for candidates; `1 / guesses_remaining` for probes |

Distance is computed exactly from per-position feedback counts, avoiding a
quadratic loop over answer pairs. The existing standalone weighted-feedback and
bucket-separation metrics remain available for further experiments.

Default stages and weights, in order **information, distance, likelihood, risk,
reduction**:

| Stage | Candidates | Weights |
| --- | --- | --- |
| many | `> 500` | `0.65, 0.20, 0.05, 0.10, 0.10` |
| moderate | `51–500` | `0.40, 0.15, 0.10, 0.20, 0.35` |
| small | `6–50` | `0.15, 0.05, 0.45, 0.65, 0.35` |
| tiny | `2–5` | `0.05, 0.00, 0.80, 1.00, 0.15`; candidates only |
| single | `1` | Guess that answer |
| final | One turn left | Highest-probability candidate |

Thresholds and weights are experimental settings, not established constants.
Override them through `AdaptiveConfig` in C++, or in the CLI:

```powershell
.\wowdle.exe --benchmark 100 --strategy adaptive --thresholds 400,40,4 --weights small:0.1,0.1,0.5,0.7,0.3
```

## Data and layout

Probabilities are normalized `wordfreq` weights among surviving candidates,
with a tiny fallback for missing/zero-frequency words. They estimate corpus
likelihood, not an official Wordle answer-selection probability. To regenerate:

```powershell
python -m pip install -r requirements.txt
python tools/generate_frequency_data.py
```

C++ has no Python runtime dependency. Candidate filtering always compares exact
simulated feedback, including duplicate letters; priors never override it.

```text
src/core/          duplicate-safe feedback and candidate filtering
src/probability/   frequency loading and candidate probabilities
src/strategies/    existing metrics, baseline scoring, adaptive controller
src/evaluation/    isolated computer game, experiments, CSV traces
src/wasm/          validated C interface, JSON state, transactional history replay
extension/         side-panel UI, WASM adapter, visible-board integration
tests/             C++ suites, DOM/UI fixtures, compiled-WASM integration
tools/             native/WASM builds and frequency generation
```
