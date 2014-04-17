DIET = diet
DIET_CFLAGS = -DDIETLIBC -g -fno-jump-tables
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	iquery-bad iquery-ok # create

clean:	
	rm -f iquery-bad iquery-ok create iquery-bad.out iquery-ok.out

iquery-bad: iquery-bad.c
	$(CC) -o iquery-bad iquery-bad.c -lresolv

iquery-bad-diet: iquery-bad.c ../b1/my-resolv.c
	$(DIET) $(CC) $(DIET_CFLAGS) -o iquery-bad-diet iquery-bad.c ../b1/my-resolv.c

iquery-ok: iquery-ok.c
	$(CC) -o iquery-ok iquery-ok.c -lresolv

create: create_iquery.c
	$(CC) -o create create_iquery.c  -lresolv ; \
	./create


