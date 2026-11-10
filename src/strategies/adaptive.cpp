#include "strategies/adaptive.hpp"
#include "strategies/scoring.hpp"
#include "strategies/risk.hpp"
#include "probability/prior.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
using namespace std;

static void validate_words(const vector<string>& words, size_t length) {
    for (const string& word : words) {
        if (word.empty() || word.size() != length) {
            throw invalid_argument("words must have the same non-zero length");
        }
        for (char letter : word) {
            if (!((letter >= 'a' && letter <= 'z') ||
                  (letter >= 'A' && letter <= 'Z'))) {
                throw invalid_argument("words must contain only letters a-z");
            }
        }
    }
}

static void validate_word_weights(const map<string, double>& weights) {
    for (const auto& entry : weights) {
        if (!isfinite(entry.second) || entry.second < 0) {
            throw invalid_argument("word weights must be finite and non-negative");
        }
    }
}

static double weight_total(const AdaptiveWeights& weights) {
    return weights.information + weights.distance + weights.likelihood +
           weights.risk + weights.reduction;
}

static void validate_component_weights(const AdaptiveWeights& weights) {
    for (double weight : {weights.information, weights.distance,
                          weights.likelihood, weights.risk, weights.reduction}) {
        if (!isfinite(weight) || weight < 0) {
            throw invalid_argument("adaptive weights must be finite and non-negative");
        }
    }
    if (!isfinite(weight_total(weights)) || weight_total(weights) <= 0) {
        throw invalid_argument("at least one adaptive weight must be positive");
    }
}

static void validate_config(const AdaptiveConfig& config) {
    if (config.tiny_threshold < 1 ||
        config.small_threshold <= config.tiny_threshold ||
        config.many_threshold <= config.small_threshold) {
        throw invalid_argument("thresholds must satisfy many > small > tiny >= 1");
    }
    validate_component_weights(config.many);
    validate_component_weights(config.moderate);
    validate_component_weights(config.small);
    validate_component_weights(config.tiny);
}

static vector<pair<string, double>> sorted_probabilities(
    const vector<string>& words, const map<string, double>& weights) {
    vector<string> unique_words;
    set<string> seen;
    for (const string& word : words) {
        if (seen.insert(word).second) {
            unique_words.push_back(word);
        }
    }
    auto ranked = rank_candidates(unique_words, weights);
    double total = 0;
    for (const auto& entry : ranked) {
        if (!isfinite(entry.second) || entry.second < 0) {
            throw invalid_argument("word weights cannot be normalized");
        }
        total += entry.second;
    }
    if (!(total > 0)) {
        throw invalid_argument("word weights cannot be normalized");
    }
    sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    return ranked;
}

vector<string> make_guess_pool(const vector<string>& all_guesses,
                               const vector<string>& candidates,
                               const map<string, double>& weights,
                               size_t limit) {
    if (candidates.empty()) {
        throw invalid_argument("cannot make a guess pool without candidates");
    }
    size_t length = candidates.front().size();
    validate_words(candidates, length);
    validate_words(all_guesses, length);
    validate_word_weights(weights);
    auto answers = sorted_probabilities(candidates, weights);
    auto valid_guesses = all_guesses.empty()
        ? answers : sorted_probabilities(all_guesses, weights);
    size_t capacity = limit == 0 ? answers.size() + valid_guesses.size() : limit;
    size_t candidate_slots = limit == 0 ? answers.size() : max(size_t(1), limit / 2);
    candidate_slots = min(candidate_slots, answers.size());

    vector<string> pool;
    set<string> seen;
    auto append = [&](const string& word) {
        if (pool.size() < capacity && seen.insert(word).second) {
            pool.push_back(word);
        }
    };
    for (size_t index = 0; index < candidate_slots; ++index) {
        append(answers[index].first);
    }
    for (const auto& guess : valid_guesses) {
        append(guess.first);
    }
    for (const auto& answer : answers) {
        append(answer.first);
    }
    return pool;
}

