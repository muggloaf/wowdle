#include "core/candidate_filter.hpp"
#include "core/feedback.hpp"
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace std;

vector<string> load_words(const string& filename) {
    ifstream file(filename);
    if (!file) {
        throw runtime_error("could not open word list: " + filename);
    }

    vector<string> words;
    string word;
    size_t word_length = 0;

    while (file >> word) {
        for (char& letter : word) {
            letter = tolower(letter);
        }

        if (word.empty()) {
            continue;
        }

        if (word_length == 0) {
            word_length = word.size();
        } else if (word.size() != word_length) {
            throw runtime_error("word list contains different word lengths");
        }

        words.push_back(word);
    }

    if (words.empty()) {
        throw runtime_error("word list is empty");
    }

    return words;
}

vector<char> read_feedback(const string& guess) {
    string input;
    cout << "feedback (g, y, x): ";
    cin >> input;

    if (input == "q") {
        return {};
    }

    if (input.size() != guess.size()) {
        throw invalid_argument("feedback must have one mark per letter");
    }

    for (char mark : input) {
        if (mark != 'g' && mark != 'y' && mark != 'x') {
            throw invalid_argument("feedback can only contain g, y, and x");
        }
    }

    return vector<char>(input.begin(), input.end());
}

bool solved(const vector<char>& feedback) {
    for (char mark : feedback) {
        if (mark != 'g') {
            return false;
        }
    }
    return true;
}

string feedback_text(const vector<char>& feedback) {
    return string(feedback.begin(), feedback.end());
}

void play_against_computer(const vector<string>& words) {
    random_device random;
    auto now = chrono::high_resolution_clock::now().time_since_epoch().count();
    mt19937 generator(random() ^ now);
    uniform_int_distribution<size_t> pick_word(0, words.size() - 1);
    string secret = words[pick_word(generator)];
    vector<string> candidates = words;
    int turn = 1;

    cout << "computer mode\n";
    cout << "the computer has picked a secret word\n\n";

    while (!candidates.empty()) {
        string guess = candidates.front();
        vector<char> feedback = evaluate_feedback(guess, secret);

        cout << "guess " << turn << ": " << guess << '\n';
        cout << "feedback: " << feedback_text(feedback) << '\n';

        if (solved(feedback)) {
            cout << "solved in " << turn << " guess";
            if (turn != 1) {
                cout << "es";
            }
            cout << "\nsecret: " << secret << '\n';
            return;
        }

        candidates = filter_candidates(candidates, guess, feedback);
        cout << candidates.size() << " candidates remain\n\n";
        ++turn;
    }

    cout << "the solver could not find the answer\n";
    cout << "secret: " << secret << '\n';
}

int main(int argc, char* argv[]) {
    string filename = "data/wordlist_fives.txt";
    bool computer_mode = false;

    if (argc > 1 && string(argv[1]) == "--computer") {
        computer_mode = true;
        if (argc > 2) {
            filename = argv[2];
        }
    } else if (argc > 1) {
        filename = argv[1];
    }

    try {
        vector<string> words = load_words(filename);

        if (computer_mode) {
            play_against_computer(words);
            return 0;
        }

        vector<string> candidates = words;
        int turn = 1;

        cout << "loaded " << candidates.size() << " words\n";
        cout << "type q instead of feedback to quit\n\n";

        while (!candidates.empty()) {
            string guess = candidates.front();
            cout << "guess " << turn << ": " << guess << '\n';

            vector<char> feedback = read_feedback(guess);
            if (feedback.empty()) {
                cout << "stopped\n";
                return 0;
            }

            if (solved(feedback)) {
                cout << "solved in " << turn << " guess";
                if (turn != 1) {
                    cout << "es";
                }
                cout << '\n';
                return 0;
            }

            candidates = filter_candidates(candidates, guess, feedback);
            cout << candidates.size() << " candidates remain\n\n";
            ++turn;
        }

        cout << "no candidates match that feedback\n";
    } catch (const exception& error) {
        cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
