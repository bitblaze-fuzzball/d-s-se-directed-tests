#ifndef READ_ASSOC
#define READ_ASSOC

#include <cassert>
#include <fstream>
#include <iostream> 
#include <getopt.h>
#include <string>
#include <vector>

using namespace std;

unsigned int split(const std::string &txt, std::vector<std::string> &strs, char ch);
vector<long> parseLine(string list);
vector<vector<long>> handlefile(ifstream &fstream);

#endif
