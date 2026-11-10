#include "strategies/distance.hpp"
#include "core/feedback.hpp"
#include <cstdlib>
#include <map>
#include <stdexcept>
using namespace std;

static string feedback_text(const vector<char>& feedback) {
    return string(feedback.begin(), feedback.end());
}

static int mark_value(char mark) {
    if (mark == 'g') {
        return 2;
    }
    if (mark == 'y') {
        return 1;
    }
    return 0;
}

int word_hamming_distance(const string& first, const string& second) {
    if (first.size() != second.size()) {
        throw invalid_argument("words must have the same length");
    }

    int distance = 0;
    for (size_t index = 0; index < first.size(); ++index) {
        if (first[index] != second[index]) {
            ++distance;
        }
    }
    return distance;
}

int feedback_hamming_distance(const string& guess,
                              const string& first_answer,
                              const string& second_answer) {
    vector<char> first_feedback = evaluate_feedback(guess, first_answer);
    vector<char> second_feedback = evaluate_feedback(guess, second_answer);
    return word_hamming_distance(feedback_text(first_feedback),
                                 feedback_text(second_feedback));
}

double weighted_feedback_distance(const string& guess,
                                  const string& first_answer,
                                  const string& second_answer) {
    vector<char> first_feedback = evaluate_feedback(guess, first_answer);
    vector<char> second_feedback = evaluate_feedback(guess, second_answer);
    double distance = 0;

    for (size_t index = 0; index < first_feedback.size(); ++index) {
        distance += abs(mark_value(first_feedback[index]) -
                        mark_value(second_feedback[index]));
    }

    if (guess.empty()) {
        return 0;
    }
    return distance / (2 * guess.size());
}

double average_feedback_separation(const string& guess,
                                   const vector<string>& candidates) {
    if (candidates.size() < 2) {
        return 0;
    }

    double total_distance = 0;
    size_t pair_count = 0;
    for (size_t first = 0; first < candidates.size(); ++first) {
        for (size_t second = first + 1; second < candidates.size(); ++second) {
            total_distance += feedback_hamming_distance(
                guess, candidates[first], candidates[second]);
            ++pair_count;
        }
    }

    return total_distance / (pair_count * guess.size());
}

double bucket_separation(const string& guess,
                         const vector<string>& candidates) {
    if (candidates.size() < 2) {
        return 0;
    }

    map<string, int> buckets;
    for (const string& candidate : candidates) {
        buckets[feedback_text(evaluate_feedback(guess, candidate))]++;
    }

    double total_pairs = candidates.size() * (candidates.size() - 1) / 2.0;
    double same_bucket_pairs = 0;
    for (const auto& bucket : buckets) {
        same_bucket_pairs += bucket.second * (bucket.second - 1) / 2.0;
    }

    return 1 - same_bucket_pairs / total_pairs;
}
