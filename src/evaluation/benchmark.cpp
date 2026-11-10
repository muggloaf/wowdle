#include "evaluation/benchmark.hpp"
#include "core/candidate_filter.hpp"
#include "core/feedback.hpp"
#include "probability/prior.hpp"
#include "strategies/scoring.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <set>
#include <stdexcept>
using namespace std;

static bool valid_word(const string& word) {
    return !word.empty() && all_of(word.begin(), word.end(), [](char letter) {
        return letter >= 'a' && letter <= 'z';
    });
}

ComputerGame::ComputerGame(const string& answer,
                           const vector<string>& valid_guesses, int max_guesses)
    : secret(answer), allowed(valid_guesses), limit(max_guesses) {
    if (!valid_word(answer) || limit <= 0 ||
        find(allowed.begin(), allowed.end(), answer) == allowed.end()) {
        throw invalid_argument("game needs a valid answer and a positive turn limit");
    }
}

vector<char> ComputerGame::submit(const string& guess) {
    if (finished()) {
        throw logic_error("the game has finished");
    }
    if (!valid_word(guess) || guess.size() != secret.size() ||
        find(allowed.begin(), allowed.end(), guess) == allowed.end()) {
        throw invalid_argument("guess is not in the valid guess dictionary");
    }
    vector<char> feedback = evaluate_feedback(guess, secret);
    ++attempts;
    won = all_of(feedback.begin(), feedback.end(), [](char mark) { return mark == 'g'; });
    return feedback;
}

int ComputerGame::guesses_remaining() const { return limit - attempts; }
bool ComputerGame::finished() const { return won || attempts == limit; }
bool ComputerGame::solved() const { return won; }

vector<string> strategy_names() {
    return {"random", "frequency", "positional", "entropy", "expected",
            "minimax", "distance", "adaptive"};
}

void validate_strategy(const string& strategy) {
    vector<string> names = strategy_names();
    if (find(names.begin(), names.end(), strategy) == names.end()) {
        throw invalid_argument("unknown strategy: " + strategy);
    }
}

vector<GuessAnalysis> recommend_guesses(
    const string& strategy, const vector<string>& valid_guesses,
    const vector<string>& candidates, const map<string, double>& weights,
    int guesses_remaining, const BenchmarkOptions& options, mt19937& generator) {
    validate_strategy(strategy);
    if (candidates.empty() || valid_guesses.empty() || guesses_remaining <= 0) {
        throw invalid_argument("recommendations need candidates, guesses and remaining turns");
    }
    vector<string> pool = make_guess_pool(valid_guesses, candidates, weights,
                                          options.pool_limit);
    if (strategy == "adaptive" || candidates.size() == 1 || guesses_remaining == 1) {
        return rank_adaptive_guesses(pool, candidates, weights, guesses_remaining,
                                     options.adaptive);
    }

    vector<pair<string, double>> ranked;
    if (strategy == "random") {
        vector<string> shuffled = candidates;
        shuffle(shuffled.begin(), shuffled.end(), generator);
        for (size_t index = 0; index < shuffled.size(); ++index) {
            ranked.push_back({shuffled[index], 1.0 - double(index) / shuffled.size()});
        }
    } else {
        ranked = rank_baseline_guesses(strategy, pool, candidates);
    }

    map<string, double> probabilities;
    double highest_probability = 0;
    for (const auto& item : rank_candidates(candidates, weights)) {
        probabilities[item.first] = item.second;
        highest_probability = max(highest_probability, item.second);
    }
    vector<GuessAnalysis> result;
    size_t count = min(size_t(5), ranked.size());
    for (size_t index = 0; index < count; ++index) {
        const auto& item = ranked[index];
        GuessMetrics metrics = calculate_guess_metrics(item.first, candidates);
        GuessAnalysis analysis{};
        analysis.guess = item.first;
        analysis.entropy = metrics.entropy;
        analysis.expected_candidates = metrics.expected_candidates;
        analysis.worst_case = metrics.worst_case;
        analysis.distance = metrics.distance;
        analysis.probability = probabilities[item.first];
        bool candidate = find(candidates.begin(), candidates.end(), item.first) != candidates.end();
        analysis.risk = candidate ? 0 : 1.0 / guesses_remaining;
        double entropy_bound = min(log2(double(candidates.size())),
                                   item.first.size() * log2(3.0));
        analysis.information_score = entropy_bound > 0
            ? min(1.0, metrics.entropy / entropy_bound) : 0;
        analysis.reduction_score = candidates.size() > 1
            ? (candidates.size() - metrics.expected_candidates) / (candidates.size() - 1) : 0;
        analysis.likelihood_score = highest_probability > 0
            ? probabilities[item.first] / highest_probability : 0;
        analysis.combined_score = item.second;
        analysis.stage = strategy;
        result.push_back(analysis);
    }
    return result;
}

