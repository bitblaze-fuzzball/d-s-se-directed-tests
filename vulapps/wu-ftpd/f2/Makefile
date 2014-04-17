DIET = diet
DIET_CFLAGS = -g -fno-jump-tables
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	obo-bad obo-ok setup

clean:	
	rm -rf obo-bad obo-ok /tmp/foo obo-bad.out obo-ok.out

obo-bad: call_fb_realpath.c realpath-bad.c
	$(CC) -o obo-bad call_fb_realpath.c realpath-bad.c

obo-bad-diet: call_fb_realpath.c realpath-bad.c
	$(DIET) $(CC) $(DIET_CFLAGS) -o obo-bad-diet call_fb_realpath.c realpath-bad.c

obo-ok: call_fb_realpath.c realpath-ok.c
	$(CC) -o obo-ok call_fb_realpath.c realpath-ok.c

setup: 
	mkdir -p /tmp/foo/bar/foo/bar/foo/bar/foo/bar ; \
	touch /tmp/foo/bar/foo/bar/foo/bar/foo/bar/abcdefghi ; \
	ln -nsf /tmp/foo/bar/foo/bar/foo/bar/foo/bar/abcdefghi /tmp/foo/bar/foo/bar/foo/bar/foo/bar/abcdefgh


