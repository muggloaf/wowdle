#include "core/candidate_filter.hpp"
#include "probability/prior.hpp"
#include "strategies/adaptive.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace std;

namespace {

vector<string> all_words;
vector<string> candidates;
map<string, double> weights;
vector<pair<string, string>> history;
string response;
bool initialized = false;
size_t skipped_words = 0;
const int max_guesses = 6;

string json_string(const string& value) {
    string escaped = "\"";
    for (unsigned char letter : value) {
        if (letter == '"' || letter == '\\') escaped += '\\';
        if (letter >= 32) escaped += letter;
    }
    return escaped + "\"";
}

const char* error_response(const string& message) {
    response = "{\"error\":" + json_string(message) + "}";
    return response.c_str();
}

string normalize_word(string word) {
    if (word.size() != 5) throw invalid_argument("enter a five-letter guess");
    for (char& letter : word) {
        if (letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
        if (letter < 'a' || letter > 'z') {
            throw invalid_argument("guesses must contain only letters a-z");
        }
    }
    return word;
}

bool valid_dictionary_word(const string& word) {
    return word.size() == 5 && all_of(word.begin(), word.end(), [](char letter) {
        return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
    });
}

bool is_solved(const vector<pair<string, string>>& turns) {
    return !turns.empty() && turns.back().second == "ggggg";
}

vector<string> apply_turn(const vector<string>& remaining,
                          const string& guess, const string& feedback) {
    if (feedback.size() != 5 || feedback.find_first_not_of("gyx") != string::npos) {
        throw invalid_argument("enter five feedback marks using g, y, and x");
    }
    vector<string> next = filter_candidates(
        remaining, guess, vector<char>(feedback.begin(), feedback.end()));
    if (next.empty()) {
        throw invalid_argument("no candidates match that feedback; check the colors or the dictionary");
    }
    return next;
}

void write_weights(ostream& out, const AdaptiveWeights& values) {
    out << "{\"information\":" << values.information
        << ",\"distance\":" << values.distance
        << ",\"likelihood\":" << values.likelihood
        << ",\"risk\":" << values.risk
        << ",\"reduction\":" << values.reduction << '}';
}

void write_analysis(ostream& out, const GuessAnalysis& value) {
    out << "{\"guess\":" << json_string(value.guess)
        << ",\"entropy\":" << value.entropy
        << ",\"expected_candidates\":" << value.expected_candidates
        << ",\"worst_case\":" << value.worst_case
        << ",\"distance\":" << value.distance
        << ",\"probability\":" << value.probability
        << ",\"risk\":" << value.risk
        << ",\"information_score\":" << value.information_score
        << ",\"reduction_score\":" << value.reduction_score
        << ",\"likelihood_score\":" << value.likelihood_score
        << ",\"combined_score\":" << value.combined_score
        << ",\"stage\":" << json_string(value.stage) << ",\"weights\":";
    write_weights(out, value.weights);
    out << '}';
}

const char* make_response() {
    if (!initialized) return error_response("solver is not initialized");
    bool won = is_solved(history);
    int remaining = max_guesses - static_cast<int>(history.size());
    string status = won ? "won" : remaining == 0 ? "lost" : "playing";
    vector<GuessAnalysis> recommendations;
    if (status == "playing") {
        vector<string> pool = make_guess_pool(all_words, candidates, weights);
        recommendations = rank_adaptive_guesses(pool, candidates, weights, remaining);
    }
    ostringstream out;
    out << setprecision(12) << "{\"status\":" << json_string(status)
        << ",\"guess\":" << json_string(won ? history.back().first :
            recommendations.empty() ? "" : recommendations.front().guess)
        << ",\"candidates\":" << candidates.size()
        << ",\"dictionary_size\":" << all_words.size() << ",\"skipped_words\":" << skipped_words
        << ",\"guesses_remaining\":" << remaining << ",\"max_guesses\":" << max_guesses
        << ",\"history\":[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i) out << ',';
        out << "{\"guess\":" << json_string(history[i].first)
            << ",\"feedback\":" << json_string(history[i].second) << '}';
    }
    out << "],\"recommendations\":[";
    for (size_t i = 0; i < min(size_t(5), recommendations.size()); ++i) {
        if (i) out << ',';
        out << "{\"word\":" << json_string(recommendations[i].guess)
            << ",\"score\":" << recommendations[i].combined_score << ",\"analysis\":";
        write_analysis(out, recommendations[i]);
        out << '}';
    }
    out << "],\"analysis\":";
    if (recommendations.empty()) out << "null";
    else write_analysis(out, recommendations.front());
    out << ",\"answers\":[";
    vector<pair<string, double>> answers = rank_candidates(candidates, weights);
    for (size_t i = 0; i < min(size_t(10), answers.size()); ++i) {
        if (i) out << ',';
        out << "{\"word\":" << json_string(answers[i].first)
            << ",\"probability\":" << answers[i].second << '}';
    }
    out << "]}";
    response = out.str();
    return response.c_str();
}

} // namespace