vector<GuessAnalysis> rank_adaptive_guesses(
    const vector<string>& guesses,
    const vector<string>& candidates,
    const map<string, double>& word_weights,
    int guesses_remaining,
    const AdaptiveConfig& config) {
    if (candidates.empty()) {
        throw invalid_argument("cannot choose without candidates");
    }
    if (guesses_remaining <= 0) {
        throw invalid_argument("guesses remaining must be positive");
    }
    validate_config(config);
    validate_words(candidates, candidates.front().size());
    validate_words(guesses, candidates.front().size());
    validate_word_weights(word_weights);
    auto answers = sorted_probabilities(candidates, word_weights);
    vector<string> unique_candidates;
    map<string, double> probabilities;
    for (const auto& answer : answers) {
        unique_candidates.push_back(answer.first);
        probabilities[answer.first] = answer.second;
    }
    size_t count = unique_candidates.size();
    string stage;
    AdaptiveWeights active_weights;
    if (count == 1) {
        stage = "single";
        active_weights = {0, 0, 1, 0, 0};
    } else if (guesses_remaining == 1) {
        stage = "final";
        active_weights = {0, 0, 1, 0, 0};
    } else if (count > config.many_threshold) {
        stage = "many";
        active_weights = config.many;
    } else if (count > config.small_threshold) {
        stage = "moderate";
        active_weights = config.moderate;
    } else if (count > config.tiny_threshold) {
        stage = "small";
        active_weights = config.small;
    } else {
        stage = "tiny";
        active_weights = config.tiny;
    }

    vector<string> pool;
    set<string> seen;
    bool candidates_only = stage == "tiny" || stage == "single" || stage == "final";
    if (candidates_only) {
        pool = unique_candidates;
        // final-turn ordering needs only priors, so score metrics for the displayed five.
        if (stage == "final" && pool.size() > 5) {
            pool.resize(5);
        }
    } else {
        if (guesses.empty()) {
            throw invalid_argument("cannot choose from an empty guess list");
        }
        for (const string& guess : guesses) {
            if (seen.insert(guess).second) {
                pool.push_back(guess);
            }
        }
        // never omit the most likely answer when a caller supplies only probes.
        if (seen.insert(answers.front().first).second) {
            pool.push_back(answers.front().first);
        }
    }

    double highest_probability = answers.front().second;
    double entropy_bound = min(log2(static_cast<double>(count)),
                               unique_candidates.front().size() * log2(3.0));
    vector<GuessAnalysis> ranked;
    for (const string& guess : pool) {
        GuessMetrics metrics = calculate_guess_metrics(guess, unique_candidates);
        GuessAnalysis analysis;
        analysis.guess = guess;
        analysis.entropy = metrics.entropy;
        analysis.expected_candidates = metrics.expected_candidates;
        analysis.worst_case = metrics.worst_case;
        analysis.distance = metrics.distance;
        auto probability = probabilities.find(guess);
        analysis.probability = probability == probabilities.end() ? 0 : probability->second;
        analysis.risk = urgency_adjusted_risk(guess, unique_candidates, guesses_remaining);
        analysis.information_score = entropy_bound > 0
            ? min(1.0, metrics.entropy / entropy_bound) : 0;
        analysis.reduction_score = count > 1
            ? max(0.0, min(1.0, (count - metrics.expected_candidates) / (count - 1))) : 0;
        analysis.likelihood_score = highest_probability > 0
            ? analysis.probability / highest_probability : 0;
        analysis.weights = active_weights;
        analysis.stage = stage;
        analysis.combined_score =
            (active_weights.information * analysis.information_score +
             active_weights.distance * analysis.distance +
             active_weights.likelihood * analysis.likelihood_score +
             active_weights.reduction * analysis.reduction_score -
             active_weights.risk * analysis.risk) / weight_total(active_weights);
        ranked.push_back(analysis);
    }

    sort(ranked.begin(), ranked.end(), [](const GuessAnalysis& left,
                                         const GuessAnalysis& right) {
        if (left.combined_score != right.combined_score) {
            return left.combined_score > right.combined_score;
        }
        if (left.probability != right.probability) {
            return left.probability > right.probability;
        }
        return left.guess < right.guess;
    });
    return ranked;
}