void write_trace_header(ostream& output) {
    output << "strategy,game,turn,candidate_count,guesses_remaining,selected_guess,feedback,"
              "selected_stage,entropy_bits,expected_candidates,worst_case,distance,"
              "answer_probability,risk,information_score,reduction_score,likelihood_score,"
              "combined_score,information_weight,distance_weight,likelihood_weight,"
              "risk_weight,reduction_weight\n";
}

void write_trace_turn(ostream& output, const string& strategy, size_t game_number,
                      int turn, size_t candidate_count, int guesses_remaining,
                      const GuessAnalysis& analysis, const string& feedback) {
    output << setprecision(12) << strategy << ',' << game_number << ',' << turn << ','
           << candidate_count << ',' << guesses_remaining << ',' << analysis.guess << ','
           << feedback << ',' << analysis.stage << ',' << analysis.entropy << ','
           << analysis.expected_candidates << ',' << analysis.worst_case << ','
           << analysis.distance << ',' << analysis.probability << ',' << analysis.risk << ','
           << analysis.information_score << ',' << analysis.reduction_score << ','
           << analysis.likelihood_score << ',' << analysis.combined_score << ','
           << analysis.weights.information << ',' << analysis.weights.distance << ','
           << analysis.weights.likelihood << ',' << analysis.weights.risk << ','
           << analysis.weights.reduction << '\n';
}

GameResult play_game(
    ComputerGame& game, const vector<string>& answers,
    const vector<string>& valid_guesses, const map<string, double>& weights,
    const string& strategy, const BenchmarkOptions& options, mt19937& generator,
    ostream* trace, size_t game_number, const GuessAnalysis* opening) {
    if (answers.empty() || game.finished()) {
        throw invalid_argument("cannot start a game without possible answers or turns");
    }
    vector<string> candidates = answers;
    vector<string> available = valid_guesses;
    GameResult result;
    auto start = chrono::steady_clock::now();
    while (!game.finished() && !candidates.empty()) {
        int remaining = game.guesses_remaining();
        GuessAnalysis analysis = result.guesses == 0 && opening ? *opening
            : recommend_guesses(strategy, available, candidates, weights, remaining,
                                 options, generator).front();
        bool candidate = find(candidates.begin(), candidates.end(), analysis.guess)
                         != candidates.end();
        vector<char> feedback = game.submit(analysis.guess);
        ++result.guesses;
        result.candidate_guesses += candidate;
        result.information_guesses += !candidate;
        result.played.push_back(analysis.guess);
        result.feedback.emplace_back(feedback.begin(), feedback.end());
        if (trace) {
            write_trace_turn(*trace, strategy, game_number, result.guesses,
                             candidates.size(), remaining, analysis,
                             string(feedback.begin(), feedback.end()));
        }
        if (game.solved()) {
            result.won = true;
            break;
        }
        candidates = filter_candidates(candidates, analysis.guess, feedback);
        available.erase(remove(available.begin(), available.end(), analysis.guess),
                        available.end());
    }
    result.runtime_ms = chrono::duration<double, milli>(chrono::steady_clock::now() - start).count();
    return result;
}

BenchmarkSummary summarize_games(const vector<GameResult>& results) {
    BenchmarkSummary summary;
    summary.games = results.size();
    if (results.empty()) return summary;
    vector<int> attempts;
    double winning_guesses = 0;
    for (const GameResult& result : results) {
        if (result.guesses < 0 || result.candidate_guesses < 0 ||
            result.information_guesses < 0 || result.runtime_ms < 0) {
            throw invalid_argument("game results cannot contain negative values");
        }
        summary.wins += result.won;
        summary.average_guesses += result.guesses;
        winning_guesses += result.won ? result.guesses : 0;
        summary.worst_guesses = max(summary.worst_guesses, result.guesses);
        summary.average_runtime_ms += result.runtime_ms;
        summary.candidate_guesses += result.candidate_guesses;
        summary.information_guesses += result.information_guesses;
        attempts.push_back(result.guesses);
    }
    sort(attempts.begin(), attempts.end());
    size_t middle = attempts.size() / 2;
    summary.median_guesses = attempts.size() % 2 ? attempts[middle]
        : (attempts[middle - 1] + attempts[middle]) / 2.0;
    summary.failures = summary.games - summary.wins;
    summary.average_guesses /= summary.games;
    summary.average_runtime_ms /= summary.games;
    summary.average_winning_guesses = summary.wins ? winning_guesses / summary.wins : 0;
    summary.win_rate = double(summary.wins) / summary.games;
    summary.failure_rate = double(summary.failures) / summary.games;
    return summary;
}

