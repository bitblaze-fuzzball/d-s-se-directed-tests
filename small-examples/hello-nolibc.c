/* This is a minimal hello world program that doesn't use any C
   library at all, instead implementing its own wrappers to call
   Linux/x86-32 system calls. This avoids some features even in
   dietlibc that lead to false positives, so this is an example of a
   program that the static analysis can correctly conclude has no
   memory safety vulnerabilities. */

/* Compile with:
   gcc-4.8 -Wall -Og -g -m32 -nostdlib hello-nolibc.c -o hello-nolibc
*/

#define __NR_exit 1
#define __NR_write 4

typedef unsigned long size_t;
typedef long ssize_t;

void exit(int status) {
    int syscall_num = __NR_exit;
    asm volatile ("int $0x80" : : "a" (syscall_num), "b" (status));
    __builtin_unreachable(); /* exit(2) can never fail to exit */
}

ssize_t write(int fd, const void *buf, size_t count) {
    int syscall_num = __NR_write;
    int retval;
    asm volatile ("int $0x80" : "=a" (retval) : "a" (syscall_num),
		 "b" (fd), "c" (buf), "d" (count));
    return retval;
}

int main(void) {
    char msg[] = "Hello, world!\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
}

void _start(void) {
    int retval = main();
    exit(retval);
}
