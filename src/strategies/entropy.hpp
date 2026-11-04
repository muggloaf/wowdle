#ifndef WOWDLE_ENTROPY_H
#define WOWDLE_ENTROPY_H

#include <string>
#include <map>
#include <vector>
using namespace std;

double calculate_entropy(const string& guess,
                         const vector<string>& candidates);

string best_entropy_guess(const vector<string>& guesses,
                          const vector<string>& candidates);

vector<pair<string, double>> rank_combined_guesses(
    const vector<string>& guesses,
    const vector<string>& candidates,
    const map<string, double>& weights,
    double information_weight = 0.7,
    double likelihood_weight = 0.3);

#endif
