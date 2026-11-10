#include "evaluation/benchmark.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace std;

static void check(bool condition, const string& message) {
    if (!condition) throw runtime_error(message);
}

static void close(double actual, double expected, const string& message) {
    check(abs(actual - expected) < 1e-9, message);
}

static void test_game_feedback_and_budget() {
    vector<string> allowed = {"apple", "allee", "apply"};
    ComputerGame game("apple", allowed, 2);
    check(game.submit("allee") == vector<char>({'g', 'y', 'x', 'x', 'g'}),
          "engine gives duplicate-safe feedback");
    check(game.guesses_remaining() == 1 && !game.finished(), "engine consumes one turn");
    game.submit("apply");
    check(game.finished() && !game.solved() && game.guesses_remaining() == 0,
          "engine records a real loss at the turn limit");
    bool rejected = false;
    try { game.submit("apple"); } catch (const logic_error&) { rejected = true; }
    check(rejected, "engine rejects play after game over");
}

static void test_invalid_guess_does_not_consume_turn() {
    ComputerGame game("apple", {"apple", "apply"}, 6);
    bool rejected = false;
    try { game.submit("?????"); } catch (const invalid_argument&) { rejected = true; }
    check(rejected && game.guesses_remaining() == 6, "invalid guesses do not consume turns");
    game.submit("apple");
    check(game.solved() && game.finished(), "all greens finish the game");
}

static void test_summary_denominators() {
    GameResult first, second, third, fourth;
    first.won = true; first.guesses = 2; first.candidate_guesses = 2; first.runtime_ms = 4;
    second.won = true; second.guesses = 3; second.candidate_guesses = 2; second.information_guesses = 1; second.runtime_ms = 8;
    third.won = false; third.guesses = 6; third.candidate_guesses = 5; third.information_guesses = 1; third.runtime_ms = 12;
    fourth.won = true; fourth.guesses = 1; fourth.candidate_guesses = 1; fourth.runtime_ms = 16;
    BenchmarkSummary summary = summarize_games({first, second, third, fourth});
    check(summary.games == 4 && summary.wins == 3 && summary.failures == 1, "summary counts games");
    close(summary.average_guesses, 3, "mean includes failed attempts");
    close(summary.median_guesses, 2.5, "even median averages middle values");
    close(summary.average_winning_guesses, 2, "winning mean excludes failed games");
    close(summary.average_runtime_ms, 10, "runtime is per game");
    close(summary.win_rate, 0.75, "win rate denominator is all games");
    close(summary.failure_rate, 0.25, "failure rate denominator is all games");
    check(summary.worst_guesses == 6 && summary.candidate_guesses == 10 &&
          summary.information_guesses == 2, "worst and guess categories are counted");
    check(summarize_games({}).games == 0, "empty summaries are defined");
}

static void test_same_seed_repeats_games() {
    vector<string> answers = {"apple", "apply", "ample", "maple"};
    vector<string> guesses = answers;
    guesses.push_back("allee");
    BenchmarkOptions options;
    options.games = 12;
    options.seed = 17;
    options.pool_limit = 0;
    vector<string> strategies = strategy_names();
    auto first = run_benchmark(answers, guesses, {}, strategies, options);
    auto second = run_benchmark(answers, guesses, {}, strategies, options);
    check(first.size() == 8, "all eight strategies run");
    for (size_t strategy = 0; strategy < first.size(); ++strategy) {
        check(first[strategy].results.size() == options.games, "requested batch size is honored");
        for (size_t game = 0; game < options.games; ++game) {
            const auto& a = first[strategy].results[game];
            const auto& b = second[strategy].results[game];
            check(a.played == b.played && a.feedback == b.feedback && a.won == b.won,
                  "same seed repeats strategy choices and feedback");
            check(a.guesses <= options.max_guesses, "no strategy exceeds budget");
            check(a.candidate_guesses + a.information_guesses == a.guesses,
                  "every guess belongs to exactly one category");
            set<string> unique(a.played.begin(), a.played.end());
            check(unique.size() == a.played.size(), "strategies do not repeat played guesses");
        }
    }
}

