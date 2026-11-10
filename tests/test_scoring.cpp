#include "strategies/scoring.hpp"
#include "strategies/distance.hpp"
#include "strategies/entropy.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace std;

void check(bool condition, const string& name) {
    if (!condition) {
        throw runtime_error("scoring test failed: " + name);
    }
}

void near(double actual, double expected, const string& name) {
    check(abs(actual - expected) < 1e-9, name);
}

void test_manually_counted_buckets() {
    GuessMetrics metrics = calculate_guess_metrics("aa", {"aa", "bb", "cc"});
    near(metrics.entropy, -(log2(1.0 / 3) / 3 + 2 * log2(2.0 / 3) / 3),
         "one bucket of one and one of two: entropy");
    near(metrics.expected_candidates, 5.0 / 3, "expected survivors");
    near(metrics.worst_case, 2, "largest bucket");
    near(metrics.distance, 2.0 / 3, "average feedback hamming distance");
    near(metrics.bucket_separation, 2.0 / 3, "fraction of separated pairs");
}

void test_identical_and_single_candidate_feedback() {
    GuessMetrics same = calculate_guess_metrics("zz", {"aa", "ab", "ac"});
    near(same.entropy, 0, "uninformative entropy");
    near(same.expected_candidates, 3, "uninformative survivors");
    near(same.distance, 0, "uninformative distance");
    GuessMetrics one = calculate_guess_metrics("aa", {"aa"});
    near(one.entropy, 0, "singleton entropy");
    near(one.expected_candidates, 1, "singleton survivors");
    near(one.distance, 0, "singleton distance");
    near(one.bucket_separation, 0, "singleton bucket separation");
}

void test_metrics_match_independent_implementations() {
    vector<string> candidates = {"allee", "apple", "ample", "apply", "papal"};
    for (const string& guess : vector<string>{"allee", "paper", "apple"}) {
        GuessMetrics metrics = calculate_guess_metrics(guess, candidates);
        near(metrics.entropy, calculate_entropy(guess, candidates),
             "duplicate-safe entropy agrees with existing engine");
        near(metrics.distance, average_feedback_separation(guess, candidates),
             "linear pair counting agrees with pairwise distance");
        near(metrics.bucket_separation, bucket_separation(guess, candidates),
             "bucket separation agrees with independent implementation");
    }
}

void test_letter_frequency_counts_each_letter_once() {
    vector<string> candidates = {"aaa", "abb", "bcc"};
    near(letter_frequency_score("aaa", candidates), 2.0 / 3,
         "guess duplicates do not repeat frequency credit");
    near(letter_frequency_score("abc", candidates), 5.0 / 3,
         "answer duplicates count once per answer");
    near(letter_frequency_score("ABC", candidates), 5.0 / 3,
         "frequency accepts uppercase letters");
}

void test_positional_frequency() {
    vector<string> candidates = {"aa", "ab", "bb"};
    near(positional_frequency_score("ab", candidates), 4.0 / 3,
         "positional counts reflect each column");
    near(positional_frequency_score("ba", candidates), 2.0 / 3,
         "swapped guess scores differently");
}

void test_every_baseline_and_deterministic_ties() {
    vector<string> candidates = {"aa", "ab"};
    vector<string> guesses = {"zz", "ab", "aa", "aa"};
    for (const string& strategy : vector<string>{
             "frequency", "positional", "entropy", "expected", "minimax", "distance"}) {
        auto ranked = rank_baseline_guesses(strategy, guesses, candidates);
        check(ranked.size() == 3, strategy + " removes duplicate guesses");
        check(ranked.front().first != "zz", strategy + " prefers informative letters");
        check(ranked.back().first == "zz", strategy + " ranks unhelpful probe last");
    }
    auto ranked = rank_baseline_guesses("entropy", {"ab", "aa"}, candidates);
    check(ranked.front().first == "aa", "ties use lexical order");
    near(rank_baseline_guesses("expected", {"aa"}, candidates)[0].second, -1,
         "expected count negated for descending ranking");
    near(rank_baseline_guesses("minimax", {"zz"}, candidates)[0].second, -2,
         "worst case negated for descending ranking");
}

void test_invalid_inputs() {
    int caught = 0;
    try { calculate_guess_metrics("aa", {}); } catch (const invalid_argument&) { ++caught; }
    try { calculate_guess_metrics("a", {"aa"}); } catch (const invalid_argument&) { ++caught; }
    try { calculate_guess_metrics("a!", {"aa"}); } catch (const invalid_argument&) { ++caught; }
    try { calculate_guess_metrics("aa", {"a!"}); } catch (const invalid_argument&) { ++caught; }
    try { rank_baseline_guesses("entropy", {}, {"aa"}); } catch (const invalid_argument&) { ++caught; }
    try { rank_baseline_guesses("unknown", {"aa"}, {"aa"}); } catch (const invalid_argument&) { ++caught; }
    check(caught == 6, "bad input rejected before feedback engine indexing");
}

int main() {
    try {
        test_manually_counted_buckets();
        test_identical_and_single_candidate_feedback();
        test_metrics_match_independent_implementations();
        test_letter_frequency_counts_each_letter_once();
        test_positional_frequency();
        test_every_baseline_and_deterministic_ties();
        test_invalid_inputs();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
    cout << "7 scoring tests passed\n";
}
