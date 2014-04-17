#!/usr/bin/env python

import sys, os, subprocess

ADDR2LINE = "addr2line"

NULL_DEREF_READ = 0
NULL_DEREF_WRITE = 1
MISALIGN_READ = 2
MISALIGN_WRITE = 3
WRITE_OOB = 4
READ_OOB = 5
READ_UNINIT = 6
UNBOUND_MALLOC = 7

msg2code = [("Possible NULL ptr dereference (read)", NULL_DEREF_READ),
            ("Possible NULL ptr dereference (write)", NULL_DEREF_WRITE),
            ("Misaligned read discovered", MISALIGN_READ),
            ("Misaligned write discovered", MISALIGN_WRITE),
            ("Write out of bounds", WRITE_OOB),
            ("Read out of bounds", READ_OOB),
            ("Read of uninitialized address", READ_UNINIT),
            ("Unbounded malloc", UNBOUND_MALLOC)]

code2warn = {
    NULL_DEREF_READ : "Null pointer dereference (read)",
    NULL_DEREF_WRITE : "Null pointer dereference (write)",
    MISALIGN_READ : "Misaligned read",
    MISALIGN_WRITE : "Misaligned write",
    WRITE_OOB : "Write out-of-bounds",
    READ_OOB : "Read out-of-bounds",
    READ_UNINIT : "Read of uninitialized address",
    UNBOUND_MALLOC : "Allocate the entire address space"}            

__cache = {}
def addr2line(exe, addr):
    if (exe, addr) in __cache:
        return __cache[(exe, addr)]
    cmdline = "%s -f -e %s 0x%.8x" % (ADDR2LINE, exe, addr)
    pipe = subprocess.Popen(cmdline.split(), stdout = subprocess.PIPE)
    output = pipe.communicate()[0]
    assert ":" in output
    output = output.split("\n")
    func = output[0]
    source, line = output[1].split(":")
    __cache[(exe, addr)] = func, source, int(line)
    return func, source, int(line)

# IGNORE = [MISALIGN_WRITE, MISALIGN_READ, NULL_DEREF_WRITE, NULL_DEREF_READ, READ_UNINIT]
IGNORE = []

def parsewarn(msg):
    if msg.startswith("***"):
        for m, c in msg2code:
            if m in msg:
                if not c in IGNORE:
                    a1 = int(msg.split(" ### ")[1].strip(), 16)
                    a2 = int(msg.split(" ### ")[2].strip(), 16)
                    return c, a1, a2
                else:
                    return None
        assert False, "Invalid warning '%s'" % msg
    else:
        return None

def printwarn(warn, addr, func, source, line, addr2, func2, source2, line2):
    s = ""
    if warn is not None: s = code2warn[warn]
    source = source.replace("/home/martignlo/DATA/Ricerca/TypeInference/", "")
    source2 = source2.replace("/home/martignlo/DATA/Ricerca/TypeInference/", "")
    source = "%s@%s:%d" % (func, source, line)
    source2 = "%s@%s:%d" % (func2, source2, line2)
    print "%-40s: %.8x : %-40s : %.8x : %s" % (s, addr, source, addr2, source2)

if __name__ == "__main__":
    assert len(sys.argv) == 3 
    exe = sys.argv[1]
    log = sys.argv[2]
    assert os.path.isfile(exe)
    assert os.path.isfile(log) or log == "/dev/stdin"
    log = open(log)

    first = True
    for warn in log.xreadlines():
        warn = warn.strip()
        warn_ = parsewarn(warn)
        if warn_:
            wanr_type, warn_addr1, warn_addr2 = warn_
            func1, source1, line1 = addr2line(exe, warn_addr1)
            func2, source2, line2 = addr2line(exe, warn_addr2)
            printwarn(wanr_type, warn_addr2, func2, source2, line2, warn_addr1, func1, source1, line1)
