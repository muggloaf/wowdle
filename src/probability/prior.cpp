#include "probability/prior.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
using namespace std;

map<string, double> load_word_weights(const string& filename) {
    ifstream file(filename);
    if (!file) {
        throw runtime_error("could not open word weights: " + filename);
    }

    map<string, double> weights;
    string word;
    double weight;

    while (file >> word >> weight) {
        if (weight < 0) {
            throw runtime_error("word weights cannot be negative");
        }
        weights[word] = weight;
    }

    if (weights.empty()) {
        throw runtime_error("word weights file is empty");
    }

    return weights;
}

vector<pair<string, double>> rank_candidates(const vector<string>& candidates, const map<string, double>& weights, double fallback_weight) {
    if (fallback_weight <= 0) {
        throw invalid_argument("fallback weight must be positive");
    }

    double total_weight = 0;
    for (const string& candidate : candidates) {
        auto found = weights.find(candidate);
        double weight = found == weights.end() ? fallback_weight : found->second;
        if (weight <= 0) {
            weight = fallback_weight;
        }
        total_weight += weight;
    }

    if (total_weight == 0) {
        throw invalid_argument("cannot rank an empty candidate list");
    }

    vector<pair<string, double>> ranked;
    for (const string& candidate : candidates) {
        auto found = weights.find(candidate);
        double weight = found == weights.end() ? fallback_weight : found->second;
        if (weight <= 0) {
            weight = fallback_weight;
        }
        ranked.push_back({candidate, weight / total_weight});
    }

    stable_sort(ranked.begin(), ranked.end(),
                [](const pair<string, double>& left,
                   const pair<string, double>& right) {
                    return left.second > right.second;
                });

    return ranked;
}
