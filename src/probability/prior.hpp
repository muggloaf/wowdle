#ifndef WOWDLE_PRIOR_H
#define WOWDLE_PRIOR_H

#include <map>
#include <string>
#include <utility>
#include <vector>
using namespace std;

map<string, double> load_word_weights(const string& filename);

vector<pair<string, double>> rank_candidates(
    const vector<string>& candidates,
    const map<string, double>& weights,
    double fallback_weight = 1e-12);

#endif
