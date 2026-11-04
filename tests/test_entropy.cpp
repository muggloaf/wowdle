#include "../src/strategies/entropy.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace std;

void check(bool condition, const string& test_name) {
    if (!condition) {
        throw runtime_error("entropy test failed: " + test_name);
    }
}

void test_no_information_has_zero_entropy() {
    vector<string> candidates = {"aa", "ab", "ac"};

    check(abs(calculate_entropy("zz", candidates)) < 1e-9,
          "identical feedback has zero entropy");
}

void test_even_split_has_one_bit() {
    vector<string> candidates = {"aa", "ab"};

    check(abs(calculate_entropy("ab", candidates) - 1.0) < 1e-9,
          "two equally likely feedback patterns have one bit");
}

void test_best_guess_has_highest_entropy() {
    vector<string> candidates = {"aa", "ab"};
    vector<string> guesses = {"cc", "aa"};

    check(best_entropy_guess(guesses, candidates) == "aa",
          "highest entropy guess is selected");
}

void test_empty_inputs_are_rejected() {
    bool threw_without_candidates = false;
    try {
        calculate_entropy("aa", {});
    } catch (const invalid_argument&) {
        threw_without_candidates = true;
    }

    bool threw_without_guesses = false;
    try {
        best_entropy_guess({}, {"aa"});
    } catch (const invalid_argument&) {
        threw_without_guesses = true;
    }

    check(threw_without_candidates, "empty candidates rejected");
    check(threw_without_guesses, "empty guesses rejected");
}

void test_combined_ranking_returns_one_recommendation_list() {
    vector<string> candidates = {"aa", "ab"};
    vector<string> guesses = {"cc", "aa"};
    map<string, double> weights = {{"aa", 10}, {"ab", 1}};

    vector<pair<string, double>> ranked =
        rank_combined_guesses(guesses, candidates, weights);

    check(ranked.size() == 2, "both guesses are ranked");
    check(ranked[0].first == "aa", "combined recommendation is first");
    check(ranked[0].second >= ranked[1].second,
          "combined scores are sorted");
}

int main() {
    try {
        test_no_information_has_zero_entropy();
        test_even_split_has_one_bit();
        test_best_guess_has_highest_entropy();
        test_empty_inputs_are_rejected();
        test_combined_ranking_returns_one_recommendation_list();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    cout << "5 entropy tests passed\n";
}
