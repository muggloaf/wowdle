#include "core/candidate_filter.hpp"
#include "core/feedback.hpp"
#include <stdexcept>
using namespace std;

vector<string> filter_candidates(const vector<string>& candidates, const string& guess, const vector<char>& feedback) {
    if (guess.empty() || feedback.size() != guess.size()) {
        throw invalid_argument("guess and feedback must have the same non-zero length");
    }

    vector<string> remaining;

    // a candidate is valid only when it produces the feedback we received.
    for (const string& candidate : candidates) {
        if (evaluate_feedback(guess, candidate) == feedback) {
            remaining.push_back(candidate);
        }
    }

    return remaining;
}