extern "C" {

void wowdle_init(const char* word_text, const char* weight_text) {
    initialized = false;
    all_words.clear();
    candidates.clear();
    weights.clear();
    history.clear();
    skipped_words = 0;
    try {
        if (!word_text || !weight_text) throw invalid_argument("missing dictionary data");
        istringstream input(word_text);
        string word;
        set<string> seen;
        while (input >> word) {
            if (!valid_dictionary_word(word)) { ++skipped_words; continue; }
            word = normalize_word(word);
            if (seen.insert(word).second) all_words.push_back(word);
        }
        if (all_words.empty()) throw invalid_argument("answer dictionary is empty");
        istringstream frequency_input(weight_text);
        while (frequency_input >> word) {
            double weight;
            if (!(frequency_input >> weight) || !isfinite(weight) || weight < 0) {
                throw invalid_argument("invalid word frequency data");
            }
            if (valid_dictionary_word(word)) weights[normalize_word(word)] = weight;
        }
        candidates = all_words;
        double probability_sum = 0;
        for (const auto& answer : rank_candidates(candidates, weights)) {
            if (!isfinite(answer.second)) throw invalid_argument("word frequencies cannot be normalized");
            probability_sum += answer.second;
        }
        if (!(probability_sum > 0)) throw invalid_argument("word frequencies cannot be normalized");
        initialized = true;
        response.clear();
    } catch (const exception& error) {
        error_response(error.what());
    }
}

const char* wowdle_recommend() {
    try {
        if (!initialized && !response.empty()) return response.c_str();
        return make_response();
    } catch (const exception& error) {
        return error_response(error.what());
    }
}

const char* wowdle_submit(const char* guess, const char* feedback) {
    try {
        if (!initialized) throw invalid_argument("solver is not initialized");
        if (!guess || !feedback) throw invalid_argument("missing guess or feedback");
        if (is_solved(history) || history.size() >= max_guesses) {
            throw invalid_argument("this game has ended; start another game");
        }
        string word = normalize_word(guess);
        vector<string> next = apply_turn(candidates, word, feedback);
        candidates = std::move(next);
        history.push_back({word, feedback});
        return make_response();
    } catch (const exception& error) {
        return error_response(error.what());
    }
}

const char* wowdle_restore(const char* history_text) {
    try {
        if (!initialized) throw invalid_argument("solver is not initialized");
        if (!history_text) throw invalid_argument("missing game history");
        vector<string> restored = all_words;
        vector<pair<string, string>> turns;
        istringstream input(history_text);
        string guess, feedback;
        while (input >> guess) {
            if (!(input >> feedback) || turns.size() >= max_guesses || is_solved(turns)) {
                throw invalid_argument("invalid game history");
            }
            guess = normalize_word(guess);
            restored = apply_turn(restored, guess, feedback);
            turns.push_back({guess, feedback});
        }
        candidates = std::move(restored);
        history = std::move(turns);
        return make_response();
    } catch (const exception& error) {
        return error_response(error.what());
    }
}

}
