DIET = diet
DIET_CFLAGS = -g -fno-jump-tables -fno-omit-frame-prointer -O2
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	ttflag-bad ttflag-ok ttflag-bad-mylibc

clean:	
	rm -f ttflag-bad ttflag-ok ttflag-bad.out ttflag-ok.out

ttflag-bad:  my-main.c tTflag-bad.c
	$(CC) -g -o ttflag-bad my-main.c mymain.c tTflag-bad.c -I . $(LDFLAGS)

ttflag-ok:  my-main.c tTflag-ok.c
	$(CC) -g -o ttflag-ok my-main.c mymain.c tTflag-ok.c -I . $(LDFLAGS)

ttflag-bad-mylibc:  my-main.c tTflag-bad-mylibc.c ../../my-libc.o
	$(CC) -g -o ttflag-bad-mylibc my-main.c mymain.c tTflag-bad-mylibc.c -I . ../../my-libc.o $(LDFLAGS)

ttflag-bad-mylibc-diet:  my-main.c tTflag-bad-mylibc.c ../../my-libc.c
	$(DIET)	$(CC) -g -o ttflag-bad-mylibc-diet my-main.c mymain.c tTflag-bad-mylibc.c -I . ../../my-libc.c $(DIET_LDFLAGS)