static void validate_dictionaries(const vector<string>& answers,
                                  const vector<string>& valid_guesses) {
    if (answers.empty() || valid_guesses.empty()) {
        throw invalid_argument("answer and valid guess dictionaries cannot be empty");
    }
    set<string> allowed(valid_guesses.begin(), valid_guesses.end());
    size_t length = answers.front().size();
    for (const auto& dictionary : {answers, valid_guesses}) {
        set<string> seen;
        for (const string& word : dictionary) {
            if (!valid_word(word) || word.size() != length || !seen.insert(word).second) {
                throw invalid_argument("dictionaries need unique, equal-length lowercase words");
            }
        }
    }
    for (const string& word : answers) {
        if (!allowed.count(word)) {
            throw invalid_argument("answer is missing from the valid guess dictionary: " + word);
        }
    }
}

vector<BenchmarkReport> run_benchmark(
    const vector<string>& answers, const vector<string>& valid_guesses,
    const map<string, double>& weights, const vector<string>& strategies,
    const BenchmarkOptions& options, ostream* trace) {
    validate_dictionaries(answers, valid_guesses);
    if (options.games == 0 || options.max_guesses <= 0 || strategies.empty()) {
        throw invalid_argument("benchmark needs games, strategies and positive limits");
    }
    for (const string& strategy : strategies) validate_strategy(strategy);
    mt19937 sample_generator(options.seed);
    uniform_int_distribution<size_t> sample(0, answers.size() - 1);
    vector<size_t> selected;
    for (size_t index = 0; index < options.games; ++index) {
        selected.push_back(sample(sample_generator));
    }
    vector<BenchmarkReport> reports;
    for (const string& strategy : strategies) {
        BenchmarkReport report;
        report.strategy = strategy;
        mt19937 guess_generator(options.seed);
        GuessAnalysis opening{};
        double opening_ms = 0;
        if (strategy != "random") {
            auto start = chrono::steady_clock::now();
            opening = recommend_guesses(strategy, valid_guesses, answers, weights,
                                        options.max_guesses, options, guess_generator).front();
            opening_ms = chrono::duration<double, milli>(chrono::steady_clock::now() - start).count();
        }
        for (size_t index = 0; index < selected.size(); ++index) {
            ComputerGame game(answers[selected[index]], valid_guesses, options.max_guesses);
            GameResult result = play_game(game, answers, valid_guesses, weights, strategy,
                                          options, guess_generator, trace, index + 1,
                                          strategy == "random" ? nullptr : &opening);
            if (index == 0) result.runtime_ms += opening_ms;
            report.results.push_back(result);
        }
        report.summary = summarize_games(report.results);
        reports.push_back(report);
    }
    return reports;
}

void write_summary_csv(ostream& output, const vector<BenchmarkReport>& reports,
                       const BenchmarkOptions& options) {
    output << "strategy,games,seed,max_guesses,pool_limit,wins,failures,win_rate,failure_rate,"
              "average_guesses_all_games,median_guesses_all_games,average_guesses_wins,"
              "worst_guesses,average_runtime_ms,candidate_guesses,information_guesses\n";
    for (const BenchmarkReport& report : reports) {
        const auto& s = report.summary;
        output << setprecision(12) << report.strategy << ',' << s.games << ',' << options.seed
               << ',' << options.max_guesses << ',' << options.pool_limit << ',' << s.wins
               << ',' << s.failures << ',' << s.win_rate << ',' << s.failure_rate << ','
               << s.average_guesses << ',' << s.median_guesses << ',';
        if (s.wins) output << s.average_winning_guesses;
        output << ',' << s.worst_guesses << ',' << s.average_runtime_ms << ','
               << s.candidate_guesses << ',' << s.information_guesses << '\n';
    }
}
