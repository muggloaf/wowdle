#include "core/candidate_filter.hpp"
#include "evaluation/benchmark.hpp"
#include "probability/prior.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif
using namespace std;

struct Options {
    string answers = "data/wordlist_fives.txt";
    string guesses;
    string frequencies = "data/word_frequencies_fives.txt";
    string strategy = "adaptive";
    string csv;
    string trace;
    bool computer = false;
    bool benchmark = false;
    bool uniform = false;
    bool help = false;
    BenchmarkOptions experiment;
};

static size_t positive_number(const string& text, const string& name) {
    if (text.empty() || !all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        throw invalid_argument(name + " needs a positive integer");
    }
    size_t consumed = 0;
    unsigned long long value = stoull(text, &consumed);
    if (value == 0 || consumed != text.size() || value > numeric_limits<size_t>::max()) {
        throw invalid_argument(name + " needs a positive integer");
    }
    return size_t(value);
}

static vector<string> split(const string& text, char delimiter) {
    vector<string> parts;
    istringstream input(text);
    string part;
    while (getline(input, part, delimiter)) parts.push_back(part);
    if (!text.empty() && text.back() == delimiter) parts.push_back("");
    return parts;
}

static void set_weights(AdaptiveConfig& config, const string& text) {
    vector<string> parts = split(text, ':');
    if (parts.size() != 2) throw invalid_argument("--weights format is stage:information,distance,likelihood,risk,reduction");
    vector<string> numbers = split(parts[1], ',');
    if (numbers.size() != 5) throw invalid_argument("--weights needs five comma-separated values");
    double values[5];
    for (size_t index = 0; index < 5; ++index) {
        size_t consumed = 0;
        values[index] = stod(numbers[index], &consumed);
        if (consumed != numbers[index].size()) throw invalid_argument("invalid adaptive weight");
    }
    AdaptiveWeights weights{values[0], values[1], values[2], values[3], values[4]};
    if (parts[0] == "many") config.many = weights;
    else if (parts[0] == "moderate") config.moderate = weights;
    else if (parts[0] == "small") config.small = weights;
    else if (parts[0] == "tiny") config.tiny = weights;
    else throw invalid_argument("weight stage must be many, moderate, small or tiny");
}

static Options parse_options(int argc, char* argv[]) {
    Options options;
    bool positional_path = false;
    for (int index = 1; index < argc; ++index) {
        string flag = argv[index];
        auto value = [&]() -> string {
            if (index + 1 >= argc) throw invalid_argument("missing value for " + flag);
            return argv[++index];
        };
        if (flag == "--computer") options.computer = true;
        else if (flag == "--benchmark") {
            options.benchmark = true;
            options.experiment.games = positive_number(value(), flag);
        } else if (flag == "--strategy") options.strategy = value();
        else if (flag == "--answers") options.answers = value();
        else if (flag == "--guesses") options.guesses = value();
        else if (flag == "--frequencies") options.frequencies = value();
        else if (flag == "--csv") options.csv = value();
        else if (flag == "--trace") options.trace = value();
        else if (flag == "--uniform") options.uniform = true;
        else if (flag == "--help" || flag == "-h") options.help = true;
        else if (flag == "--pool") {
            string text = value();
            options.experiment.pool_limit = text == "0" ? 0 : positive_number(text, flag);
        }
        else if (flag == "--max-guesses") {
            size_t turns = positive_number(value(), flag);
            if (turns > size_t(numeric_limits<int>::max())) throw invalid_argument("turn limit is too large");
            options.experiment.max_guesses = int(turns);
        } else if (flag == "--seed") {
            string text = value();
            if (text == "0") options.experiment.seed = 0;
            else {
                size_t seed = positive_number(text, flag);
                if (seed > numeric_limits<unsigned int>::max()) throw invalid_argument("seed is too large");
                options.experiment.seed = unsigned(seed);
            }
        } else if (flag == "--thresholds") {
            vector<string> thresholds = split(value(), ',');
            if (thresholds.size() != 3) throw invalid_argument("--thresholds needs many,small,tiny");
            options.experiment.adaptive.many_threshold = positive_number(thresholds[0], flag);
            options.experiment.adaptive.small_threshold = positive_number(thresholds[1], flag);
            options.experiment.adaptive.tiny_threshold = positive_number(thresholds[2], flag);
        } else if (flag == "--weights") set_weights(options.experiment.adaptive, value());
        else if (!flag.empty() && flag.front() != '-' && !positional_path) {
            options.answers = flag;
            positional_path = true;
        } else throw invalid_argument("unknown argument: " + flag);
    }
    if (options.computer && options.benchmark) throw invalid_argument("choose --computer or --benchmark");
    if (options.strategy == "all" && !options.benchmark) throw invalid_argument("--strategy all requires --benchmark");
    if (options.strategy != "all") validate_strategy(options.strategy);
    if (!options.csv.empty() && !options.benchmark && !options.computer) {
        throw invalid_argument("--csv requires --benchmark or --computer");
    }
    return options;
}

