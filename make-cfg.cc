#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <elf.h>
#include <libelf.h>

extern "C" {
#include <xed-interface.h>
}

#include "argv_readparam.h"
#include "types.h"

int disassemble_from(addr_t addr, byte_t *code, addr_t &next1, addr_t &next2, 
		     xed_category_enum_t &category, char *buf = NULL, 
		     size_t bufsize = 0);

#include "debug.h"

int DEBUG_LEVEL = 2;
FILE *DEBUG_FILE = stderr;

// Cmd line args
static const char *dot = NULL;
static const char *vcg = NULL;

unsigned char *code;
addr_t code_base;
size_t code_size;

int main(int argc, char **argv) {
    char *tmpstr;

    Elf *elf;
    Elf32_Ehdr *ehdr;
    Elf32_Phdr *phdr;
    Elf32_Shdr *shdr;
    Elf_Scn *scn, *code_scn;
    addr_t start;
    unsigned version;

    int fd, res;
    int ph_count;
    int match_count = 0;

    if((tmpstr = argv_getString(argc, argv, "--dot=", NULL)) != NULL ) {
	dot = tmpstr;
    } else {
	dot = NULL;
    }

    if((tmpstr = argv_getString(argc, argv, "--vcg=", NULL)) != NULL ) {
	vcg = tmpstr;
    } else {
	vcg = NULL;
    }

    if (argc < 2 || argv[argc - 1][0] == '-') {
	fprintf(stderr, "Usage: make-cfg [options] program\n");
	exit(1);
    }

    fd = open(argv[argc - 1], O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "Failed to open %s for reading: %s\n",
		argv[argc - 1], strerror(errno));
	exit(1);
    }
    
    version = elf_version(EV_CURRENT);
    assert(version != EV_NONE);
    elf = elf_begin(fd, ELF_C_READ, 0);
    ehdr = elf32_getehdr(elf);
    ph_count = ehdr->e_phnum;
    start = ehdr->e_entry;
    phdr = elf32_getphdr(elf);

    /* It might be safer to iterate over segments (the program header)
       instead of sections, but it doesn't look like libelf would provide
       much help with that. */
    scn = 0;
    while ((scn = elf_nextscn(elf, scn)) != 0) {
        char *name;
        shdr = elf32_getshdr(scn);
        name = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
	if (shdr->sh_type == SHT_PROGBITS && start >= shdr->sh_addr
	    && start < shdr->sh_addr + shdr->sh_size) {
	    match_count++;
	    printf("Found candidate section at 0x%08x (%s)\n", shdr->sh_addr,
		   name);
	    code_scn = scn;
	}
    }
    assert(match_count == 1);

    shdr = elf32_getshdr(code_scn);
    code_size = shdr->sh_size;
    code_base = shdr->sh_addr;
    code = (unsigned char *)malloc(code_size);
    if (!code) {
	fprintf(stderr, "Failed to allocate %lu bytes for code\n",
		(unsigned long)code_size);
	exit(1);
    }
    lseek(fd, shdr->sh_offset, SEEK_SET);
    res = read(fd, code, code_size);
    assert(res == (int)code_size);
    printf("_start:\n%08lx: ", (long)start);
    for (int i = 0; i < 16; i++) {
	byte_t b = code[start + i - code_base];
	printf("%02x ", b);
    }
    printf("\n");
    addr_t addr = start;
    for (int i = 0; i < 20; i++) {
	char buf[80];
	addr_t next1 = 0, next2 = 0;
	xed_category_enum_t category;

	disassemble_from(addr, &code[addr - code_base], next1, next2,
		    category, buf, sizeof(buf));
	printf("%08x: %s (0x%08x, 0x%08x)\n", addr, buf, next1, next2);
	addr = next1;
    }


    (void)phdr;
    (void)ph_count;

    elf_end(elf);
}
