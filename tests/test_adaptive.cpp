#include "strategies/adaptive.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace std;

void check(bool condition, const string& name) {
    if (!condition) {
        throw runtime_error("adaptive test failed: " + name);
    }
}

void near(double actual, double expected, const string& name) {
    check(abs(actual - expected) < 1e-9, name);
}

vector<string> words(size_t count) {
    vector<string> result;
    for (size_t value = 0; value < count; ++value) {
        string word = "aaa";
        size_t remaining = value;
        for (char& letter : word) {
            letter += remaining % 26;
            remaining /= 26;
        }
        result.push_back(word);
    }
    return result;
}

const GuessAnalysis& find_guess(const vector<GuessAnalysis>& ranked,
                                const string& guess) {
    for (const auto& result : ranked) {
        if (result.guess == guess) {
            return result;
        }
    }
    throw runtime_error("missing test guess: " + guess);
}

void test_configurable_stage_boundaries() {
    for (const auto& entry : vector<pair<size_t, string>>{
             {501, "many"}, {500, "moderate"}, {51, "moderate"},
             {50, "small"}, {6, "small"}, {5, "tiny"}}) {
        auto candidates = words(entry.first);
        auto ranked = rank_adaptive_guesses({"zzz"}, candidates, {}, 6);
        check(ranked.front().stage == entry.second, "default threshold boundary");
    }
    AdaptiveConfig config;
    config.many_threshold = 8;
    config.small_threshold = 4;
    config.tiny_threshold = 2;
    check(rank_adaptive_guesses({"zzz"}, words(9), {}, 6, config)[0].stage == "many",
          "many threshold configurable");
    check(rank_adaptive_guesses({"zzz"}, words(4), {}, 6, config)[0].stage == "small",
          "small threshold configurable");
}

void test_single_candidate_and_last_turn() {
    auto one = rank_adaptive_guesses({}, {"apple"}, {}, 6);
    check(one.size() == 1 && one[0].guess == "apple" && one[0].stage == "single",
          "singleton selected even without supplied guesses");
    auto final = rank_adaptive_guesses({"zz"}, {"aa", "ab", "ac"},
                                      {{"aa", 1}, {"ab", 9}, {"ac", 2}}, 1);
    check(final.front().guess == "ab" && final.front().stage == "final",
          "last turn picks highest probability candidate outside guess pool");
    check(final.size() == 3, "last turn excludes all information-only probes");
    near(final.front().probability, 0.75, "probability renormalized over candidates");
    near(final.front().risk, 0, "final guess has no probe risk");
}

void test_tiny_stage_prefers_candidates() {
    auto ranked = rank_adaptive_guesses({"zz"}, {"aa", "ab", "ac"}, {}, 3);
    check(ranked.size() == 3, "tiny evaluates remaining answers");
    for (const auto& result : ranked) {
        check(result.guess != "zz" && result.risk == 0, "tiny rejects probes");
    }
}

void test_large_final_turn_limits_metric_work() {
    vector<string> candidates = words(1000);
    string most_likely = candidates.back();
    auto final = rank_adaptive_guesses({"zzz"}, candidates, {{most_likely, 100}}, 1);
    check(final.size() == 5, "large final turn returns five suggestions");
    check(final.front().guess == most_likely,
          "large final turn selects exact highest prior outside input pool");
    for (const auto& result : final) {
        check(result.stage == "final" && result.risk == 0,
              "every final suggestion is a candidate");
        check(find(candidates.begin(), candidates.end(), result.guess) != candidates.end(),
              "large final turn excludes probes");
    }
}

void test_normalized_scores_and_explanation() {
    AdaptiveConfig config;
    config.tiny_threshold = 1;
    auto ranked = rank_adaptive_guesses({"aa", "zz"}, {"aa", "ab", "ac"},
                                      {{"aa", 2}, {"ab", 1}, {"ac", 1}}, 4, config);
    const auto& answer = find_guess(ranked, "aa");
    const auto& probe = find_guess(ranked, "zz");
    near(answer.probability, 0.5, "candidate probability is raw normalized prior");
    near(answer.likelihood_score, 1, "likelihood normalized independently");
    near(probe.probability, 0, "probe has no answer probability");
    near(probe.expected_candidates, 3, "uninformative probe keeps all candidates");
    near(probe.reduction_score, 0, "no reduction gets zero reward");
    near(answer.reduction_score, 2.0 / 3, "reduction normalized from manual bucket counts");
    for (const auto& result : ranked) {
        check(result.information_score >= 0 && result.information_score <= 1,
              "information normalized");
        check(result.reduction_score >= 0 && result.reduction_score <= 1,
              "reduction normalized");
        check(result.distance >= 0 && result.distance <= 1, "distance normalized");
        check(result.likelihood_score >= 0 && result.likelihood_score <= 1,
              "likelihood normalized");
        const auto& w = result.weights;
        double expected = (w.information * result.information_score +
                           w.distance * result.distance +
                           w.likelihood * result.likelihood_score +
                           w.reduction * result.reduction_score -
                           w.risk * result.risk) /
                          (w.information + w.distance + w.likelihood + w.reduction + w.risk);
        near(result.combined_score, expected, "logged components reproduce selection score");
    }
}