static void print_help() {
    cout << "wowdle - C++ Wordle solver\n\n"
         << "wowdle [word-list]                         manual feedback mode\n"
         << "wowdle --computer                         one computer game\n"
         << "wowdle --benchmark 100 --strategy all       compare eight strategies\n\n"
         << "--strategy NAME     random, frequency, positional, entropy, expected,\n"
         << "                    minimax, distance, adaptive (default)\n"
         << "--answers PATH      possible answers (default: data/wordlist_fives.txt)\n"
         << "--guesses PATH      valid guesses; must include every answer\n"
         << "--frequencies PATH  prior weights (default: data/word_frequencies_fives.txt)\n"
         << "--uniform           use uniform answer probabilities\n"
         << "--seed N            reproducible seed, including zero (default: 42)\n"
         << "--max-guesses N     turn limit (default: 6)\n"
         << "--pool N            guess shortlist size; 0 evaluates all (default: 100)\n"
         << "--csv PATH          write benchmark summary CSV\n"
         << "--trace PATH        write each turn's metrics and adaptive weights\n"
         << "--thresholds A,B,C  many, small and tiny candidate thresholds\n"
         << "--weights STAGE:I,D,L,R,E  override stage component weights\n\n"
         << "Manual input: g/y/x feedback, c to select another suggestion, q to quit.\n"
         << "Benchmarks sample answers uniformly with replacement and reuse the same\n"
         << "sample for each strategy. Fixed-strategy opening calculations are cached.\n";
    cout << "All strategies guess a sole candidate directly; on the final turn they\n"
         << "choose the candidate with highest answer probability.\n";
}

static string normalized_path(const string& value) {
#ifdef _WIN32
    char* absolute = _fullpath(nullptr, value.c_str(), 0);
    if (!absolute) throw invalid_argument("could not resolve path: " + value);
    string path = absolute;
    free(absolute);
    replace(path.begin(), path.end(), '\\', '/');
    transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return char(tolower(c)); });
    return path;
#else
    char* directory = getcwd(nullptr, 0);
    if (!directory) throw runtime_error("could not read current directory");
    string path = value.front() == '/' ? value : string(directory) + "/" + value;
    free(directory);
    vector<string> components;
    for (const string& part : split(path, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") { if (!components.empty()) components.pop_back(); }
        else components.push_back(part);
    }
    string result;
    for (const string& part : components) result += "/" + part;
    return result.empty() ? "/" : result;
#endif
}

static void validate_output_paths(const Options& options) {
    vector<string> inputs = {options.answers, options.guesses, options.frequencies};
    set<string> outputs;
    for (const string& output : {options.csv, options.trace}) {
        if (output.empty()) continue;
        string path = normalized_path(output);
        if (!outputs.insert(path).second) throw invalid_argument("CSV and trace must use different output paths");
        for (const string& input : inputs) {
            if (!input.empty() && normalized_path(input) == path) {
                throw invalid_argument("output path would overwrite an input file: " + output);
            }
        }
    }
}

