DIET = diet
DIET_CFLAGS = -DDIETLIBC -g -fno-jump-tables
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	sig-bad sig-ok #create

clean:	
	rm -f sig-bad sig-ok create sig-bad.out sig-ok.out SIGFILE

sig-bad: sig-bad.c
	$(CC) -g -o sig-bad sig-bad.c -I . -lresolv

sig-bad-diet: sig-bad.c ../b1/my-resolv.c
	$(DIET) $(CC) $(DIET_CFLAGS) -o sig-bad-diet sig-bad.c ../b1/my-resolv.c -I .

sig-bad-nojt: sig-bad.c
	$(CC) -fno-jump-tables -g -o $@ sig-bad.c -I . -lresolv

sig-ok: sig-ok.c
	$(CC) -o sig-ok sig-ok.c -I . -lresolv

create:	create_sig.c
	$(CC) -o create create_sig.c -lresolv ; \
	./create
