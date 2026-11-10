#ifndef WOWDLE_BENCHMARK_H
#define WOWDLE_BENCHMARK_H

#include "strategies/adaptive.hpp"
#include <map>
#include <ostream>
#include <random>
#include <string>
#include <vector>
using namespace std;

class ComputerGame {
    string secret;
    vector<string> allowed;
    int limit;
    int attempts = 0;
    bool won = false;

public:
    ComputerGame(const string& answer, const vector<string>& valid_guesses,
                 int max_guesses = 6);
    vector<char> submit(const string& guess);
    int guesses_remaining() const;
    bool finished() const;
    bool solved() const;
};

struct BenchmarkOptions {
    size_t games = 100;
    unsigned int seed = 42;
    int max_guesses = 6;
    size_t pool_limit = 100;
    AdaptiveConfig adaptive;
};

struct GameResult {
    bool won = false;
    int guesses = 0;
    int candidate_guesses = 0;
    int information_guesses = 0;
    double runtime_ms = 0;
    vector<string> played;
    vector<string> feedback;
};

struct BenchmarkSummary {
    size_t games = 0;
    size_t wins = 0;
    size_t failures = 0;
    double average_guesses = 0;
    double median_guesses = 0;
    double average_winning_guesses = 0;
    double win_rate = 0;
    double failure_rate = 0;
    int worst_guesses = 0;
    double average_runtime_ms = 0;
    size_t candidate_guesses = 0;
    size_t information_guesses = 0;
};

struct BenchmarkReport {
    string strategy;
    BenchmarkSummary summary;
    vector<GameResult> results;
};

vector<string> strategy_names();
void validate_strategy(const string& strategy);
vector<GuessAnalysis> recommend_guesses(
    const string& strategy, const vector<string>& valid_guesses,
    const vector<string>& candidates, const map<string, double>& weights,
    int guesses_remaining, const BenchmarkOptions& options, mt19937& generator);
GameResult play_game(
    ComputerGame& game, const vector<string>& answers,
    const vector<string>& valid_guesses, const map<string, double>& weights,
    const string& strategy, const BenchmarkOptions& options, mt19937& generator,
    ostream* trace = nullptr, size_t game_number = 1,
    const GuessAnalysis* opening = nullptr);
BenchmarkSummary summarize_games(const vector<GameResult>& results);
vector<BenchmarkReport> run_benchmark(
    const vector<string>& answers, const vector<string>& valid_guesses,
    const map<string, double>& weights, const vector<string>& strategies,
    const BenchmarkOptions& options, ostream* trace = nullptr);
void write_trace_header(ostream& output);
void write_trace_turn(ostream& output, const string& strategy, size_t game_number,
                      int turn, size_t candidate_count, int guesses_remaining,
                      const GuessAnalysis& analysis, const string& feedback);
void write_summary_csv(ostream& output, const vector<BenchmarkReport>& reports,
                       const BenchmarkOptions& options);

#endif
