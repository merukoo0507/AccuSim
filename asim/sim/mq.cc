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
#include "arc.h"
#include "mq.h"
#include "prefetch.h"



unsigned allocBufs; //number of allocated buffers
unsigned 	numQ = 4;  //number of mqs
unsigned	lifeTime = 0; //set it to cachesize / numQ
unsigned	ghostSize = 0; //set it to cachesize 
unsigned	ghosthit = 0;


CDB qLists[MAXQ]; //max 10 mq in mq + ghost queue q11 is always ghost
unsigned qLength[MAXQ]; //size of each list

void do_MQ(int filD, unsigned inode, unsigned block, int type, int rok)
{
       	perform_io(filD, inode, block, type,rok); // arb
	//	MQref(inode, block,0);	/* page(disk block) reference */
	
	// arb using above instead of the following
	//pgref(inode, block);	/* page(disk block) reference */

}


void reportMQ(void)
{
    printf("MQ : Accesses %d Hit %d Miss %d Ratio %d\n", accesses, hit, miss, (100*hit)/accesses);
    return;    
}

void initMQ(void)
{
    if(numQ > GHOSTQ)
    {
	printf("Increase MAXQ define\n");
	exit(-1);
    }
    if(lifeTime == 0) //use defaults
	lifeTime = cachesize; // numQ; //set it to cachesize / numQ

    if(ghostSize == 0) //use defaults
	ghostSize = cachesize; //set it to cachesize 

    if(K == 0) //use defaults
        K = cachesize/4; //set it to cachesize 


    qLength[GHOSTQ] = 0; //clear ghost queue
    for(unsigned i = 0; i < numQ; i++)
	qLength[i] = 0;
    
    allocBufs = 0;
  
    for(unsigned i=0;i<numQ;i++)
    {
	qLists[i].lrunext = qLists[i].lruprev = &(qLists[i]);
	qLists[i].inode = 0;
	qLists[i].block = 0;
	qLists[i].expireTime = 0;
	qLists[i].reference = 0;
	qLists[i].where = 0;
	qLists[i].hashnext = qLists[i].hashprev = NULL;
    }
    //allocate ghost
    qLists[GHOSTQ].lrunext = qLists[GHOSTQ].lruprev = &(qLists[GHOSTQ]);
    qLists[GHOSTQ].inode = 0;
    qLists[GHOSTQ].block = 0;
    qLists[GHOSTQ].expireTime = 0;
    qLists[GHOSTQ].reference = 0;
    qLists[GHOSTQ].where = 0;
    qLists[GHOSTQ].hashnext = qLists[GHOSTQ].hashprev = NULL;
}

int MQref(unsigned inode, unsigned block, int prefetch) 
{
    struct CDB *b=NULL;
    unsigned retval= 0;
    if (!prefetch) {
  	curVtime++;
	accesses++;
    }

    b = locate(inode, block); //find the requested page
    //printf("%u %u %p\n",inode,block,b);
    if(b!= NULL) //in the cache or ghost cache
    {
	MQremove_from_list(b); //take off whichever list
	if(b->where < GHOSTQ) // is in cache
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
	    
	}
	else //ghost cache
	{
	    EvictBlock(GHOST); // make room in real cache
	    if (!prefetch)  {
	      miss++;
	      ghosthit++;
	    }
	    b->prefetch = prefetch; // arb -- check this
	}
    }
    else
    {
	b = EvictBlock(CACHE);
	if (!prefetch)  	miss++;
	
	b->inode = inode;
	b->block = block;
	b->reference = 0;
	b->prefetch = prefetch;
	hashin(b); //with new block info
    }
    
    if (prefetch)
      b->reference++;
    b->expireTime = curVtime + lifeTime;
    MQmru_insert(b,QueueNum(b->reference));
    Adjust();

    return retval;
}

unsigned QueueNum(unsigned refs)
{
    unsigned i = 0;
    while((refs = refs >> 1))
	i++;
    if(i >= numQ)
    	return numQ-1;
    return i;
}

CDB *EvictBlock(unsigned type)
{
    unsigned i;
    CDB *victim=NULL;
    //first check if the cache is full
    if(allocBufs < cachesize) //cache not full just return empty block
    {
	allocBufs++;
	victim = get_new_CDB();
    }
    else //need to find a victim
    {
	for( i = 0; i < numQ; i++)
	    if(qLength[i])
		break;
	
	victim = MQlru_remove(i);
	// now if the victim was a prefetched block, and was never accessed
	// we do not need to put it on GHOSTQ, we simply discard it
	if (victim->prefetch) {
	  static int xxx = 0;
	  if (i !=0)
	    fprintf(stderr,"%d %u.",i,xxx++);
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

    return victim;
}

void Adjust(void)
{
    unsigned k;
    CDB *c;

    for(k=1; k < numQ; k++)
    {
	if(qLength[k])
	{
	    c = get_lru(k);
	    if(c->expireTime < curVtime)
	    {
		c = MQlru_remove(k);
		MQmru_insert(c,k-1);
		c->expireTime = curVtime + lifeTime;
	    }
	}
    }
}

/* Functions which are related hash to find buffer containing
   disk block whose block number is "blkno" */



/* Remove and return LRU(Least Recently Used block from the list */
CDB *MQlru_remove(unsigned queue)
{
    CDB *bp;

    bp = qLists[queue].lruprev;
    (bp->lruprev)->lrunext = bp->lrunext;
    (bp->lrunext)->lruprev = bp->lruprev;
    bp->lrunext = bp->lruprev = NULL;
    qLength[queue]--;

    return(bp);
}
/* Return LRU(Least Recently Used block from the list */
CDB *get_lru(unsigned queue)
{
    return qLists[queue].lruprev;
}


void MQmru_insert(CDB *bp, unsigned queue) // Insert a block to the MRU location
{
    CDB *dp;
 
    dp = &qLists[queue];
    
    bp->lrunext = dp->lrunext;
    bp->lruprev = dp;
    (dp->lrunext)->lruprev = bp;
    dp->lrunext = bp;
    bp->where = queue;
    qLength[queue]++;

}


void MQremove_from_list(CDB *bp) //take off whichever list
{
    
    (bp->lruprev)->lrunext = bp->lrunext;
    (bp->lrunext)->lruprev = bp->lruprev;
    bp->lrunext = bp->lruprev = NULL;
    qLength[bp->where]--;
}

int MQCHECK(unsigned block, unsigned inode)
{
  struct CDB *b = locate(inode, block);
  //  printf("%u %u %p\n",inode,block,b);
  if (b) {
    if(b->where < GHOSTQ)
      return 1;
  }
  return 0;
}
