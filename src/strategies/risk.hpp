#ifndef WOWDLE_RISK_H
#define WOWDLE_RISK_H

#include <string>
#include <vector>
using namespace std;

double basic_guess_risk(const string& guess,
                        const vector<string>& candidates);

double urgency_adjusted_risk(const string& guess,
                             const vector<string>& candidates,
                             int guesses_remaining);

#endif
