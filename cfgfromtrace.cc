#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "graph.h"
#include "debug.h"

int DEBUG_LEVEL = 2;
FILE *DEBUG_FILE = stderr;

int main(int argc, char **argv) {
  unsigned int funcaddr;
  FILE *trace;
  char *line = NULL;
  int tid, len, pos, linelen = 0;
  unsigned int addr, func, prev;
  Cfg *cfg;

  assert(argc == 3);

  trace = fopen(argv[1], "r");
  assert(trace);
  funcaddr = strtoul(argv[2], NULL, 16);
  assert(funcaddr);

  cfg = new Cfg();

  while (getline(&line, (size_t *) &linelen, trace) != -1) {
    sscanf(line, "%d %d %x %d %x %x", &tid, &pos, &addr, &len, &func, &prev);
    if (funcaddr == func) {
     cfg->addInstruction(addr, (unsigned char *) "\x00\x00\x00\x00\x00\x00\x00\x00", len, pos, prev);
    }

  }

  //printf("%s\n", cfg->dot().c_str());
}
