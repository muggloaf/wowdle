#include "strategies/scoring.hpp"
#include "core/feedback.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
using namespace std;

static int letter_index(char letter) {
    if (letter >= 'A' && letter <= 'Z') {
        return letter - 'A';
    }
    if (letter >= 'a' && letter <= 'z') {
        return letter - 'a';
    }
    throw invalid_argument("words must contain only letters a-z");
}

static void validate_word(const string& word, size_t length) {
    if (word.empty() || word.size() != length) {
        throw invalid_argument("words must have the same non-zero length");
    }
    for (char letter : word) {
        letter_index(letter);
    }
}

static size_t validate_candidates(const vector<string>& candidates) {
    if (candidates.empty()) {
        throw invalid_argument("cannot score without candidates");
    }
    size_t length = candidates.front().size();
    for (const string& word : candidates) {
        validate_word(word, length);
    }
    return length;
}

GuessMetrics calculate_guess_metrics(const string& guess,
                                    const vector<string>& candidates) {
    size_t length = validate_candidates(candidates);
    validate_word(guess, length);
    map<string, size_t> buckets;
    vector<array<size_t, 3>> position_counts(length, {0, 0, 0});

    for (const string& candidate : candidates) {
        vector<char> feedback = evaluate_feedback(guess, candidate);
        ++buckets[string(feedback.begin(), feedback.end())];
        for (size_t position = 0; position < length; ++position) {
            char mark = feedback[position];
            size_t index = mark == 'g' ? 2 : (mark == 'y' ? 1 : 0);
            ++position_counts[position][index];
        }
    }

    GuessMetrics metrics;
    double count = static_cast<double>(candidates.size());
    double same_bucket_pairs = 0;
    for (const auto& bucket : buckets) {
        double size = static_cast<double>(bucket.second);
        double probability = size / count;
        metrics.entropy -= probability * log2(probability);
        metrics.expected_candidates += probability * size;
        metrics.worst_case = max(metrics.worst_case, size);
        same_bucket_pairs += size * (size - 1) / 2;
    }

    if (candidates.size() > 1) {
        double pairs = count * (count - 1) / 2;
        metrics.bucket_separation = 1 - same_bucket_pairs / pairs;
        // count unequal feedback marks directly instead of visiting every pair.
        for (const auto& marks : position_counts) {
            double matching_pairs = 0;
            for (size_t mark_count : marks) {
                double size = static_cast<double>(mark_count);
                matching_pairs += size * (size - 1) / 2;
            }
            metrics.distance += (pairs - matching_pairs) / pairs;
        }
        metrics.distance /= length;
    }
    return metrics;
}

static array<size_t, 26> count_letters(const vector<string>& candidates) {
    array<size_t, 26> counts = {};
    for (const string& candidate : candidates) {
        array<bool, 26> seen = {};
        for (char letter : candidate) {
            seen[letter_index(letter)] = true;
        }
        for (size_t letter = 0; letter < counts.size(); ++letter) {
            counts[letter] += seen[letter] ? 1 : 0;
        }
    }
    return counts;
}

static double score_letters(const string& guess,
                            const array<size_t, 26>& counts,
                            size_t candidate_count) {
    array<bool, 26> seen = {};
    double score = 0;
    for (char letter : guess) {
        size_t index = letter_index(letter);
        if (!seen[index]) {
            score += static_cast<double>(counts[index]) / candidate_count;
            seen[index] = true;
        }
    }
    return score;
}

double letter_frequency_score(const string& guess,
                              const vector<string>& candidates) {
    size_t length = validate_candidates(candidates);
    validate_word(guess, length);
    return score_letters(guess, count_letters(candidates), candidates.size());
}

static vector<array<size_t, 26>> count_positions(
    const vector<string>& candidates, size_t length) {
    vector<array<size_t, 26>> counts(length, array<size_t, 26>{});
    for (const string& candidate : candidates) {
        for (size_t position = 0; position < length; ++position) {
            ++counts[position][letter_index(candidate[position])];
        }
    }
    return counts;
}

static double score_positions(const string& guess,
                              const vector<array<size_t, 26>>& counts,
                              size_t candidate_count) {
    double score = 0;
    for (size_t position = 0; position < guess.size(); ++position) {
        score += static_cast<double>(counts[position][letter_index(guess[position])]) /
                 candidate_count;
    }
    return score;
}

double positional_frequency_score(const string& guess,
                                  const vector<string>& candidates) {
    size_t length = validate_candidates(candidates);
    validate_word(guess, length);
    return score_positions(guess, count_positions(candidates, length),
                           candidates.size());
}

vector<pair<string, double>> rank_baseline_guesses(
    const string& strategy,
    const vector<string>& guesses,
    const vector<string>& candidates) {
    size_t length = validate_candidates(candidates);
    if (guesses.empty()) {
        throw invalid_argument("cannot choose from an empty guess list");
    }
    if (strategy != "frequency" && strategy != "positional" &&
        strategy != "entropy" && strategy != "expected" &&
        strategy != "minimax" && strategy != "distance") {
        throw invalid_argument("unknown baseline strategy: " + strategy);
    }

    array<size_t, 26> letters = {};
    vector<array<size_t, 26>> positions;
    if (strategy == "frequency") {
        letters = count_letters(candidates);
    } else if (strategy == "positional") {
        positions = count_positions(candidates, length);
    }

    vector<pair<string, double>> ranked;
    set<string> seen;
    for (const string& guess : guesses) {
        validate_word(guess, length);
        if (!seen.insert(guess).second) {
            continue;
        }
        double score;
        if (strategy == "frequency") {
            score = score_letters(guess, letters, candidates.size());
        } else if (strategy == "positional") {
            score = score_positions(guess, positions, candidates.size());
        } else {
            GuessMetrics metrics = calculate_guess_metrics(guess, candidates);
            if (strategy == "entropy") {
                score = metrics.entropy;
            } else if (strategy == "expected") {
                score = -metrics.expected_candidates;
            } else if (strategy == "minimax") {
                score = -metrics.worst_case;
            } else {
                score = metrics.distance;
            }
        }
        ranked.push_back({guess, score});
    }

    sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    return ranked;
}
