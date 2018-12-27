/**************************************************************************
*                                                                         *
*                                                                         *
*  Copyright (c) 2005 				                          *
*  by Ali R. Butt, Chris Gniady, Y. Charlie Hu 	               	          *
*  Purdue University, West Lafayette, IN 47906                            *
*                                                                         * 
*                                                                         *
*  This software is furnished AS IS, without warranty of any kind,        *
*  either express or implied (including, but not limited to, any          *
*  implied warranty of merchantability or fitness), with regard to the    *
*  software.                                                              *
*                                                                         *
*                                                                         *
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fs.h"
#include "arc.h"
#include "prefetch.h"

extern CDB *freebufs;
extern CDB *blkhash; //hash table
extern unsigned allocBufInCache; //number of allocated buffers
extern unsigned	cachesize; /* The total buffer cache size in the system */
extern unsigned	accesses;
extern unsigned	hit;
extern unsigned	miss;

extern CDB arcLists[5]; // arb 5 is for free list
extern long T1Length, T2Length, B1Length, B2Length;
extern long target_T1;

void do_LRU(int filD, unsigned inode, unsigned block, int type, int rok)
{
	perform_io(filD, inode, block, type,rok); // arb
	// arb using above instead of the following
	//pgref(inode, block);	/* page(disk block) reference */

}

void reportLRU(void)
{
    printf("LRU : Accesses %d Hit %d Miss %d Ratio %d\n", accesses, hit, miss, (100*hit)/accesses);
    return;    
}

int LRUref(unsigned inode, unsigned block, int prefetch) //do cache thingy
{
    struct CDB *temp;

    if (!prefetch) accesses++;

    temp = locate(inode, block); //find the requested page
    if (temp != NULL) 
    {	
      if (temp->prefetch) { // arb 
	temp->prefetch = 0;
	
	if (check_group_ready(inode,block)) {
	  hit++;
	  prefetch_hits ++;
	} else {
	  miss++;
	}
      } else 
	hit++;
      
      if (prefetch)
	printf("Hit on a prefetch ...???\n");
      
	// just take it from the list and add to MRU location	
      remove_from_list(temp); //take off whichever list
      mru_insert(temp, T1); 
    }
    else // page is not in cache directory
    { 
      if (!prefetch)    	miss++;
      if ((unsigned)T1Length < cachesize) {
	temp = get_new_CDB();
      } else {
	temp = lru_remove(T1); 
	T1Length--;
      }
      
      mru_insert(temp, T1); 
      T1Length++; // bookkeep:

      hashout(temp); //remove from hash table if there
      temp->inode = inode;
      temp->block = block;
      if (prefetch == 1) 
	temp->prefetch = 1;
      else 
	temp->prefetch = 0;
      hashin(temp); //put it back in
    }
    return 0;
}   

