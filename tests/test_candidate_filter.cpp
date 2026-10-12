#include "../src/core/candidate_filter.hpp"
#include <iostream>
#include <stdexcept>
using namespace std;

void check(const vector<string>& actual, const vector<string>& expected,
           const string& test_name) {
    if (actual != expected) {
        throw runtime_error("candidate filter mismatch in " + test_name);
    }
}

void test_filters_green_letters() {
    vector<string> candidates = {"slate", "sloth", "share", "crane"};

    check(filter_candidates(candidates, "slate", {'g', 'g', 'g', 'g', 'g'}),
          {"slate"}, "green letters");
}

void test_filters_yellow_and_gray_letters() {
    vector<string> candidates = {"crate", "trace", "stare", "cigar"};

    check(filter_candidates(candidates, "arise", {'y', 'y', 'x', 'y', 'g'}),
          {"stare"}, "yellow and gray letters");
}

void test_uses_duplicate_letter_feedback() {
    vector<string> candidates = {"apple", "allee", "ample", "apply"};

    check(filter_candidates(candidates, "allee", {'g', 'y', 'x', 'x', 'g'}),
            {"apple", "ample"}, "duplicate letters");
}

void test_can_filter_any_word_length() {
    vector<string> candidates = {"loop", "tool", "pool", "look"};

    check(filter_candidates(candidates, "TOOL", {'x', 'g', 'g', 'y'}),
            {"loop", "look"}, "arbitrary word length");
}

void test_invalid_feedback_length() {
    bool threw = false;
    try {
        filter_candidates({"slate"}, "slate", {'g', 'g'});
    } catch (const invalid_argument&) {
        threw = true;
    }

    if (!threw) {
        throw runtime_error("invalid feedback length was accepted");
    }
}

int main() {
    try {
        test_filters_green_letters();
        test_filters_yellow_and_gray_letters();
        test_uses_duplicate_letter_feedback();
        test_can_filter_any_word_length();
        test_invalid_feedback_length();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    cout << "5 candidate filter tests passed\n";
}
