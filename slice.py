import sys

defs = {}
uses = {}
definitions = {}

regs = {
    "R1:4[0x14,0x18]" : "EDI",
    "R1:4[0x8,0xc]" : "ECX",
    "R1:4[0,0x4]" : "EAX"
}

def reg2str(r):
    if r in regs:
        return regs[r]
    else:
        return r

def parse(infile, addr = 0):
    defs.clear()
    uses.clear()
    definitions.clear()

    _uses = _defs = _definitions = _go = False
    

    infile = open(infile)
    for l in infile.xreadlines():

        if l.startswith("@@@@@@@") and "%.x" % addr in l:
            _go = True
            continue

        if not _go:
            continue

        if _go and l.startswith("@@@@@"):
            break

        l = l.strip()
        if "===== USES" in l:
            _uses = True
            continue
        elif "==== DEFS" in l:
            _uses = False
            _defs = True
            continue
        elif "==== DEFINI" in l:
            _definitions = True
            _defs = False
            continue

        if _uses:
            if not l:
                _uses = False
                continue
            assert "\t" in l, l
            instr = int(l.split("\t")[0], 16)
            if not instr in uses:
                uses[instr] = set()
            for u in l.split("\t")[1].split():
                uses[instr].add(reg2str(u))
            assert len(uses[instr]) > 0, l

        if _defs:
            if not l:
                _defs = False
                continue
            assert "\t" in l, l
            instr = int(l.split("\t")[0], 16)
            if not instr in defs:
                defs[instr] = set()
            for d in l.split("\t")[1].split():
                defs[instr].add(reg2str(d))
            assert len(defs[instr]) > 0, l

        if _definitions:
            if not l or l.startswith("Serializing"):
                _definitions = False
                continue
            assert "\t" in l, l
            d = reg2str(l.split("\t")[0])
            if not d in definitions:
                definitions[d] = set()
            for i in l.split("\t")[1].split():
                definitions[d].add(int(i, 16))
            assert len(definitions[d]) > 0, l


if __name__ == "__main__":
    parse("/dev/stdin", int(sys.argv[1], 16))
    
