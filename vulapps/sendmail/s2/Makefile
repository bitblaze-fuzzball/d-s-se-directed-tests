DIET = diet
DIET_CFLAGS =
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	ge-bad ge-ok

clean:	
	rm -f ge-bad ge-ok ge-bad.out ge-ok.out *.o

ge-bad: main-bad.c  recipient-bad.c  util-bad.c mywrapper.c
	$(CC) -g -c main-bad.c  -I .
	$(CC) -g -c recipient-bad.c -I .  
	$(CC) -g -c util-bad.c  -I .
	$(CC) -g -c mywrapper.c  -I .
	ld --wrap getpwent -r -o ge-bad.o main-bad.o recipient-bad.o util-bad.o mywrapper.o
	$(CC) ge-bad.o -o ge-bad $(LDFLAGS)

ge-bad-diet: main-bad.c  recipient-bad.c  util-bad.c mywrapper.c
	$(DIET) $(CC) -g -c main-bad.c  -I .
	$(DIET) $(CC) -g -c recipient-bad.c -I .  
	$(DIET) $(CC) -g -c util-bad.c  -I .
	$(DIET) $(CC) -g -c mywrapper.c  -I .
	ld --wrap getpwent -r -o ge-bad.o main-bad.o recipient-bad.o util-bad.o mywrapper.o
	$(DIET) $(CC) $(DIET_CFLAGS) ge-bad.o -o ge-bad-diet $(DIET_LDFLAGS)

ge-ok: main-ok.c  recipient-ok.c  util-ok.c mywrapper.c
	$(CC) -g -c main-ok.c  -I .
	$(CC) -g -c recipient-ok.c -I .  
	$(CC) -g -c util-ok.c  -I .
	$(CC) -g -c mywrapper.c  -I .
	ld --wrap getpwent -r -o ge-ok.o main-ok.o recipient-ok.o util-ok.o mywrapper.o
	$(CC) ge-ok.o -o ge-ok $(LDFLAGS)


