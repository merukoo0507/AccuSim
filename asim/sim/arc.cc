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

CDB *freebufs = NULL;
CDB *blkhash = NULL; //hash table

unsigned allocBufInCache; //number of allocated buffers

unsigned cachesize = 100;	/* The total buffer cache size in the system */

unsigned	curVtime = 0;
unsigned	accesses = 0;
unsigned	hit = 0;
unsigned	miss = 0;



CDB arcLists[5]; // arb 5 is for free list
long T1Length, T2Length, B1Length, B2Length;
long target_T1;

void do_ARC(int filD, unsigned inode, unsigned block, int type, int rok)
{
	perform_io(filD, inode, block, type,rok); // arb
	// arb using above instead of the following
	//pgref(inode, block);	/* page(disk block) reference */

}

void reportARC(void)
{
    printf("ARC : Accesses %d Hit %d Miss %d Ratio %d\n", accesses, hit, miss, (100*hit)/accesses);
    return;    
}

void initARC(void)
{
    T1Length = T2Length = B1Length = B2Length = 0;
    
    target_T1 = cachesize / 2;
    
    allocBufInCache = 0;
    // arb fix the following for free access
    // for mq ... this cachesize+ghostsize
    freebufs = (CDB *)calloc((2*cachesize + 2), sizeof(CDB));  //arb20 was2
    {
      unsigned xx = 0;
      for (xx=0;xx<(2*cachesize + 1);xx++) {
	freebufs[xx].freenext = &(freebufs[xx+1]);
      }
      freebufs[xx].freenext = &(arcLists[4]);
    }
    

    blkhash = (CDB *)calloc(BLKHASHBUCKET, sizeof(CDB)); 
   
    for (int i = 0; i < BLKHASHBUCKET; i++)
	blkhash[i].hashnext = blkhash[i].hashprev = &(blkhash[i]);
  
    for(int i=0;i<5;i++)
    {
	arcLists[i].lrunext = arcLists[i].lruprev = &(arcLists[i]);
	arcLists[i].inode = 0;
	arcLists[i].block = 0;
	arcLists[i].ARC_where = 0;
	arcLists[i].hashnext = arcLists[i].hashprev = NULL;
	arcLists[i].freenext = NULL;
    }

    arcLists[4].freenext = freebufs;
    

}

CDB *replace()
{
    CDB* temp;
    if (T1Length >= max(1,target_T1)) //T's size exceeds target?
    {
	//yes: T1 is too big 
	temp = lru_remove(T1); //grab LRU from T1
	T1Length--; //bookkeep
	
	if (!temp->prefetch) {
	  mru_insert(temp, B1); //put it on B1
	  //temp->ARC_where = B1; //note that fact
	  B1Length++; //bookkeep
	}
    }
    else
    {
	//no: T1 is not too big
	temp = lru_remove(T2); //grab LRU page of T2
	mru_insert(temp, B2); //put it on B2
	//temp->ARC_where = B2; //note that fact

	T2Length--; //bookkeep
	B2Length++; //bookkeep
    }
    //if (temp->dirty) destage(temp); //if dirty, evict before overwrite
    return temp;
    //return (temp->inode << 16)^temp->block;
}



int pgref(unsigned inode, unsigned block, int prefetch) //do cache thingy
{
    struct CDB *temp;
    CDB* replaceBlock =NULL;
    if (!prefetch) {
      curVtime++;
      accesses++;
    }

    temp = locate(inode, block); //find the requested page
    if (temp != NULL) 
    {
	//IN CACHE; check lists
	switch (temp->ARC_where)
	{
	  case T1:
	    if (temp->prefetch) { // arb 
	      if (prefetch)
		printf("Hit on a prefetch ...???\n");
	      temp->prefetch = 0;
	
	      if (check_group_ready(inode,block)) {
		hit++;
		prefetch_hits ++;
	      } else {
		miss++;
	      }

	      break;
	    }
	    T1Length--;
	    T2Length++;
	    //fall through
	  case T2:
	    remove_from_list(temp); //take off whichever list
	    mru_insert(temp, T2); //seen twice recently, put on T2
	    // temp->ARC_where = T2; // note that fact
	    //if (dirty) temp->dirty = dirty; // bookkeep dirty
	    if (!prefetch) hit++;
	    break;
	  case B1:
	  case B2:
	    if (temp->ARC_where == B1) // B1 hit: favor recency
	    { 
		target_T1 = min((unsigned)(target_T1 + max(B2Length/B1Length, 1)), cachesize);
		// adapt the target size
		B1Length--; // bookkeep
	    }
	    else // B2 hit: favor frequency
	    { 
		target_T1 = max(target_T1 - max(B1Length/B2Length, 1), 0);
		// adapt the target size
		B2Length--; // bookkeep
	    }

	    remove_from_list(temp); // take off whichever list
	    replaceBlock = replace(); // find a place to put new page
	    // discard prefetched block that is evicted without on-demand acc
	    if (replaceBlock->prefetch) {
	      hashout(replaceBlock);
	      free_CDB(replaceBlock);
	    }

	    hashout(temp); //remove from hash table

	    temp->inode = inode; // bookkeep
	    temp->block = block; // bookkeep
	    temp->prefetch = 0; // shouldnt happen on a prefetch 
	    if (prefetch) 
	      printf("Prefetching ghost?\n");
	    hashin(temp); //hash in with the new block

	    //temp->dirty = dirty; // bookkeep
	    mru_insert(temp, T2); // seen twice recently, put on T2
	    T2Length++;
	    //temp->ARC_where = T2; // note that fact
	    //fetch(page_number, temp->pointer, dirty); // load page into cache
	    if (!prefetch)	    miss++;
	    break;
	}
    }
    else // page is not in cache directory
    { 
      if (!prefetch)    	miss++;
	if ((unsigned)(T1Length + B1Length) == cachesize) // B1 + T1 full?
	{ 
	    if ((unsigned)T1Length < cachesize)// Still room in T1?
	    { 
		replaceBlock = replace(); // find new place to put page
		if (replaceBlock->prefetch) 
		  temp = replaceBlock;
		else {
		  temp = lru_remove(B1); // yes: take page off B1
		  B1Length--; // bookkeep that
		}

	    }
	    else // no: B1 must be empty
	    { 
		temp = lru_remove(T1); // take page off T1
		//if (temp->dirty) destage(temp); // if dirty, evict before overwrite
		T1Length--; // bookkeep that
	    }
	}
	else// B1 + T1 have less than cachesize pages
	{ 
	    if ((unsigned)(T1Length + T2Length + B1Length + B2Length) >= cachesize)// cache full?
	    { 
		// Yes, cache full:
		if ((unsigned)(T1Length + T2Length + B1Length + B2Length) == 2*cachesize)
		{
		    // directory is full:
		    B2Length--; // find and reuse B2's LRU
		    temp = lru_remove(B2);
		}
		else // cache directory not full, easy case
		    temp = get_new_CDB();
		
		replaceBlock = replace(); // new place for page
		if (replaceBlock->prefetch) {
		  hashout(replaceBlock);
		  free_CDB(replaceBlock);
		}
		
	    }
	    else // cache not full, easy case
	    { 
		temp = get_new_CDB();
		replaceBlock = NULL;
	    }
	}

	mru_insert(temp, T1); // seen once recently, put on T1
	T1Length++; // bookkeep:
	//temp->ARC_where = T1;

	hashout(temp); //remove from hash table if there
	temp->inode = inode;
	temp->block = block;
	if (prefetch == 1) 
	  temp->prefetch = 1;
	else 
	  temp->prefetch = 0;
	hashin(temp); //put it back in
	//temp->dirty = dirty;
	//fetch(page_number, temp->pointer, dirty); // load page into cache
    }
    return 0;
}   

