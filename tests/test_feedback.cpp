#include "../src/core/feedback.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

void check(const vector<char>& actual, const vector<char>& expected, const string& test_name) {
    if (actual != expected) {
        throw runtime_error("feedback mismatch in " + test_name);
    }
}

void test_all_green() {
    check(evaluate_feedback("slate", "slate"),
          {'g', 'g', 'g', 'g', 'g'},
          "all green");
}

void test_all_gray() {
    check(evaluate_feedback("arise", "cloud"),
          {'x', 'x', 'x', 'x', 'x'},
          "all gray");
}

void test_mixed_feedback() {
    check(evaluate_feedback("arise", "irate"),
          {'y', 'g', 'y', 'x', 'g'},
          "mixed feedback");
}

void test_single_duplicate_is_not_overreported() {
    check(evaluate_feedback("allee", "apple"),
          {'g', 'y', 'x', 'x', 'g'},
          "single duplicate");
}

void test_multiple_duplicates() {
    check(evaluate_feedback("eerie", "three"),
          {'y', 'x', 'g', 'x', 'g'},
          "multiple duplicates");
    check(evaluate_feedback("bobby", "abbey"),
          {'y', 'x', 'g', 'x', 'g'},
          "repeated b");
}

void test_arbitrary_length_and_case_insensitivity() {
    check(evaluate_feedback("TOOL", "LOOP"),
          {'x', 'g', 'g', 'y'},
          "arbitrary length");
}

void test_invalid_lengths() {
    for (const auto& inputs :
         vector<pair<string, string>>{{"", ""}, {"word", "words"}}) {
        bool threw = false;
        try {
            evaluate_feedback(inputs.first, inputs.second);
        } catch (const invalid_argument&) {
            threw = true;
        }
        if (!threw) {
            throw runtime_error("invalid input was accepted");
        }
    }
}

int main() {
    try {
        test_all_green();
        test_all_gray();
        test_mixed_feedback();
        test_single_duplicate_is_not_overreported();
        test_multiple_duplicates();
        test_arbitrary_length_and_case_insensitivity();
        test_invalid_lengths();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    cout << "7 feedback tests passed\n";
}