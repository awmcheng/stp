
/*
 * This code has been adapted from a course at Boston University
 * for use in CPSC 317 at UBC
 *
 * Version 1.0
 * 
 */

#include <string.h>
#include "stp.h"


/*Adds modulo maxVal so can have seqno wraparound*/
unsigned short plus(unsigned short val1,  unsigned short val2)
{
  return (val1+val2);            

}

/* Subtracts the lesserVal from greaterVal.
   This is useful for sequence wraparound cases, when a packet
   with a seqno 15 can actually be greater than the packet with
   a seqno 65530 because of the wraparound.

   This particular function is used to subtract lastByteRead from
   lastByteReceived to figure out the current advertised window; we can
   do this because we know that lastByteReceived >= lastByteRead at all times.

 */
unsigned short minus(unsigned short greaterVal, 
		     unsigned short lesserVal)
{
     return (greaterVal - lesserVal);
}




/* Is val1 > val2 ? */
int greater(unsigned short val1, 
	    unsigned short val2)
{
  unsigned short int maxVal;

  memset(&maxVal, 0xff, sizeof(unsigned short int));
  maxVal = (maxVal / 2) + 1;

  /* How does this code work ? 
     
     The sequence numbers cannot be more than MAX_UNSIGNED_SHORT_INT/2 apart.
     If they are, that means that one of the numbers has wrapped around.

     To figure out which one, we recast these numbers into signed ints
     and see if the difference is greater than MAX_UNSIGNED_SHORT_INT/2 apart.

     So suppose we have val1 = 100 and val2 = 65500 and MAX_UNSIGNED_SHORT_INT/2 = (usually) 32768.
     Then, we do the first comparison:
     100 - 65500 > 32678 ? No, -65400 < 32768
     Second comparison:
     65500 - 100 > 32768 ? Yes, 65400 > 32678
     So there has been some wraparound, and 100 is the greater value 
  */

  if (((int)val1 - (int)val2) > (int)maxVal)
    {
      /* There has been some wraparound, val2 is the greater value */
      return 0;
    }
  else if (((int)val2 - (int)val1) > (int)maxVal)
    {
      /* There's been some wraparound, val1 is the greater value */
      return 1;
    }
  else
    {
      return (val1 > val2);
    }

}


