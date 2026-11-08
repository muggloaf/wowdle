#include "core/candidate_filter.hpp"
#include "probability/prior.hpp"
#include "strategies/entropy.hpp"
#include <sstream>
#include <stdexcept>
using namespace std;

namespace {

vector<string> all_words;
vector<string> candidates;
map<string, double> weights;
string response;

vector<string> read_words(const string& text) {
    istringstream input(text);
    vector<string> words;
    string word;
    while (input >> word) {
        words.push_back(word);
    }
    return words;
}

map<string, double> read_weights(const string& text) {
    istringstream input(text);
    map<string, double> result;
    string word;
    double weight;
    while (input >> word >> weight) {
        result[word] = weight;
    }
    return result;
}

string json_string(const string& value) {
    string escaped;
    for (char letter : value) {
        if (letter == '"' || letter == '\\') {
            escaped += '\\';
        }
        escaped += letter;
    }
    return "\"" + escaped + "\"";
}

const char* make_response() {
    vector<pair<string, double>> likely_answers =
        rank_candidates(candidates, weights);
    vector<string> guesses;
    size_t limit = likely_answers.size();
    if (limit > 100) {
        limit = 100;
    }
    for (size_t index = 0; index < limit; ++index) {
        guesses.push_back(likely_answers[index].first);
    }

    vector<pair<string, double>> recommendations =
        rank_combined_guesses(guesses, candidates, weights);
    response = "{\"guess\":" + json_string(recommendations.front().first);
    response += ",\"candidates\":" + to_string(candidates.size());
    response += ",\"recommendations\":[";

    size_t recommendation_count = recommendations.size();
    if (recommendation_count > 5) {
        recommendation_count = 5;
    }
    for (size_t index = 0; index < recommendation_count; ++index) {
        if (index > 0) {
            response += ",";
        }
        response += "{\"word\":" + json_string(recommendations[index].first);
        response += ",\"score\":" + to_string(recommendations[index].second);
        response += "}";
    }
    response += "]}";
    return response.c_str();
}

} // namespace

extern "C" {

void wowdle_init(const char* word_text, const char* weight_text) {
    all_words = read_words(word_text);
    candidates = all_words;
    weights = read_weights(weight_text);
    response.clear();
}

const char* wowdle_recommend() {
    if (candidates.empty()) {
        return "{\"error\":\"no candidates remain\"}";
    }
    return make_response();
}

const char* wowdle_submit(const char* guess, const char* feedback) {
    vector<char> marks;
    string feedback_text = feedback;
    for (char mark : feedback_text) {
        marks.push_back(mark);
    }

    candidates = filter_candidates(candidates, guess, marks);
    if (candidates.empty()) {
        response = "{\"error\":\"no candidates remain\"}";
        return response.c_str();
    }
    return make_response();
}

}
