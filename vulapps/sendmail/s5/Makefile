DIET = diet
DIET_CFLAGS =
DIET_LDFLAGS = /opt/diet/lib-i386/libcompat.a

all:	prescan-bad prescan-ok

clean:	
	rm -f prescan-bad prescan-ok prescan-bad.out prescan-ok.out

prescan-bad: prescan-overflow-bad.c
	$(CC) -g -o prescan-bad prescan-overflow-bad.c

prescan-bad-diet: prescan-overflow-bad.c
	$(DIET) $(CC) -g -o prescan-bad-diet prescan-overflow-bad.c

prescan-ok: prescan-overflow-ok.c
	$(CC) -g -o prescan-ok prescan-overflow-ok.c


