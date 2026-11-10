#include <iostream>
#include <stdexcept>
#include <string>
using namespace std;

extern "C" {
void wowdle_init(const char*, const char*);
const char* wowdle_recommend();
const char* wowdle_submit(const char*, const char*);
const char* wowdle_restore(const char*);
}

void check(bool condition, const string& message) {
    if (!condition) throw runtime_error(message);
}

bool contains(const string& text, const string& expected) {
    return text.find(expected) != string::npos;
}

void reset() { wowdle_init("cater caper baker", "cater 3 caper 2 baker 1"); }

int main() {
    try {
        reset();
        string initial = wowdle_recommend();
        check(contains(initial, "\"candidates\":3"), "initial candidates");
        check(contains(initial, "\"guesses_remaining\":6"), "initial turns");
        check(contains(initial, "\"analysis\":{") && contains(initial, "\"weights\":{"), "real analysis");
        check(contains(initial, "\"answers\":[") && contains(initial, "\"probability\":0.5"), "answer probabilities");
        check(contains(wowdle_submit("cater", "zzzzz"), "\"error\":"), "invalid marks rejected");
        check(string(wowdle_recommend()) == initial, "invalid marks preserve full state");
        check(contains(wowdle_submit("cater", "xxxxx"), "\"error\":"), "impossible feedback rejected");
        check(string(wowdle_recommend()) == initial, "impossible feedback preserves full state");
        check(contains(wowdle_submit("ca!er", "xxxxx"), "\"error\":"), "nonletters rejected before feedback engine");
        string filtered = wowdle_submit("CATER", "ggxgg");
        check(contains(filtered, "\"candidates\":1") && contains(filtered, "\"guess\":\"caper\""), "uppercase guesses and filtering");
        check(contains(filtered, "\"guesses_remaining\":5"), "turn counted once");
        check(contains(wowdle_submit("caper", "ggggg"), "\"status\":\"won\""), "verified win");
        check(contains(wowdle_submit("caper", "ggggg"), "\"error\":"), "submissions after win rejected");
        check(string(wowdle_restore("")) == initial, "reset restores original distribution");
        string restored = wowdle_restore("cater ggxgg");
        check(restored == filtered, "history replay matches live feedback");
        check(contains(wowdle_restore("cater ggxgg\ncaper xxxxx"), "\"error\":"), "bad replay rejected");
        check(string(wowdle_recommend()) == restored, "bad replay is transactional");
        check(contains(wowdle_restore("cater"), "\"error\":"), "incomplete replay rejected");
        string six;
        for (int i = 0; i < 6; ++i) six += "cater ggxgg\n";
        check(contains(wowdle_restore(six.c_str()), "\"status\":\"lost\""), "six-turn failure");
        check(contains(wowdle_restore((six + "caper ggggg").c_str()), "\"error\":"), "seventh turn rejected");
        wowdle_init("cat", "");
        check(contains(wowdle_recommend(), "\"error\":"), "bad dictionary rejected");
        wowdle_init("cater", "cater -1");
        check(contains(wowdle_recommend(), "\"error\":"), "negative weights rejected");
        wowdle_init("CATER cater", "");
        check(contains(wowdle_recommend(), "\"candidates\":1"), "dictionary deduplicated and normalized");
        wowdle_init("ain't cater nano-", "ain't 1 cater 2");
        check(contains(wowdle_recommend(), "\"skipped_words\":2"), "punctuated source entries skipped safely");
        cout << "bridge validation, replay, probability, and terminal-state checks passed\n";
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
}