static void test_shared_answer_sample() {
    vector<string> answers = {"apple", "apply", "ample", "maple"};
    BenchmarkOptions options;
    options.games = 16;
    options.max_guesses = 10;
    auto reports = run_benchmark(answers, answers, {}, strategy_names(), options);
    for (size_t game = 0; game < options.games; ++game) {
        string expected = reports.front().results[game].played.back();
        for (const auto& report : reports) {
            const auto& result = report.results[game];
            check(result.won && result.played.back() == expected,
                  "every strategy sees the same sampled answer for each game");
        }
    }
}

static void test_final_turn_and_trace() {
    vector<string> answers = {"apple", "apply", "ample", "maple"};
    map<string, double> weights = {{"apple", 1}, {"apply", 100}, {"ample", 1}, {"maple", 1}};
    BenchmarkOptions options;
    options.games = 20;
    options.max_guesses = 1;
    options.pool_limit = 1;
    ostringstream trace;
    write_trace_header(trace);
    auto reports = run_benchmark(answers, answers, weights, {"adaptive"}, options, &trace);
    check(reports[0].summary.failures > 0, "unsolved games at the limit are failures");
    for (const auto& result : reports[0].results) {
        check(result.guesses == 1 && result.played[0] == "apply", "final turn uses maximum answer probability");
    }
    check(trace.str().find("information_weight,distance_weight") != string::npos,
          "trace records actual adaptive weights");
    check(trace.str().find(",apply,") != string::npos && trace.str().find(",final,") != string::npos,
          "trace records selected guess and stage");
    ostringstream csv;
    write_summary_csv(csv, reports, options);
    check(csv.str().find("average_guesses_all_games") != string::npos &&
          csv.str().find("adaptive,20,42,1,1,") != string::npos,
          "summary CSV identifies denominators and experiment settings");
}

static void test_invalid_experiments_rejected() {
    BenchmarkOptions options;
    bool rejected = false;
    try { run_benchmark({"apple"}, {"apply"}, {}, {"adaptive"}, options); }
    catch (const invalid_argument&) { rejected = true; }
    check(rejected, "answers must be legal guesses");
    rejected = false;
    options.games = 0;
    try { run_benchmark({"apple"}, {"apple"}, {}, {"adaptive"}, options); }
    catch (const invalid_argument&) { rejected = true; }
    check(rejected, "zero-game experiments are rejected");
    rejected = false;
    options.games = 1;
    try { run_benchmark({"apple"}, {"apple"}, {}, {"unknown"}, options); }
    catch (const invalid_argument&) { rejected = true; }
    check(rejected, "unknown strategies are rejected");
}

static void test_shared_endgame_and_membership_risk() {
    BenchmarkOptions options;
    options.pool_limit = 0;
    mt19937 generator(42);
    for (const string& strategy : strategy_names()) {
        auto single = recommend_guesses(strategy, {"aaaaa", "bbbbb", "zzzzz"},
                                         {"zzzzz"}, {}, 5, options, generator);
        check(single.front().guess == "zzzzz", "every strategy solves a sole candidate immediately");
        auto final = recommend_guesses(strategy, {"aaaaa", "bbbbb", "zzzzz"},
                                        {"bbbbb", "zzzzz"}, {{"bbbbb", 1}, {"zzzzz", 100}},
                                        1, options, generator);
        check(final.front().guess == "zzzzz", "every strategy uses answer probability on final turn");
    }
    auto ranked = recommend_guesses("entropy", {"aa", "bb"}, {"aa", "bb"},
                                     {{"aa", 1e308}, {"bb", 1e-300}}, 3,
                                     options, generator);
    auto candidate = find_if(ranked.begin(), ranked.end(), [](const GuessAnalysis& value) {
        return value.guess == "bb";
    });
    check(candidate != ranked.end() && candidate->probability == 0 && candidate->risk == 0,
          "underflowing probability does not turn a candidate into an information-only guess");
}

int main() {
    try {
        test_game_feedback_and_budget();
        test_invalid_guess_does_not_consume_turn();
        test_summary_denominators();
        test_same_seed_repeats_games();
        test_shared_answer_sample();
        test_final_turn_and_trace();
        test_invalid_experiments_rejected();
        test_shared_endgame_and_membership_risk();
        cout << "8 benchmark tests passed\n";
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
}
