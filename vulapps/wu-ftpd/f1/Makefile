DIET = diet
DIET_CFLAGS =
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	mp-bad mp-ok create

clean:	
	rm -f mp-bad mp-ok create mp-bad.out mp-ok.out

mp-bad: mapped-path-bad.c
	$(CC) -g -fno-stack-protector -o mp-bad mapped-path-bad.c

mp-bad-mylibc: mapped-path-bad.c ../../my-libc.o
	$(CC) -g -fno-stack-protector -o $@ mapped-path-bad.c ../../my-libc.o

mp-bad-mylibc-diet: mapped-path-bad.c ../../my-libc.c
	$(DIET) $(CC) -g -fno-stack-protector -o $@ mapped-path-bad.c ../../my-libc.c

mp-ok: mapped-path-ok.c
	$(CC) -fno-stack-protector -o mp-ok mapped-path-ok.c

create: make-long-path.c
	$(CC) -o create make-long-path.c ; \
	touch pathfile ; \
	./create
