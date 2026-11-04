#include "strategies/entropy.hpp"
#include "core/feedback.hpp"
#include "probability/prior.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
using namespace std;

static string pattern_text(const vector<char>& feedback) {
    return string(feedback.begin(), feedback.end());
}

double calculate_entropy(const string& guess,
                         const vector<string>& candidates) {
    if (candidates.empty()) {
        throw invalid_argument("cannot calculate entropy without candidates");
    }

    map<string, int> buckets;
    for (const string& candidate : candidates) {
        string pattern = pattern_text(evaluate_feedback(guess, candidate));
        buckets[pattern]++;
    }

    double entropy = 0;
    double total = candidates.size();
    for (const auto& bucket : buckets) {
        double probability = bucket.second / total;
        entropy -= probability * log2(probability);
    }

    return entropy;
}

string best_entropy_guess(const vector<string>& guesses,
                          const vector<string>& candidates) {
    if (guesses.empty()) {
        throw invalid_argument("cannot choose from an empty guess list");
    }
    if (candidates.empty()) {
        throw invalid_argument("cannot choose without candidates");
    }

    string best_guess = guesses.front();
    double highest_entropy = calculate_entropy(best_guess, candidates);

    for (size_t i = 1; i < guesses.size(); ++i) {
        double entropy = calculate_entropy(guesses[i], candidates);
        if (entropy > highest_entropy) {
            highest_entropy = entropy;
            best_guess = guesses[i];
        }
    }

    return best_guess;
}

vector<pair<string, double>> rank_combined_guesses(
    const vector<string>& guesses,
    const vector<string>& candidates,
    const map<string, double>& weights,
    double information_weight,
    double likelihood_weight) {
    if (guesses.empty() || candidates.empty()) {
        throw invalid_argument("guesses and candidates cannot be empty");
    }
    if (information_weight < 0 || likelihood_weight < 0 ||
        information_weight + likelihood_weight == 0) {
        throw invalid_argument("combined guess weights must be positive");
    }

    vector<pair<string, double>> ranked_answers =
        rank_candidates(candidates, weights);
    map<string, double> likelihoods;
    double highest_likelihood = 0;
    for (const auto& answer : ranked_answers) {
        likelihoods[answer.first] = answer.second;
        if (answer.second > highest_likelihood) {
            highest_likelihood = answer.second;
        }
    }

    vector<pair<string, double>> entropy_scores;
    double highest_entropy = 0;
    for (const string& guess : guesses) {
        double entropy = calculate_entropy(guess, candidates);
        entropy_scores.push_back({guess, entropy});
        if (entropy > highest_entropy) {
            highest_entropy = entropy;
        }
    }

    vector<pair<string, double>> combined;
    double total_weight = information_weight + likelihood_weight;
    for (const auto& scored_guess : entropy_scores) {
        double normalized_entropy = 0;
        if (highest_entropy > 0) {
            normalized_entropy = scored_guess.second / highest_entropy;
        }

        double normalized_likelihood = 0;
        auto likelihood = likelihoods.find(scored_guess.first);
        if (likelihood != likelihoods.end() && highest_likelihood > 0) {
            normalized_likelihood = likelihood->second / highest_likelihood;
        }

        double score =
            (information_weight * normalized_entropy +
             likelihood_weight * normalized_likelihood) /
            total_weight;
        combined.push_back({scored_guess.first, score});
    }

    stable_sort(combined.begin(), combined.end(),
                [](const pair<string, double>& left,
                   const pair<string, double>& right) {
                    return left.second > right.second;
                });
    return combined;
}
