#include <stdio.h>
#include <sys/types.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <resolv.h>

int main(){

  FILE *f;
  u_char buf[1000];
  u_char *p;
  char *temp, *temp1; 
  u_char *comp_dn, *comp_dn2;
  char exp_dn[200], exp_dn2[200];
  u_char **dnptrs, **lastdnptr, **dnptrs2;
  int i,len = 0, comp_size;
  u_long now;


  dnptrs = (unsigned char **) malloc(2 * sizeof(unsigned char *));
  dnptrs2 = (unsigned char **) malloc(2 * sizeof(unsigned char *));

  comp_dn = (unsigned char *) malloc(200*sizeof(unsigned char));
  comp_dn2 = (unsigned char *) malloc(200*sizeof(unsigned char));

  temp1 = (char *) malloc(400*sizeof(char));
    
  temp = temp1;
  
  p = buf;

  strcpy(temp, "HEADER JUNK:");
  
  len += strlen(temp);

  while (*temp != '\0') 
    *p++ = *temp++;
  
  strcpy(exp_dn, "lcs.mit.edu");         
  
  *dnptrs++ = (u_char *) exp_dn;
  *dnptrs-- = NULL;

  lastdnptr = NULL;

  printf("Calling dn_comp..\n");
  comp_size = dn_comp((const char *) exp_dn, comp_dn, 200, dnptrs, lastdnptr);
  printf("uncomp_size = %d\n", strlen(exp_dn));
  printf("comp_size = %d\n", comp_size);
  printf("exp_dn = %s, comp_dn = %s\n", exp_dn, (char *) comp_dn);
  
  for(i=0; i<comp_size; i++) 
    *p++ = *comp_dn++;

  len += comp_size;

  PUTSHORT(24, p); /* type = T_SIG = 24 */
  p += 2;    
  
  PUTSHORT(C_IN, p);   /* class = C_IN = 1*/
  p += 2;

  PUTLONG(255, p);  /* ttl */
  p += 4;

  PUTSHORT(30, p);  /* dlen = len of everything starting with the covered byte (the length 
			of the entire resource record... we lie about it
		   */
  p += 2;

  len += 10;

  PUTSHORT(15, p);  /* covered type */
  p += 2;
    
  PUTSHORT(256*2, p);  /* algorithm and labels.. MAKE ALG = 2,i.e default ALG*/
  p += 2;
  
  PUTLONG(255, p);  /* orig ttl */
  p += 4;

  now = time(NULL);  
  
  printf("Signing at = %d\n", now);
  PUTLONG(now+(30*24*2600), p);   /* expiration time, ~1mo in future */
  p += 4;
  PUTLONG(now, p);         /* time signed */  
  p += 4;

  PUTSHORT(100, p);            /* random key footprint */
  p += 2;
  
  len += 18;

  strcpy(exp_dn2, "sls.lcs.mit.edu"); /* signer */

  *dnptrs2++ = (u_char *) exp_dn2;
  *dnptrs2-- = NULL;
  lastdnptr = NULL;
  
  printf("Calling dn_comp..\n");
  comp_size = dn_comp((const char *) exp_dn2, comp_dn2, 200, dnptrs2, lastdnptr);
  printf("uncomp_size = %d\n", strlen(exp_dn2));
  printf("comp_size = %d\n", comp_size);
  printf("exp_dn2 = %s, comp_dn2 = %s\n", exp_dn2, (char *) comp_dn2);

  len += comp_size;
   
  for(i=0; i<comp_size; i++) 
    *p++ = *comp_dn2++;

  for(i=0; i<11; i++)
  {  
    PUTLONG(123, p);           /* fake signature */
    p += 4;
    len += 4;
  }
  
  f = fopen("testcase", "w");

  p = buf;

  printf("len = %d\n", len);
  for(i=0; i<len; i++, p++)  /* write record into file */
    fputc(*p, f);
  
  fclose(f);
  
}
