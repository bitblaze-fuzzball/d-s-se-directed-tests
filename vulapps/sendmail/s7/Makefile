DIET = diet
DIET_CFLAGS = -DDIETLIBC -g -fno-jump-tables
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	txtdns-bad txtdns-ok

clean:	
	rm -f txtdns-bad txtdns-ok txtdns-bad.out txtdns-ok.out

txtdns-bad: txt-dns-file-bad.c my-util.c  
	$(CC) -o txtdns-bad txt-dns-file-bad.c my-util.c  -I . -lresolv

txtdns-bad-diet: txt-dns-file-bad.c my-util.c  
	$(DIET) $(CC) $(DIET_CFLAGS) -o txtdns-bad-diet txt-dns-file-bad.c my-util.c  -I .

txtdns-ok: txt-dns-file-ok.c  my-util.c  
	$(CC) -o txtdns-ok txt-dns-file-ok.c my-util.c  -I . -lresolv
