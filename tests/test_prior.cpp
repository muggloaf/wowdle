#include "../src/probability/prior.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace std;

void check(bool condition, const string& test_name) {
    if (!condition) {
        throw runtime_error("probability test failed: " + test_name);
    }
}

void test_uniform_weights() {
    vector<pair<string, double>> ranked = rank_candidates(
        {"slate", "crate", "stare"}, {});

    check(ranked.size() == 3, "uniform candidate count");
    check(abs(ranked[0].second - 1.0 / 3.0) < 1e-9,
          "uniform first probability");
    check(abs(ranked[1].second - 1.0 / 3.0) < 1e-9,
          "uniform second probability");
    check(abs(ranked[2].second - 1.0 / 3.0) < 1e-9,
          "uniform third probability");
}

void test_weighted_candidates_are_sorted() {
    map<string, double> weights = {
        {"slate", 2},
        {"crate", 8},
        {"stare", 10},
    };
    vector<pair<string, double>> ranked =
        rank_candidates({"slate", "crate", "stare"}, weights);

    check(ranked[0].first == "stare", "highest weight first");
    check(ranked[1].first == "crate", "second highest weight second");
    check(ranked[2].first == "slate", "lowest weight last");
    check(abs(ranked[0].second + ranked[1].second + ranked[2].second - 1) <
              1e-9,
          "probabilities sum to one");
}

void test_zero_and_missing_weights_use_fallback() {
    map<string, double> weights = {{"known", 4}, {"zero", 0}};
    vector<pair<string, double>> ranked =
        rank_candidates({"known", "zero", "missing"}, weights, 1);

    check(ranked[0].first == "known", "known weight remains largest");
    check(abs(ranked[1].second - ranked[2].second) < 1e-9,
          "zero and missing weights use fallback");
}

void test_invalid_fallback_is_rejected() {
    bool threw = false;
    try {
        rank_candidates({"slate"}, {}, 0);
    } catch (const invalid_argument&) {
        threw = true;
    }
    check(threw, "invalid fallback");
}

int main() {
    try {
        test_uniform_weights();
        test_weighted_candidates_are_sorted();
        test_zero_and_missing_weights_use_fallback();
        test_invalid_fallback_is_rejected();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    cout << "4 probability tests passed\n";
}
