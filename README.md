# wowdle

A generalized Wordle solver framework in C++. Phase 1 provides the standalone
Wordle feedback engine with correct duplicate-letter handling. Phase 2 filters
possible answers by comparing simulated feedback. Phase 3 adds a basic command-
line solver. Phase 4 ranks remaining answers using English word-frequency data.
Phase 5 chooses guesses using expected information gain.

## Build and test

From PowerShell, compile the library and tests directly:

```powershell
g++ -std=c++17 -Isrc src/core/feedback.cpp tests/test_feedback.cpp -o feedback_tests.exe
./feedback_tests.exe

g++ -std=c++17 -Isrc src/core/feedback.cpp src/core/candidate_filter.cpp tests/test_candidate_filter.cpp -o candidate_filter_tests.exe
./candidate_filter_tests.exe

g++ -std=c++17 -Isrc src/probability/prior.cpp tests/test_prior.cpp -o prior_tests.exe
./prior_tests.exe

g++ -std=c++17 -Isrc src/core/feedback.cpp src/probability/prior.cpp src/strategies/entropy.cpp tests/test_entropy.cpp -o entropy_tests.exe
./entropy_tests.exe

g++ -std=c++17 -Isrc src/main.cpp src/core/feedback.cpp src/core/candidate_filter.cpp src/probability/prior.cpp src/strategies/entropy.cpp -o wowdle.exe
./wowdle.exe
```

The frequency file is generated from the `wordfreq` package:

```powershell
python -m pip install -r requirements.txt
python tools/generate_frequency_data.py
```

This creates `data/word_frequencies_fives.txt`. The C++ program only reads the
generated file; it does not depend on Python at runtime.

The solver combines entropy and frequency into one recommendation score. This
lets it balance an information-gathering word such as `rates` with a likely
answer such as `about`, instead of presenting them as competing answers. The
combined score is calculated against every remaining answer, while
the CLI checks the top 500 probability-ranked words as possible guesses so the
five-letter solver stays practical. You can also provide a different word list:

```powershell
./wowdle.exe data/wordlist_fives.txt
```

After each guess, enter feedback using `g`, `y`, and `x`, for example `gxygx`.
If the suggested guess is not accepted by the Wordle game, enter `c` instead
and choose one of the five displayed likely answers. After the game ends,
press Enter to close the program.

To let the program choose a secret answer and play automatically:

```powershell
./wowdle.exe --computer
./wowdle.exe --computer data/wordlist_fives.txt
```

The feedback API accepts words of any non-zero, equal length. It returns one
mark per position: `g` for green, `y` for yellow, and `x` for gray.
