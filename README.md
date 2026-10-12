# wowdle

A generalized Wordle solver framework in C++. Phase 1 provides the standalone
Wordle feedback engine with correct duplicate-letter handling. Phase 2 filters
possible answers by comparing simulated feedback.

## Build and test

From PowerShell, compile the library and tests directly:

```powershell
g++ -std=c++17 -Isrc src/core/feedback.cpp tests/test_feedback.cpp -o feedback_tests.exe
./feedback_tests.exe

g++ -std=c++17 -Isrc src/core/feedback.cpp src/core/candidate_filter.cpp tests/test_candidate_filter.cpp -o candidate_filter_tests.exe
./candidate_filter_tests.exe
```

The feedback API accepts words of any non-zero, equal length. It returns one
mark per position: `g` for green, `y` for yellow, and `x` for gray.
