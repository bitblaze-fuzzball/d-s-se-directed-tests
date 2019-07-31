/* Compile with:
   gcc-4.8 -Wall -Og -g -m32 -nostdlib rot13char-overflow-nolibc.c mini-start.S -o rot13char-overflow-nolibc
*/

#define __NR_exit 1
#define __NR_write 4

typedef unsigned long size_t;
typedef long ssize_t;

ssize_t write(int fd, const void *buf, size_t count) {
    int syscall_num = __NR_write;
    int retval;
    asm volatile ("int $0x80" : "=a" (retval) : "a" (syscall_num),
		 "b" (fd), "c" (buf), "d" (count));
    return retval;
}

int main(int argc, char **argv) {
    char rot_table_ucase[] = "NOPQRSTUVWXYZABCDEFGHIJKLM";
    char rot_table_lcase[] = "nopqrstuvwxyzabcdefghijklm";
    char out_char = '?';
    char out_msg[] = "x\n";
    unsigned char in_char = '!';
    if (argc > 1 && argv[1][0])
	in_char = argv[1][0];
    if (in_char >= ' ' && in_char < 'A') {
	out_char = in_char;
    } else if (in_char >= 'A' && in_char < 'a') {
	out_char = rot_table_ucase[in_char - 'A'];
    } else {
	out_char = rot_table_lcase[in_char - 'a'];
    }
    out_msg[0] = out_char;
    write(1, out_msg, sizeof(out_msg) - 1);
    return 0;
}

