DIET = diet
DIET_CFLAGS = -DDIETLIBC
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a


all:	nxt-bad nxt-ok

clean:	
	rm -f nxt-bad nxt-ok nxt-bad.out nxt-ok.out

nxt-bad: nxt-bad.c 
	$(CC) -o nxt-bad nxt-bad.c -I .  -lresolv

nxt-bad-nojt: nxt-bad.c 
	$(CC) -g -fno-jump-tables -o $@ nxt-bad.c -I .  -lresolv

nxt-bad-myresolv: nxt-bad.c my-resolv.c
	$(CC) -g -o $@ nxt-bad.c my-resolv.c -I .

nxt-bad-myresolv-mylibc: nxt-bad.c my-resolv.c ../../my-libc.o
	$(CC) -g -o $@ nxt-bad.c my-resolv.c ../../my-libc.o -I .

nxt-bad-myresolv-mylibc-diet: nxt-bad.c my-resolv.c ../../my-libc.c
	$(DIET) $(CC) $(DIET_CFLAGS) -g -o $@ nxt-bad.c my-resolv.c ../../my-libc.c -I .

nxt-bad-myresolv-mylibc-nojt: nxt-bad.c my-resolv.c ../../my-libc.o
	$(CC) -fno-jump-tables -g -o $@ nxt-bad.c my-resolv.c ../../my-libc.o -I .

nxt-ok: nxt-ok.c 
	$(CC) -o nxt-ok nxt-ok.c -I .  -lresolv