static vector<string> load_words(const string& filename) {
    ifstream input(filename);
    if (!input) throw runtime_error("could not open word list: " + filename);
    vector<string> words;
    set<string> seen;
    string word;
    size_t length = 0;
    size_t skipped = 0;
    while (input >> word) {
        for (char& letter : word) letter = char(tolower(static_cast<unsigned char>(letter)));
        if (!all_of(word.begin(), word.end(), [](char c) { return c >= 'a' && c <= 'z'; })) {
            ++skipped;
            continue;
        }
        if (length == 0) length = word.size();
        if (word.size() != length) throw runtime_error("word list contains different word lengths");
        if (seen.insert(word).second) words.push_back(word);
    }
    if (words.empty()) throw runtime_error("word list is empty: " + filename);
    if (skipped) cout << "skipped " << skipped << " non-alphabetic entries in " << filename << '\n';
    return words;
}

static void print_summary(const BenchmarkReport& report) {
    const auto& summary = report.summary;
    cout << fixed << setprecision(3) << '\n' << report.strategy << '\n'
         << "  games: " << summary.games << "; wins: " << summary.wins
         << "; failures: " << summary.failures << '\n'
         << "  win rate: " << summary.win_rate * 100 << "%; failure rate: "
         << summary.failure_rate * 100 << "%\n"
         << "  guesses (all games, failures included): mean " << summary.average_guesses
         << ", median " << summary.median_guesses << ", worst " << summary.worst_guesses << '\n';
    if (summary.wins) cout << "  mean guesses (wins only): " << summary.average_winning_guesses << '\n';
    else cout << "  mean guesses (wins only): n/a\n";
    cout << "  mean solver runtime: " << summary.average_runtime_ms << " ms/game\n"
         << "  candidate guesses: " << summary.candidate_guesses
         << "; information-only guesses: " << summary.information_guesses << '\n';
}

static void print_recommendations(const vector<GuessAnalysis>& recommendations) {
    cout << "potential guesses:\n";
    for (size_t index = 0; index < min(size_t(5), recommendations.size()); ++index) {
        cout << "  " << index + 1 << ". " << recommendations[index].guess
             << " (score " << fixed << setprecision(4)
             << recommendations[index].combined_score << ")\n";
    }
    const auto& best = recommendations.front();
    cout << "analysis: " << best.stage << "; entropy " << best.entropy
         << " bits; expected " << best.expected_candidates << "; distance "
         << best.distance << "; risk " << best.risk << '\n';
}

static bool read_feedback(GuessAnalysis& selected,
                          const vector<GuessAnalysis>& recommendations, vector<char>& feedback) {
    string input;
    while (true) {
        cout << "feedback (g, y, x), c to change guess, q to quit: ";
        if (!getline(cin, input)) return false;
        input.erase(remove_if(input.begin(), input.end(), [](unsigned char c) { return isspace(c); }), input.end());
        for (char& mark : input) mark = char(tolower(static_cast<unsigned char>(mark)));
        if (input == "q") return false;
        if (input == "c") {
            size_t count = min(size_t(5), recommendations.size());
            cout << "choose a suggestion (1-" << count << "): ";
            if (!getline(cin, input)) return false;
            try {
                size_t number = positive_number(input, "choice");
                if (number > count) throw invalid_argument("choice out of range");
                selected = recommendations[number - 1];
                cout << "using guess: " << selected.guess << '\n';
            } catch (const exception&) {
                cout << "please choose a displayed suggestion\n";
            }
            continue;
        }
        if (input.size() != selected.guess.size() ||
            !all_of(input.begin(), input.end(), [](char c) { return c == 'g' || c == 'y' || c == 'x'; })) {
            cout << "enter one g, y or x mark per letter\n";
            continue;
        }
        feedback.assign(input.begin(), input.end());
        return true;
    }
}

