# Build and use Wowdle

Wowdle has a native C++ command-line program and a Chrome side panel backed by
the same C++ solver compiled to WebAssembly. The extension supports five-letter
games with six turns. The CLI and core also accept dictionaries of other word
lengths, provided every answer and valid guess has the same length.

See [Algorithms](algorithms.md) for scoring, [Code and architecture](architecture.md)
for the implementation map, and [Benchmarks](benchmarks.md) for experiments.

[Build](#build-and-test-on-windows) · [Install](#install-or-update-the-extension) · [Manual play](#play-in-manual-mode) · [Auto play](#play-in-auto-mode) · [CLI](#run-the-cli) · [Troubleshooting](#troubleshooting)

## Requirements

| What you want to do | Requirements |
| --- | --- |
| Use the existing unpacked extension | Chrome 116 or newer; the generated files in `extension/` |
| Build the native program | PowerShell and a C++17 `g++` compiler on `PATH` |
| Run the automated test suites | The native build tools and Node.js 18 or newer |
| Rebuild the extension's C++ backend | Emscripten, with `em++` available as described below |
| Serve a browser preview | Python 3; no Python packages are needed |
| Regenerate frequency data | Python 3 and the packages in `tools/requirements.txt` |
| Run the optional live site check | Installed Chrome, Node.js 20 or newer/npm, and the optional Playwright dependency |

Python is not required by the native solver or installed extension. Emscripten
is needed only when rebuilding WebAssembly. The repository includes
`extension/wowdle.js`, `extension/wowdle.wasm`, and copied dictionary data; if you
trust those current artifacts, you can load the extension without compiling it.

## Build and test on Windows

Run these commands in PowerShell from the repository root, the folder containing `README.md`.
The execution-policy option applies to the launched process; it does not change machine policy.

Build the native executable:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
```

This compiles the CLI and its supporting C++17 sources into `.build/wowdle.exe`.
Native executables and test binaries stay in the ignored `.build/` directory.

After changing C++ code used by the extension, rebuild its backend:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_wasm.ps1
```

The WASM script first looks for `em++` on `PATH`, then for the existing sibling
installation at `..\emsdk\upstream\emscripten\em++.exe`. Activate your Emscripten
environment before running the script if it is installed elsewhere. The script
regenerates `extension/wowdle.js` and `extension/wowdle.wasm`, and copies both
data files from `data/` into `extension/data/`.

Run the complete regression suite after rebuilding WASM:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Test
```

`-Test` rebuilds the CLI and runs the C++ feedback, filtering, probability, entropy,
distance, risk, scoring, adaptive, benchmark, and bridge tests, then the Node
browser-adapter, interface, and compiled-WASM tests. Test executables go in `.build/`.
WASM tests use the generated loader and binary; old artifacts do not validate a new C++ change.

For a focused extension check:

```powershell
node --test tests/test_content.js tests/test_extension.js
node --test tests/test_wasm.js
```

JavaScript, HTML, and CSS changes need an extension/page reload rather than a C++ rebuild.

## Install or update the extension

1. Open `chrome://extensions` and enable **Developer mode**.
2. Choose **Load unpacked** and select this repository's `extension` folder.
3. Open the game you want to play and click the Wowdle toolbar icon to open the
   side panel. Pin the icon through Chrome's extensions menu if needed.
4. Wait for `cpp solver bridge: connected` before entering feedback or playing.

After an update, click **Reload** on Wowdle's extension card, close and reopen
the side panel, and refresh the game tab. This loads the new UI, WASM module,
permissions, and content script. An already open game tab may still contain an
older content script until refreshed.

## Play in manual mode

Manual mode works with any external five-letter Wordle game because you enter
the feedback yourself.

1. Play the displayed guess in your game. Click another suggested word to
   select it, or edit the guess field to match a word you played yourself.
2. Enter one feedback mark per letter: `g` for green, `y` for yellow, and `x`
   for gray. The input accepts upper/lowercase marks and removes spaces.
3. Click **submit feedback**, or press Enter while the guess or feedback field
   has focus. Review the next guess and repeat.

For example, guessing `ALLEE` against `APPLE` produces `gyxxg`: the first `A`
and final `E` are green, one `L` is yellow, and the extra `L` and `E` are gray.
Copy the game's actual colors when a guess repeats letters; the solver handles
their counts through exact feedback simulation.

With focus in the side panel, press **c** to open the guess chooser, then press
**1–5** or click a displayed choice. The shortcut also works from the feedback
field. In the guess field, `c` remains an ordinary letter. Keyboard shortcuts
typed while the game tab has focus go to the game instead of Wowdle.

**Undo last turn** removes the most recent submitted feedback. Manual mode also
offers Undo on the completion screen, so an accidental all-green submission can
be corrected. Invalid or contradictory feedback does not consume a turn.
Undo changes the solver's history; it does not remove a played word from the
external game. Undo is disabled in auto mode.

Manual history is saved in the extension's local browser storage and restored
when the panel reopens. **Reset** or **solve another** starts an empty solver
session; in manual mode this also replaces the saved history. The mode switch
changes how feedback is entered, rather than creating two independent active
sessions. Auto reads the current game board when selected.

## Play in auto mode

Open one of these supported games as the active tab:

- [NYT Wordle](https://www.nytimes.com/games/wordle/index.html)
- [Wordle Unlimited / WordPlay](https://wordleunlimited.com/)
- [Wordle Unlimited](https://wordleunlimited.org/)

Use a five-letter, six-turn game in normal mode. The solver does not enforce an
external game's hard-mode restrictions, so hard mode can reject an otherwise
useful information-only guess. Close the site's instructions, welcome screen,
settings, and other dialogs before playing.

1. Select **auto**. Wowdle reads the visible board and replays its completed
   guesses and colors through the C++ solver.
2. Review the recommendation, or choose another suggestion.
3. Press **play this guess**. Wowdle enters one word, submits it, waits for the
   tile colors to finish, and updates its candidates.
4. Review the next recommendation and press Play again for the next turn.

Auto plays **one guess per Play action**. It is not a continuous unattended
loop. Arrow Right also triggers Play when the side panel has focus outside the
guess and feedback inputs. The manual feedback field is disabled in auto mode.

**Sync board** catches up after you play a word directly in the game. Play also
checks the board first. If completed history changed, Wowdle syncs and waits for
another Play action so you can review the revised recommendation. Before typing,
the content script checks the exact history and verifies the resulting row's
letters, rather than trusting only its row number.

If the current game row contains a different or partially typed word, clear it
in the game before pressing Play. After a rejected word, clear that word, let
the rejection message disappear, and select another guess. The supplied
dictionary includes words some sites do not accept.

Reset affects the solver only. To begin a new auto game, start a new puzzle in
the game tab, then reset or sync Wowdle. Auto reconstructs its state from visible
guesses and feedback; it does not read the hidden answer or internal answer
state. Auto updates do not overwrite the saved manual history.

## Read the analysis panel

Open **solver analysis** to inspect the selected suggestion:

| Field | Meaning |
| --- | --- |
| Entropy | Information in the guess's feedback buckets, in bits |
| Answer likelihood | That word's estimated probability among surviving answers |
| Expected candidates | Average size of the feedback bucket after this guess |
| Worst case | Size of its largest feedback bucket |
| Feedback separation | Average feedback-vector separation, normalized by word length |
| Guess type | `candidate` if it remains a possible answer; `information-only` otherwise |
| Combined score | The adaptive strategy's weighted score for guess usefulness |
| Remaining | Candidate count and turns left |

The panel also shows active component weights, ranked possible answers, and
played turns. Answer probability and guess score measure different things: an
information-only guess can have useful feedback while having zero answer
probability. Frequency-based probabilities are estimates from the supplied
corpus weights, not official Wordle answer-selection probabilities.

A custom word outside the displayed suggestions has no precomputed analysis;
its analysis fields show a dash. Numeric risk remains part of C++ scoring and
exported analysis even though the interface shows the simpler guess type.

**Download analysis** saves `wowdle-analysis.json` with the current history,
state, and recorded decisions. Reopening the panel or syncing a different board
history does not reconstruct earlier decision metrics. A custom guess has no
recorded suggestion analysis. Use CLI traces for complete experiment logs.

## Run the CLI

Start manual feedback mode, or run one computer-selected game:

```powershell
.\.build\wowdle.exe
.\.build\wowdle.exe --computer --seed 42
.\.build\wowdle.exe --help
```

The CLI defaults to adaptive scoring and six turns. Manual input accepts spaced
feedback such as `g y x x g`, `c` followed by a displayed suggestion number, and `q`
or EOF to stop. Invalid feedback can be retried without advancing the turn. Computer
mode keeps the secret inside the game engine and supplies only feedback to the solver.

Compare all eight strategies and write reproducible output files:

```powershell
New-Item -ItemType Directory -Force benchmark_results | Out-Null
.\.build\wowdle.exe --benchmark 100 --strategy all --seed 42 --csv benchmark_results/results.csv --trace benchmark_results/turns.csv
.\.build\wowdle.exe --benchmark 1000 --strategy adaptive --seed 42
.\.build\wowdle.exe --benchmark 10000 --strategy entropy --seed 42
```

Strategy names are `random`, `frequency`, `positional`, `entropy`, `expected`,
`minimax`, `distance`, and `adaptive`. `all` is available only with `--benchmark`.
The larger examples are optional; runtime depends on strategy, dictionary, pool,
and machine. See [Benchmarks](benchmarks.md) before comparing results.

### CLI options

| Option | Purpose |
| --- | --- |
| `--answers PATH` | Possible answers; defaults to `data/wordlist_fives.txt` |
| `--guesses PATH` | Valid guesses; must contain every answer; defaults to the answer list |
| `--frequencies PATH` | Prior weights; defaults to `data/word_frequencies_fives.txt` |
| `--uniform` | Use equal probabilities instead of frequency weights |
| `--seed N` | Random seed, including zero; default `42` |
| `--max-guesses N` | Positive turn limit; default `6` |
| `--pool N` | Guess shortlist size; default `100`; `0` evaluates all available guesses |
| `--csv PATH` | Summary CSV for benchmark or computer mode |
| `--trace PATH` | Per-turn metrics and weights, including manual CLI sessions |
| `--thresholds A,B,C` | Many, small, and tiny candidate thresholds; require `A > B > C >= 1` |
| `--weights STAGE:I,D,L,R,E` | Weights for information, distance, likelihood, risk, and candidate reduction |

Weight stages are `many`, `moderate`, `small`, and `tiny`. Weights must be finite,
nonnegative, and have a positive total. Repeat `--weights` to override several
stages. These CLI settings do not change the installed extension's configuration.

```powershell
New-Item -ItemType Directory -Force benchmark_results | Out-Null
.\.build\wowdle.exe --benchmark 100 --strategy all --answers data/my_answers.txt --guesses data/my_guesses.txt --uniform
.\.build\wowdle.exe --benchmark 100 --strategy adaptive --thresholds 400,40,4 --weights small:0.1,0.1,0.5,0.7,0.3
.\.build\wowdle.exe --answers data/my_small_list.txt --pool 0 --trace benchmark_results/manual-turns.csv
```

Word lists contain whitespace-separated alphabetic words of one common length.
The loaders normalize case, remove duplicates, and skip nonalphabetic entries.
Supply your own equal-length lists for non-five-letter CLI experiments. A lone
positional word-list path also selects the answer dictionary. A missing CLI
frequency file produces an explicit uniform-prior fallback message.

`--pool 0` can be much slower than the default heuristic shortlist. CSV and trace
outputs replace existing files; use distinct output paths. The CLI rejects
output paths that would overwrite a supplied input file. Preserve the command,
dictionaries, seed, priors, pool size, thresholds, and weights with your results.

## Optional preview, data generation, and live check

For a manual browser preview using the real WASM backend:

```powershell
python -m http.server 8000 --bind 127.0.0.1
```

Open `http://localhost:8000/extension/index.html`. Auto requires the installed
extension. Stop the preview server with Ctrl+C.

To regenerate the supplied frequency weights, then copy them into the extension:

```powershell
python -m pip install -r tools/requirements.txt
python tools/generate_frequency_data.py
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_wasm.ps1
```

The optional live `.org` check uses installed Chrome in an isolated headless
browser. Its dependency stays under `.build/browser-check/`:

```powershell
npm install --prefix .build/browser-check --no-save --package-lock=false playwright-core
node tools/check_unlimited.cjs
```

It checks board sync, the welcome dialog, and rejection, then attempts a real game
using compiled C++ and visible feedback, without your browser profile or hidden
answer access. It asserts a win for that run; inspect failures, since site updates,
network failures, or a solver loss can fail it.

The editable logo is [`extension/icons/wowdle.svg`](../extension/icons/wowdle.svg).
Its PNG exports are included in the extension. After changing the SVG, regenerate
them with the same optional Playwright dependency and installed Chrome:

```powershell
node tools/build_icons.cjs
```

The [tools index](../tools/README.md) explains when each script is needed and
which generated files can be removed. Python and Playwright are optional;
neither is needed to run the installed extension or native solver.

## Troubleshooting

| Message or symptom | What to check |
| --- | --- |
| `g++` is not recognized | Make your C++17 compiler available on `PATH`, then rerun the native build. |
| `em++ was not found` | Activate the existing Emscripten environment or check the sibling SDK path described above. |
| PowerShell refuses a build script | Use the process-scoped `-ExecutionPolicy Bypass` commands above. |
| `cpp solver bridge: unavailable` or an asset cannot load | Check the displayed error, rebuild WASM if needed, and reload the extension and side panel. Serve previews over HTTP. |
| Auto asks you to open a supported site | Make a supported HTTPS game URL the active tab. After an extension update, reload the extension and refresh the game tab. |
| `No supported board found` | Wait for the five-letter board to load and close its welcome screen. A changed site layout may need an adapter update. |
| `Close the game dialog` | Close the dialog on the game website, then press Play again. |
| A row contains a different word, has gaps, or a guess was rejected | Clear the current game row, wait for the toast to disappear, and review a different valid guess. Use normal mode. |
| `The game changed` or `A different guess appeared` | Sync the board, review the new recommendation, then press Play. |
| No candidates match the feedback | Check the played word and every tile color. The answer may be absent from the dictionary. Correct the feedback or undo a mistaken manual turn. |
| Auto times out or letters are not accepted | Check dialogs, current input, and the site's response. Sync before retrying; an interrupted Play is never automatically repeated. |
