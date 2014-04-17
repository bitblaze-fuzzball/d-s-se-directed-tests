DIET = diet
DIET_CFLAGS = -g -fno-jump-tables -DHAVE_SYMLINK
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	rp-bad rp-ok

clean:	
	rm -f rp-bad rp-ok rp-bad.out rp-ok.out

rp-bad: call-realpath-bad.c realpath-2.4.2-bad.c
	$(CC) -fno-stack-protector -o rp-bad mymain.c call-realpath-bad.c realpath-2.4.2-bad.c

rp-bad-diet: call-realpath-bad.c realpath-2.4.2-bad.c
	$(DIET) $(CC) $(DIET_CFLAGS) -fno-stack-protector -o rp-bad-diet mymain.c call-realpath-bad.c realpath-2.4.2-bad.c $(DIET_LDFLAGS)

rp-ok: call-realpath-ok.c realpath-2.4.2-ok.c
	$(CC) -fno-stack-protector -o rp-ok mymain.c call-realpath-ok.c realpath-2.4.2-ok.c



