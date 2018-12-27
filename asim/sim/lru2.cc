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
#include "opt.h"
#include "util.h"

#define OLD_METHOD 1
#undef OLD_METHOD

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

unsigned CRP = 20; // LRU-K Correlated Reference Period
#define RIP cachesize // retained information period 

void do_LRU2(int filD, unsigned inode, unsigned block, int type, int rok)
{

	perform_io(filD, inode, block, type,rok); // arb
	// arb using above instead of the following
	//pgref(inode, block);	/* page(disk block) reference */

}

void reportLRU2(void)
{
    printf("LRU-2 : Accesses %d Hit %d Miss %d Ratio %d ents (%u %lu:%lu)\n", 
	   accesses, hit, miss, (100*hit)/accesses,memUsed,
	   T1Length+T2Length,T2Length);
    return;    
}
unsigned KC1 = 0;
unsigned KC2 = 0;
unsigned KC3 = 0;

int LRU2ref(unsigned inode, unsigned block, int prefetch) //do cache thingy
{
    struct CDB *p,*q,*victim;
    unsigned currCPR;
    if (!prefetch) {
      curVtime++;
      accesses++;    
    }

    // lets promote the ones for which time has come
    q = arcLists[T2].lrunext;
    while (q != &(arcLists[T2])) {
      p = q->lrunext;
      if ((curVtime - q->last) > CRP) { /*eligible for replacement*/
	//	printf("inserting %d\n",q->ARC_where);
	remove_from_list(q);
	T2Length--;
	mru_insert(q,T1);
	T1Length++;
	// also put it on the heap
	assert(q->lrunext);
	assert(q->lruprev);
	insert_heap(q,MAXINT - q->HIST[__K-1]); // was there working	
      }
      q = p;
    }

    p = locate(inode, block); //find the requested page
    // if p is already in the buffer
    if (p != NULL && p->isResident) {	
      if (p->prefetch) { // arb 
	p->prefetch = 0;
	
	// ignore the previous access so the penultimate references are 
	// not screwed up
	p->last = curVtime;
	if (p->HIST[0] == 0)
	  p->HIST[0] = curVtime;

	if (check_group_ready(inode,block)) {
	  hit++;
	  prefetch_hits ++;
	} else {
	  miss++;
	}
      } else 
	hit++;
      
      /* update history information of p */
      if ((curVtime - p->last) > CRP) {
		remove_heap(p);
	/* a new, uncorrelated reference */
	currCPR = p->last - p->HIST[0];
	for (int i = 1; i < __K; i++) {
	  p->HIST[i] = p->HIST[i-1] + currCPR;
	}
	p->HIST[0] = curVtime;
	p->last = curVtime;
	
	remove_from_list(p);
	T1Length--;
	mru_insert(p,T2);
	T2Length++;

	//	insert_heap(p,MAXINT - p->HIST[__K-1]);

	//	if (PQ[p->heap_index].key != MAXINT - p->HIST[__K-1]) {
	//	  printf("problem\n");
	//	  exit(0);
	//	}
      } else {
	/* a correlated reference */
	p->last = curVtime;
      }

      // the following two were for LRU ... need to change
      //      remove_from_list(p);
      //      mru_insert(p, T1); 
    } else { // page is not in cache directory 
      if (!prefetch)    	miss++;
      
      if ((unsigned)(T1Length+T2Length) < cachesize) {
	// the cache is not full yet... just use an available block
	if (!p) {
	  p = get_new_CDB();
	  //	  printf("using %d %u %u\n",p->isResident, curVtime,p->last);
	} else {
	  remove_from_list(p); // p is on unused list
	  B2Length--;
	}
      } else {	/* select replacement victim */
	KC1++;
	victim = heap_max();
	if (!victim) {
	  //	  printf("This is gonna be fun\n");
	  // in case heap is empty, we get one from T2 
	  victim = lru_remove(T2);
	  T2Length--; 
	} else {
	  remove_from_list(victim);
	  T1Length--; 
	  KC2++;
	}

	victim->isResident = 0;
	// the ghost cache here may exceed maxsize, lets trim it
	
	// A prefetched block that is not accessed and then evicted is
	// is not moved to ghostQ
	if (victim->prefetch) {
	  hashout(victim);
	  free_CDB(victim);
	  KC3++;
	} else {
	  mru_insert(victim,B2);
	  B2Length++;
	}

	// if p exists it has to be on B2
	if (p) {
	  assert(p->ARC_where == B2);
	  remove_from_list(p);
	  B2Length--;
 	} else {
	  // otherwise create a new entry for p
	  p=get_new_CDB();	  
	  
	  // Is ghost cache too long 
	  if ((unsigned)B2Length > RIP) {
	    q = lru_remove(B2);
	    B2Length--;
	    hashout(q);
	    free_CDB(q);
	  }

	}
	

      }      /* end select replacement victim */
      
      /* now fetch the referenced page */
      // fetch p into the buffer frame that was previously held by victim
      
      hashout(p); //remove from hash table if there
      p->inode = inode;
      p->block = block;
      p->prefetch = prefetch;
      p->isResident = 1;
      hashin(p); //put it back in
      
      // if HIST(p) does not exist
      if (!p->histValid) {
	/* initialize history control block */
	p->histValid = 1;
	p->HIST[0] = 0;
	for (int i = 1; i < __K; i++)
	  p->HIST[i] = 0;
      } else {
	if (!prefetch)
	  for (int i = 1; i < __K; i++)
	    p->HIST[i] =  p->HIST[i-1];
      }
      
      if (!prefetch)
	p->HIST[0] = curVtime;

      p->last = curVtime;
      //insert_heap(p,MAXINT - p->HIST[__K-1]); // was there working
      //      mru_insert(p,B1); 

      mru_insert(p, T2);  // put it on a wait, till its accesse for CRP
      T2Length++; // bookkeep:
    } // end page is not in cache directory 
    return 0;
}   


