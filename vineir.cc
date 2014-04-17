/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
//======================================================================
//
// This file contains a test of the VEX IR translation interface.
//
//======================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <set>

#include "asm_program.h"

extern "C" 
{
#include "libvex.h"
}

#include "irtoir.h"

using namespace std;

void print_vine_ir(asm_program_t *prog, vector<vine_block_t *> vblocks )
{
    unsigned int i, j;
    
    for ( i = 0; i < vblocks.size(); i++ )
    {
        vine_block_t *block = vblocks.at(i);
        assert(block);
        
        vector<Stmt *> *inner = block->vine_ir;

	ostringstream os;
	ostream_insn(prog, block->inst, os);
	cout << "   // " << os.str() << endl;


	vector<VarDecl *> globals = get_reg_decls();
	map<string,reg_t> context;
        for(vector<VarDecl *>::const_iterator gi = globals.begin();
	    gi != globals.end(); gi++){
           VarDecl *vd = *gi;
           context.insert(pair<string, reg_t>(vd->name, vd->typ));
        }

        for ( j = 0; j < inner->size(); j++ )
        {
	  Stmt *s = inner->at(j);
	  cout << "     " << s->tostring();
	  cout << endl;

        }
    }
    
}

int main(int argc, char **argv) {
    unsigned char buf[1024];
    int size;
    asm_program_t *prog;
    vine_block_t *block;
    vector<vine_block_t *> vblocks;

    extern bool translate_calls_and_returns;

	translate_calls_and_returns = true;

    size = read(0, buf, sizeof(buf));

    prog = byte_insn_to_asmp(bfd_arch_i386, 0, (unsigned char *) buf, size);
    block = asm_addr_to_ir(prog, 0);

    vblocks.push_back(block);

    print_vine_ir(prog, vblocks);
}
