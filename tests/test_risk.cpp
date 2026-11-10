#include "../src/strategies/risk.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace std;

void check(bool condition, const string& test_name) {
    if (!condition) {
        throw runtime_error("risk test failed: " + test_name);
    }
}

void test_candidate_guess_has_no_risk() {
    vector<string> candidates = {"slate", "crate"};

    check(basic_guess_risk("slate", candidates) == 0,
          "candidate guess has no risk");
    check(urgency_adjusted_risk("slate", candidates, 1) == 0,
          "candidate guess remains safe");
}

void test_probe_risk_increases_when_guesses_run_out() {
    vector<string> candidates = {"slate", "crate"};

    check(basic_guess_risk("arise", candidates) == 1,
          "probe has basic risk");
    check(abs(urgency_adjusted_risk("arise", candidates, 4) - 0.25) < 1e-9,
          "probe risk with four guesses");
    check(urgency_adjusted_risk("arise", candidates, 1) == 1,
          "probe risk with one guess");
}

void test_invalid_inputs_are_rejected() {
    bool threw_without_candidates = false;
    try {
        basic_guess_risk("slate", {});
    } catch (const invalid_argument&) {
        threw_without_candidates = true;
    }

    bool threw_without_turns = false;
    try {
        urgency_adjusted_risk("arise", {"slate"}, 0);
    } catch (const invalid_argument&) {
        threw_without_turns = true;
    }

    check(threw_without_candidates, "empty candidates");
    check(threw_without_turns, "zero guesses remaining");
}

int main() {
    try {
        test_candidate_guess_has_no_risk();
        test_probe_risk_increases_when_guesses_run_out();
        test_invalid_inputs_are_rejected();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    cout << "3 risk tests passed\n";
}