void free_CDB(CDB *d)
{
  memUsed--;
  d->freenext = arcLists[4].freenext;
  arcLists[4].freenext = d;
  d->inode = 0;
  d->block = 0;
  d->ARC_where = 0;
  d->prefetch = 0;

  d->where = 0;
  d->histValid = 0;
  d->isResident = 0;
  d->last = 0;
}   

unsigned memUsed = 0;

CDB *get_new_CDB(void)
{ 
  // arcLists[5].freenext 
  CDB *d;
  memUsed++;

  if (arcLists[4].freenext->freenext == &(arcLists[4])) {
    CDB *fbufs = (CDB *)calloc((2*cachesize + 2), sizeof(CDB));  //arb20 was2
    {
      unsigned xx = 0;
      for (xx=0;xx<(2*cachesize + 1);xx++) {
	fbufs[xx].freenext = &(fbufs[xx+1]);
      }
      fbufs[xx].freenext = &(arcLists[4]);

      arcLists[4].freenext->freenext = fbufs;
    }
    
    //    printf("Out of memory\n");
    //    exit(-1);
  }
  
  d = arcLists[4].freenext;
  arcLists[4].freenext = d->freenext;
  d->freenext = NULL;
  return d;
  
  //  return(&(freebufs[allocBufInCache++]));
}

/* Functions which are related hash to find buffer containing
   disk block whose block number is "blkno" */

struct CDB *locate(unsigned inode, unsigned block)
{
    int	off;
    CDB *dp, *bp;
    
    off = ((inode << 16)^block) % BLKHASHBUCKET;
    dp = &(blkhash[off]);
    for (bp = dp->hashnext; bp != dp; bp = bp->hashnext)
	if (bp->inode == inode && bp->block == block)
	    return(bp);

    return(NULL);
}

void hashin(CDB *bp)
{
    int	off;
    CDB *dp;
    
    off = ((bp->inode << 16)^bp->block) % BLKHASHBUCKET;
    dp = &(blkhash[off]);
    bp->hashnext = dp->hashnext;
    bp->hashprev = dp;
    (dp->hashnext)->hashprev = bp;
    dp->hashnext = bp;
}

void hashout(CDB *bp)
{
    if(bp->hashnext == NULL)
	return;
    (bp->hashprev)->hashnext = bp->hashnext;
    (bp->hashnext)->hashprev = bp->hashprev;
    bp->hashnext = bp->hashprev = NULL;
}

/* Remove and return LRU(Least Recently Used block from the list */
CDB *lru_remove(unsigned arcList)
{
    CDB *bp;

    bp = arcLists[arcList].lruprev;
    (bp->lruprev)->lrunext = bp->lrunext;
    (bp->lrunext)->lruprev = bp->lruprev;
    bp->lrunext = bp->lruprev = NULL;

    return(bp);
}


void mru_insert(CDB *bp, unsigned arcList) // Insert a block to the MRU location
{
    CDB *dp;
 
    dp = &arcLists[arcList];
    
    bp->lrunext = dp->lrunext;
    bp->lruprev = dp;
    (dp->lrunext)->lruprev = bp;
    dp->lrunext = bp;
    bp->ARC_where = arcList;
}


void remove_from_list(CDB *bp) //take off whichever list
{
    
    (bp->lruprev)->lrunext = bp->lrunext;
    (bp->lrunext)->lruprev = bp->lruprev;
    bp->lrunext = bp->lruprev = NULL;
}
