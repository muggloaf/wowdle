#ifndef WOWDLE_DISTANCE_H
#define WOWDLE_DISTANCE_H

#include <string>
#include <vector>
using namespace std;

int word_hamming_distance(const string& first, const string& second);

int feedback_hamming_distance(const string& guess,
                              const string& first_answer,
                              const string& second_answer);

double weighted_feedback_distance(const string& guess,
                                  const string& first_answer,
                                  const string& second_answer);

double average_feedback_separation(const string& guess,
                                   const vector<string>& candidates);

double bucket_separation(const string& guess,
                         const vector<string>& candidates);

#endif
