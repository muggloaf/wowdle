#include "strategies/risk.hpp"
#include <algorithm>
#include <stdexcept>
using namespace std;

static bool is_candidate(const string& guess,
                         const vector<string>& candidates) {
    return find(candidates.begin(), candidates.end(), guess) !=
           candidates.end();
}

double basic_guess_risk(const string& guess,
                        const vector<string>& candidates) {
    if (candidates.empty()) {
        throw invalid_argument("cannot calculate risk without candidates");
    }

    return is_candidate(guess, candidates) ? 0 : 1;
}

double urgency_adjusted_risk(const string& guess,
                             const vector<string>& candidates,
                             int guesses_remaining) {
    if (guesses_remaining <= 0) {
        throw invalid_argument("guesses remaining must be positive");
    }
    if (candidates.empty()) {
        throw invalid_argument("cannot calculate risk without candidates");
    }

    if (is_candidate(guess, candidates)) {
        return 0;
    }

    return min(1.0, 1.0 / guesses_remaining);
}
