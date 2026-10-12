#include "core/feedback.hpp"
#include <stdexcept>
#include <cctype>
using namespace std;


vector<char> evaluate_feedback(const string& guess, const string& answer) {
    if (guess.empty() || guess.size() != answer.size()) {
        throw invalid_argument("guess and answer must have the same non-zero length");
    }

    vector<char> feedback(guess.size(), 'x');
    int unused_letters[26] = {};

    // mark greens first so they cannot also be counted as yellows.
    for (int i = 0; i < guess.size(); i++) {
        char guess_letter = tolower(guess[i]);
        char answer_letter = tolower(answer[i]);

        if (guess_letter == answer_letter) {
            feedback[i] = 'g';
        } else {
            unused_letters[answer_letter - 'a']++;
        }
    }

    // use each unmatched answer letter at most once for a yellow mark.
    for (int i = 0; i < guess.size(); ++i) {
        if (feedback[i] == 'g') {
            continue;
        }

        int letter = tolower(guess[i])-'a';
        if (unused_letters[letter] > 0) {
            feedback[i] = 'y';
            unused_letters[letter]--;
        }
    }

    return feedback;
}