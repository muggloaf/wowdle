#ifndef WOWDLE_ADAPTIVE_H
#define WOWDLE_ADAPTIVE_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>
using namespace std;

struct AdaptiveWeights {
    double information;
    double distance;
    double likelihood;
    double risk;
    double reduction;
};

struct AdaptiveConfig {
    size_t many_threshold = 500;
    size_t small_threshold = 50;
    size_t tiny_threshold = 5;
    AdaptiveWeights many = {0.65, 0.20, 0.05, 0.10, 0.10};
    AdaptiveWeights moderate = {0.40, 0.15, 0.10, 0.20, 0.35};
    AdaptiveWeights small = {0.15, 0.05, 0.45, 0.65, 0.35};
    AdaptiveWeights tiny = {0.05, 0.00, 0.80, 1.00, 0.15};
};

struct GuessAnalysis {
    string guess;
    double entropy = 0;
    double expected_candidates = 0;
    double worst_case = 0;
    double distance = 0;
    double probability = 0;
    double risk = 0;
    double information_score = 0;
    double reduction_score = 0;
    double likelihood_score = 0;
    double combined_score = 0;
    string stage;
    AdaptiveWeights weights = {0, 0, 0, 0, 0};
};

// the final turn returns up to five highest-probability candidates.
vector<GuessAnalysis> rank_adaptive_guesses(
    const vector<string>& guesses,
    const vector<string>& candidates,
    const map<string, double>& word_weights,
    int guesses_remaining,
    const AdaptiveConfig& config = {});

// reserve half the capped pool for likely candidates, then add likely valid probes.
// a zero limit evaluates the full union instead of using this heuristic shortlist.
vector<string> make_guess_pool(const vector<string>& all_guesses,
                               const vector<string>& candidates,
                               const map<string, double>& weights,
                               size_t limit = 100);

#endif
