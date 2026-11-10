#include "../src/strategies/distance.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace std;

void check(bool condition, const string& test_name) {
    if (!condition) {
        throw runtime_error("distance test failed: " + test_name);
    }
}

void test_word_hamming_distance() {
    check(word_hamming_distance("abcde", "abfde") == 1,
          "word hamming distance");
}

void test_feedback_distances() {
    check(feedback_hamming_distance("abc", "abc", "axc") == 1,
          "feedback hamming distance");
    check(abs(weighted_feedback_distance("abc", "abc", "axc") - 1.0 / 3.0) <
              1e-9,
          "weighted feedback distance");
}

void test_average_feedback_separation() {
    vector<string> candidates = {"aaa", "bbb"};

    check(abs(average_feedback_separation("aaa", candidates) - 1.0) < 1e-9,
          "average separation");
}

void test_bucket_separation() {
    vector<string> candidates = {"aaa", "bbb", "ccc"};

    check(abs(bucket_separation("aaa", candidates) - 2.0 / 3.0) < 1e-9,
          "bucket separation");
}

void test_mismatched_words_are_rejected() {
    bool threw = false;
    try {
        word_hamming_distance("abc", "abcd");
    } catch (const invalid_argument&) {
        threw = true;
    }
    check(threw, "mismatched words");
}

int main() {
    try {
        test_word_hamming_distance();
        test_feedback_distances();
        test_average_feedback_separation();
        test_bucket_separation();
        test_mismatched_words_are_rejected();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    cout << "5 distance tests passed\n";
}
