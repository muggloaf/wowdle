#ifndef WOWDLE_SCORING_H
#define WOWDLE_SCORING_H

#include <string>
#include <utility>
#include <vector>
using namespace std;

struct GuessMetrics {
    double entropy = 0;
    double expected_candidates = 0;
    double worst_case = 0;
    double distance = 0;
    double bucket_separation = 0;
};

GuessMetrics calculate_guess_metrics(const string& guess,
                                    const vector<string>& candidates);

double letter_frequency_score(const string& guess,
                              const vector<string>& candidates);

double positional_frequency_score(const string& guess,
                                  const vector<string>& candidates);

// scores are sorted highest first; expected and minimax use negative counts.
vector<pair<string, double>> rank_baseline_guesses(
    const string& strategy,
    const vector<string>& guesses,
    const vector<string>& candidates);

#endif
