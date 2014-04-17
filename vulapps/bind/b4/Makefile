DIET = diet
DIET_CFLAGS = -g -fno-jump-tables
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	nsl-bad nsl-ok #create

clean:	
	rm -f nsl-bad nsl-ok create nsl-bad.out nsl-ok.out

nsl-bad: ns-lookup-bad.c
	$(CC) -g -fno-stack-protector -o nsl-bad ns-lookup-bad.c

nsl-bad-diet: ns-lookup-bad.c
	$(DIET) $(CC) $(DIET_CFLAGS) -fno-stack-protector -o nsl-bad-diet ns-lookup-bad.c

nsl-ok: ns-lookup-ok.c
	$(CC) -o nsl-ok ns-lookup-ok.c

create:	create_address_file
	$(CC) -fno-stack-protector -o create create_address_file.c ; \
	./create
