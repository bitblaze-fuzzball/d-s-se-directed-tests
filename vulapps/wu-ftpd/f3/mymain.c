#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

int myfoo(int argc, char **argv);

int main() {
  char* argv[3];
  char* name = "myfoo";
  size_t dummy;

  FILE * fp = fopen ("testcase", "r");
  if (!fp) {
	fprintf (stderr, "Could not find a testfile\n");
	exit(1);  
  }
  argv[0] = name;
  argv[1] = NULL;
  argv[2] = NULL;

  getline(&(argv[1]), &dummy, fp);
  getline(&(argv[2]), &dummy, fp);

  return (myfoo(3, argv));
}
