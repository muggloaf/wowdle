#include "core/candidate_filter.hpp"
#include "core/feedback.hpp"
#include "probability/prior.hpp"
#include "strategies/entropy.hpp"
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
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

bool read_feedback(string& guess,
                   const vector<pair<string, double>>& recommendations,
                   vector<char>& feedback) {
    string input;
    while (true) {
        cout << "feedback (g, y, x), c to change guess, q to quit: ";
        cin >> input;

        if (input == "q") {
            return false;
        }

        if (input == "c") {
            size_t choice_count = recommendations.size();
            if (choice_count > 5) {
                choice_count = 5;
            }
            cout << "choose a potential guess from the list (1-";
            cout << choice_count << "): ";

            string choice;
            cin >> choice;
            if (choice.size() != 1 || choice[0] < '1' || choice[0] > '5') {
                cout << "please choose a number from 1 to 5\n";
                continue;
            }

            size_t index = choice[0] - '1';
            if (index >= recommendations.size()) {
                cout << "that guess is not available\n";
                continue;
            }

            guess = recommendations[index].first;
            cout << "using guess: " << guess << '\n';
            continue;
        }

        if (input.size() != guess.size()) {
            cout << "feedback must have one mark per letter\n";
            continue;
        }

        bool valid = true;
        for (char mark : input) {
            if (mark != 'g' && mark != 'y' && mark != 'x') {
                valid = false;
                break;
            }
        }

        if (!valid) {
            cout << "feedback can only contain g, y, and x\n";
            continue;
        }

        feedback = vector<char>(input.begin(), input.end());
        return true;
    }
}

void wait_for_exit() {
    cout << "press enter to exit...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

bool solved(const vector<char>& feedback) {
    for (char mark : feedback) {
        if (mark != 'g') {
            return false;
        }
    }
    return true;
}

void print_recommendations(
    const vector<pair<string, double>>& recommendations) {
    cout << "potential guesses:\n";
    size_t limit = recommendations.size();
    if (limit > 5) {
        limit = 5;
    }
    for (size_t i = 0; i < limit; ++i) {
        cout << "  " << i + 1 << ". " << recommendations[i].first << " ("
             << recommendations[i].second * 100 << " score)\n";
    }
}

vector<string> entropy_guess_pool(
    const vector<pair<string, double>>& ranked) {
    const size_t max_entropy_guesses = 500;
    size_t limit = ranked.size();
    if (limit > max_entropy_guesses) {
        limit = max_entropy_guesses;
    }

    vector<string> guesses;
    for (size_t i = 0; i < limit; ++i) {
        guesses.push_back(ranked[i].first);
    }
    return guesses;
}

string feedback_text(const vector<char>& feedback) {
    return string(feedback.begin(), feedback.end());
}

void play_against_computer(const vector<string>& words,
                           const map<string, double>& weights) {
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
        vector<pair<string, double>> likely_answers =
            rank_candidates(candidates, weights);
        vector<string> guesses = entropy_guess_pool(likely_answers);
        vector<pair<string, double>> recommendations =
            rank_combined_guesses(guesses, candidates, weights);
        string guess = recommendations.front().first;
        vector<char> feedback = evaluate_feedback(guess, secret);

        cout << "guess " << turn << ": " << guess << '\n';
        cout << "feedback: " << feedback_text(feedback) << '\n';
        print_recommendations(recommendations);

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
        map<string, double> weights;
        ifstream frequency_file("data/word_frequencies_fives.txt");
        if (frequency_file) {
            weights = load_word_weights("data/word_frequencies_fives.txt");
            cout << "loaded word frequencies\n";
        } else {
            cout << "no word frequencies found; using uniform probabilities\n";
        }

        if (computer_mode) {
            play_against_computer(words, weights);
            return 0;
        }

        vector<string> candidates = words;
        int turn = 1;

        cout << "loaded " << candidates.size() << " words\n";
        cout << "type q instead of feedback to quit\n\n";

        while (!candidates.empty()) {
            vector<pair<string, double>> likely_answers =
                rank_candidates(candidates, weights);
            vector<string> guesses = entropy_guess_pool(likely_answers);
            vector<pair<string, double>> recommendations =
                rank_combined_guesses(guesses, candidates, weights);
            string guess = recommendations.front().first;
            cout << "guess " << turn << ": " << guess << '\n';
            print_recommendations(recommendations);

            vector<char> feedback;
            if (!read_feedback(guess, recommendations, feedback)) {
                cout << "stopped\n";
                wait_for_exit();
                return 0;
            }

            if (solved(feedback)) {
                cout << "solved in " << turn << " guess";
                if (turn != 1) {
                    cout << "es";
                }
                cout << '\n';
                wait_for_exit();
                return 0;
            }

            candidates = filter_candidates(candidates, guess, feedback);
            cout << candidates.size() << " candidates remain\n\n";
            ++turn;
        }

        cout << "no candidates match that feedback\n";
    wait_for_exit();
    } catch (const exception& error) {
        cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
