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

unsigned	K = 0; //set it to cachesize 

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "arc.h"
#include "mq.h"
#include "prefetch.h"


void do_TQ(int filD, unsigned inode, unsigned block, int type, int rok)
{
	perform_io(filD, inode, block, type,rok); // arb
	//TQref(inode, block,0);	/* page(disk block) reference */
	
	// arb using above instead of the following
	//pgref(inode, block);	/* page(disk block) reference */

}

void reportTQ(void)
{
    printf("2Q : Accesses %d Hit %d Miss %d Ratio %d\n", accesses, hit, miss, (100*hit)/accesses);
    return;    
}

int TQref(unsigned inode, unsigned block, int prefetch) //do cache thingy
{
    struct CDB *b=NULL;
    unsigned retval= 0;
    if (!prefetch) {
      curVtime++;
      accesses++;
    }
    

    b = locate(inode, block); //find the requested page

    if(b!= NULL) //in the cache or ghost cache
    {
	if(b->where == Am) // is in cache
	{

	  if (b->prefetch) { // arb 
	    if (prefetch)
	      printf("Hit on a prefetch ...???\n");
	    b->prefetch = 0;
	
	    if (check_group_ready(inode,block)) {
	      hit++;
	      prefetch_hits ++;
	    } else {
	      miss++;
	    }
	  } else
	    hit++;	    
	  retval++;
	  MQremove_from_list(b);
	  MQmru_insert(b, Am);
	}
	else if(b->where == GHOSTQ)
	{
	  MQremove_from_list(b);	   
	    TQEvictBlock(GHOST); // make room in real cache
	    if (!prefetch) {
	      miss++;
	      ghosthit++;
	    }
	    b->prefetch = prefetch; // arb -- check this

	    MQmru_insert(b, Am);

	}
	else //hit in A1
	{
	  //	  if (b->where != A1) 
	  //	    printf("prob\n");
	  if (b->prefetch) { // arb 
	    if (prefetch)
	      printf("Hit on a prefetch ...???\n");
	    b->prefetch = 0;
	    
	    if (check_group_ready(inode,block)) {
	      hit++;
	      prefetch_hits ++;
	    } else {
	      miss++;
	    }
	  } else
	    hit++;	    
	  retval++;
	}

    }
    else
    {
      // we check if the block was prefetched and not accessed so far
      // if this happens, we simply discard it instead of moving it
      // to the GHOST cache
	b = TQEvictBlock(CACHE);
	if (!prefetch)  	miss++;
	
	b->inode = inode;
	b->block = block;
	b->prefetch = prefetch;	
	hashin(b); //with new block info
	MQmru_insert(b, A1);
    }
    
    return retval;
}

CDB *TQEvictBlock(unsigned type)
{
    CDB *victim=NULL;
    //first check if the cache is full
    
    if(allocBufs < cachesize) //cache not full just return empty block
    {
      //      if (type == GHOST) 
      //	printf("Shoulnt happen 2q\n");
	allocBufs++;
	victim = get_new_CDB();
    }
    else if (qLength[A1] > K)//need to find a victim
    {
	victim = MQlru_remove(A1);
	// now if the victim was a prefetched block, and was never accessed
	// we do not need to put it on GHOSTQ, we simply discard it
	if (victim->prefetch) {
	  //	  static int xxx = 0;
	  //	  fprintf(stderr,"%u.",xxx++);
	  hashout(victim);
	  free_CDB(victim);
	  
	}
	else
	  MQmru_insert(victim, GHOSTQ);
	//check if ghost full. if so remove from ghost
	if(type == CACHE) //make room in the ghost cache
	{
	    if(qLength[GHOSTQ] > ghostSize) //remove
	    {
		victim = MQlru_remove(GHOSTQ);
		hashout(victim);
	    }
	    else
		victim = get_new_CDB();
	}
	else //type == ghost we alredy have a ghost buffer
	    victim = NULL;
    }
    else //rm from AM
    {
	victim = MQlru_remove(Am);
	hashout(victim);
	if(type == GHOST) //make room in the ghost cache
	  //mru_insert(victim, FREEQ);
	  free_CDB(victim);
    }
    return victim;
}


int TQCHECK(unsigned block, unsigned inode)
{
  struct CDB *b = locate(inode, block);
  //  printf("%u %u %p\n",inode,block,b);
  if (b) {
    if(b->where == A1 || b->where == Am )
      return 1;
  }
  return 0;
}
