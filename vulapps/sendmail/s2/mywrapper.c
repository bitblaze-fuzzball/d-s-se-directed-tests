#include <sys/types.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

void
taint_data(char *fname, struct passwd* pwd)
{
    FILE *fp = fopen (fname, "r");
    int dummy =0;

    printf ("TAINTED HERE !!\n");

    if (!pwd)
	return;

    if (!fp) {
	fprintf (stderr, "Can't find testcase file (%s) with %d lines of tainted input. You should manually 'cp testcase.exploit testcase' or 'cp testcase.init testcase'\n", fname, 2);
	exit(1);
    }
    
    pwd->pw_name = NULL;
    pwd->pw_gecos = NULL;
    getline (&(pwd->pw_name), &dummy, fp);
    getline (&(pwd->pw_gecos), &dummy, fp); 
}


struct passwd *
__wrap_getpwent() 
{
  struct passwd* ret = __real_getpwent();
  taint_data ("testcase", ret);
  return ret;
  
}