void test_risk_increases_as_turns_run_out() {
    AdaptiveConfig config;
    config.tiny_threshold = 1;
    auto early = rank_adaptive_guesses({"zz"}, {"aa", "ab"}, {}, 6, config);
    auto late = rank_adaptive_guesses({"zz"}, {"aa", "ab"}, {}, 2, config);
    near(find_guess(early, "zz").risk, 1.0 / 6, "early probe risk");
    near(find_guess(late, "zz").risk, 0.5, "late probe risk");
    check(find_guess(late, "zz").combined_score < find_guess(early, "zz").combined_score,
          "urgency lowers same probe score");
    near(find_guess(late, "aa").risk, 0, "candidate risk stays zero");
}

void test_configurable_weights_change_decision() {
    AdaptiveConfig config;
    config.tiny_threshold = 1;
    config.small = {0, 0, 1, 0, 0};
    auto likelihood = rank_adaptive_guesses({"aa", "ab"}, {"aa", "ab"},
                                           {{"aa", 1}, {"ab", 4}}, 3, config);
    check(likelihood.front().guess == "ab", "likelihood-only weights honored");
    config.small = {0, 1, 0, 0, 0};
    auto separation = rank_adaptive_guesses({"zz", "aa"}, {"aa", "ab"}, {}, 3, config);
    check(separation.front().guess == "aa", "distance-only weights honored");
}

void test_guess_pool_candidates_probes_and_full_mode() {
    vector<string> candidates = {"aa", "ab", "ac"};
    vector<string> valid = {"zz", "zy", "aa", "ab", "ac", "zz"};
    map<string, double> weights = {{"aa", 10}, {"ab", 2}, {"ac", 1},
                                  {"zz", 100}, {"zy", 50}};
    auto pool = make_guess_pool(valid, candidates, weights, 4);
    check(pool.size() == 4, "guess cap respected");
    check(pool[0] == "aa" && pool[1] == "ab", "half of pool reserves likely candidates");
    check(find(pool.begin(), pool.end(), "zz") != pool.end(), "valid probe included");
    check(make_guess_pool(valid, candidates, weights, 1) == vector<string>{"aa"},
          "one-slot pool keeps most likely candidate");
    check(make_guess_pool(valid, candidates, weights, 0).size() == 5,
          "zero cap evaluates full distinct union");
    auto reversed = valid;
    reverse(reversed.begin(), reversed.end());
    check(make_guess_pool(valid, candidates, {}, 4) ==
          make_guess_pool(reversed, candidates, {}, 4), "uniform ties deterministic");
}

void test_invalid_input_and_config() {
    int caught = 0;
    auto attempt = [&](const vector<string>& guesses, const vector<string>& candidates,
                       const map<string, double>& weights, int remaining,
                       const AdaptiveConfig& config) {
        try {
            rank_adaptive_guesses(guesses, candidates, weights, remaining, config);
        } catch (const invalid_argument&) { ++caught; }
    };
    AdaptiveConfig config;
    attempt({"aa"}, {}, {}, 2, config);
    attempt({"aa"}, {"aa"}, {}, 0, config);
    attempt({"a!"}, {"aa"}, {}, 2, config);
    attempt({"aa"}, {"aa"}, {{"aa", -1}}, 2, config);
    attempt({"aa"}, {"aa"}, {{"aa", numeric_limits<double>::quiet_NaN()}}, 2, config);
    config.small_threshold = config.tiny_threshold;
    attempt({"aa"}, {"aa"}, {}, 2, config);
    config = {};
    config.many.information = -1;
    attempt({"aa"}, {"aa"}, {}, 2, config);
    config = {};
    config.tiny = {0, 0, 0, 0, 0};
    attempt({"aa"}, {"aa"}, {}, 2, config);
    config = {};
    config.tiny_threshold = 1;
    attempt({}, {"aa", "ab"}, {}, 2, config);
    check(caught == 9, "invalid inputs and config rejected");
}

int main() {
    try {
        test_configurable_stage_boundaries();
        test_single_candidate_and_last_turn();
        test_tiny_stage_prefers_candidates();
        test_large_final_turn_limits_metric_work();
        test_normalized_scores_and_explanation();
        test_risk_increases_as_turns_run_out();
        test_configurable_weights_change_decision();
        test_guess_pool_candidates_probes_and_full_mode();
        test_invalid_input_and_config();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
    cout << "9 adaptive tests passed\n";
}