static void play_manual(const vector<string>& answers, const vector<string>& valid_guesses,
                         const map<string, double>& weights, const Options& options,
                         ostream* trace) {
    vector<string> candidates = answers;
    vector<string> available = valid_guesses;
    mt19937 generator(options.experiment.seed);
    cout << "loaded " << candidates.size() << " possible answers\n";
    for (int turn = 1; turn <= options.experiment.max_guesses; ++turn) {
        int remaining = options.experiment.max_guesses - turn + 1;
        vector<GuessAnalysis> recommendations = recommend_guesses(
            options.strategy, available, candidates, weights, remaining,
            options.experiment, generator);
        GuessAnalysis selected = recommendations.front();
        cout << "\nguess " << turn << " / " << options.experiment.max_guesses
             << ": " << selected.guess << '\n';
        print_recommendations(recommendations);
        vector<char> feedback;
        vector<string> filtered;
        while (true) {
            if (!read_feedback(selected, recommendations, feedback)) {
                cout << "stopped\n";
                return;
            }
            filtered = filter_candidates(candidates, selected.guess, feedback);
            if (!filtered.empty()) break;
            cout << "no possible answer matches that feedback; check the colors and try again\n";
        }
        if (trace) write_trace_turn(*trace, options.strategy, 1, turn, candidates.size(),
                                    remaining, selected, string(feedback.begin(), feedback.end()));
        if (all_of(feedback.begin(), feedback.end(), [](char mark) { return mark == 'g'; })) {
            cout << "solved in " << turn << (turn == 1 ? " guess\n" : " guesses\n");
            return;
        }
        candidates = filtered;
        available.erase(remove(available.begin(), available.end(), selected.guess), available.end());
        cout << candidates.size() << " candidates remain\n";
    }
    cout << "guess limit reached\n";
}

int main(int argc, char* argv[]) {
    try {
        Options options = parse_options(argc, argv);
        if (options.help) { print_help(); return 0; }
        validate_output_paths(options);
        vector<string> answers = load_words(options.answers);
        vector<string> guesses = options.guesses.empty() ? answers : load_words(options.guesses);
        set<string> allowed(guesses.begin(), guesses.end());
        for (const string& answer : answers) {
            if (answer.size() != guesses.front().size() || !allowed.count(answer)) {
                throw invalid_argument("valid guess dictionary must include every answer");
            }
        }
        map<string, double> weights;
        if (!options.uniform) {
            ifstream frequency_file(options.frequencies);
            if (frequency_file) weights = load_word_weights(options.frequencies);
            else cout << "no frequency file found; using uniform answer probabilities\n";
        }
        ofstream trace_file;
        ostream* trace = nullptr;
        if (!options.trace.empty()) {
            trace_file.open(options.trace);
            if (!trace_file) throw runtime_error("could not write trace: " + options.trace);
            trace = &trace_file;
            write_trace_header(*trace);
        }
        if (!options.benchmark && !options.computer) {
            play_manual(answers, guesses, weights, options, trace);
            return 0;
        }
        if (options.computer) options.experiment.games = 1;
        vector<string> strategies = options.strategy == "all" ? strategy_names()
                                                               : vector<string>{options.strategy};
        cout << "answers: " << answers.size() << "; valid guesses: " << guesses.size()
             << "; seed: " << options.experiment.seed << "; pool limit: "
             << options.experiment.pool_limit << '\n';
        cout << "sampling: uniform answers with replacement; shared sample across strategies\n";
        cout << "runtime includes one cached opening calculation per fixed strategy\n";
        vector<BenchmarkReport> reports = run_benchmark(answers, guesses, weights, strategies,
                                                       options.experiment, trace);
        if (options.computer) {
            const auto& game = reports.front().results.front();
            for (size_t turn = 0; turn < game.played.size(); ++turn) {
                cout << "guess " << turn + 1 << ": " << game.played[turn]
                     << " -> " << game.feedback[turn] << '\n';
            }
            cout << (game.won ? "solved\n" : "guess limit reached\n");
        }
        for (const auto& report : reports) print_summary(report);
        if (!options.csv.empty()) {
            ofstream csv(options.csv);
            if (!csv) throw runtime_error("could not write CSV: " + options.csv);
            write_summary_csv(csv, reports, options.experiment);
            cout << "\nwrote " << options.csv << '\n';
        }
        return 0;
    } catch (const exception& error) {
        cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
