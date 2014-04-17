DIET = diet
DIET_CFLAGS =
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	m1-bad m1-ok

clean:	
	rm -f m1-bad m1-ok m1-bad.out m1-ok.out

m1-bad: mime1-bad.c main.c 
	$(CC) -g -o m1-bad mime1-bad.c main.c -I .

m1-bad-diet: mime1-bad.c main.c 
	$(DIET)	$(CC) -g -o m1-bad-diet mime1-bad.c main.c -I .

m1-ok: mime1-ok.c main.c
	$(CC) -g -o m1-ok mime1-ok.c main.c -I .

