#ifndef READ_ASSOC
#define READ_ASSOC

#include <cassert>
#include <fstream>
#include <iostream> 
#include <getopt.h>
#include <string>
#include <vector>

using namespace std;

static unsigned int split(const std::string &txt, std::vector<std::string> &strs, char ch);
static vector<long> parseLine(string list);
static vector<vector<long>> handlefile(ifstream &fstream);

#endif
