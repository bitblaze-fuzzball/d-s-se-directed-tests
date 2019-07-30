#include "read_assoc.h"

using namespace std;

const static int FAILURE = -1;
const static int NUMBER_BASE = 16;
const static char DELIMITER = ' ';

static struct option long_options[] =
  {
    {"verbose", no_argument, 0, 'v'},
    {"addr_file", required_argument, 0, 'a'},
    {0,0,0,0}
  };

unsigned int split(const std::string &txt, std::vector<std::string> &strs, char ch){
  // stolen from stack overflow because I'm lazy.
  // pushes strings delimited by the token ch into a vector
  unsigned int pos = txt.find( ch );
  unsigned int initialPos = 0;
  strs.clear();

  // Decompose statement
  while( pos != std::string::npos ) {
    strs.push_back( txt.substr( initialPos, pos - initialPos + 1 ) );
    initialPos = pos + 1;
    pos = txt.find( ch, initialPos );
  }

  strs.push_back( txt.substr( initialPos, std::min( pos, txt.size() ) - initialPos + 1 ) );
  return strs.size();
}

vector<long> parseLine(string list){
  // read in a simple space delimited line of hex digits
  // set up for the split
  vector<string> nums;
  int num_nums = split(list, nums, ' ');
  vector<long> ret;
  // parse the numbers one at a time
  for(int index = 0; index < num_nums; index++){
    ret.push_back(strtol(nums[index].c_str(), NULL, 16));
  }
  return ret;
}

vector<vector<long>> handlefile(ifstream &fstream){
  // take a single line from ifstream and return the vector of numbers.
  string current;
  vector<vector<long>> lines;
  while((getline(fstream, current))){
    lines.push_back(parseLine(current));
  }
  return lines;
}

static void print_usage(){
  cout << "--verbose: Set output to verbose (quiet by default)" << endl
	    << "--addr_file <FileName>: the file to read in." << endl;
}

/* int main(int argc, char** argv){

  int verbose_flag = 0;
  int option_index = 0;
  char* fname;
  int c = 0;

  while( (c = getopt_long(argc,argv, "a:v", long_options, &option_index)) != -1){
    switch(c){
    case 'a':
      fname = optarg;
      if(verbose_flag){
	cout << "File to read: " << fname << endl;
      }
      break;

    case 'v': // flag already set?
      verbose_flag = 1;
      break;

    default:
      print_usage();
      exit(FAILURE);
    }
  }

  // args are parsed, now do things
  ifstream myFile(fname, ios::in);
  if(myFile.is_open()){
  }else{
    cerr << "File " << fname
	 << " didn't open appropriately. Exiting."
	 << endl;
    exit(FAILURE);
  }

  vector<vector<long>> addresses = handlefile(myFile);
  myFile.close();

  } */
