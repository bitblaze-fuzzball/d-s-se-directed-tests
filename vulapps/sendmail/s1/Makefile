DIET = diet
DIET_CFLAGS =
DIET_LDFLAGS = 

all:	ca-bad ca-ok 

clean:	
	rm -f ca-bad ca-ok ca-bad.out ca-ok.out

ca-bad: crackaddr-bad.c
	$(CC) -g -o ca-bad crackaddr-bad.c

ca-bad-mylibc: crackaddr-bad-mylibc.c ../../my-libc.o
	$(CC) -g -o $@ crackaddr-bad-mylibc.c ../../my-libc.o

ca-bad-mylibc-diet: crackaddr-bad-mylibc.c ../../my-libc.c
	$(DIET) $(CC) $(DIET_CFLAGS) -g -o $@ crackaddr-bad-mylibc.c ../../my-libc.c $(DIET_LDFLAGS)

ca-bad-mylibc-strchr-diet: crackaddr-bad-mylibc-strchr.c ../../my-libc.c
	$(DIET) $(CC) $(DIET_CFLAGS) -g -o $@ crackaddr-bad-mylibc-strchr.c ../../my-libc.c $(DIET_LDFLAGS)

ca-ok: crackaddr-ok.c
	$(CC) -g -o ca-ok crackaddr-ok.c

