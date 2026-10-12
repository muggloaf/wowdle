#ifndef WOWDLE_CANDIDATE_FILTER_H
#define WOWDLE_CANDIDATE_FILTER_H

#include <string>
#include <vector>
using namespace std;

vector<string> filter_candidates(const vector<string>& candidates,
                                 const string& guess,
                                 const vector<char>& feedback);

#endif
