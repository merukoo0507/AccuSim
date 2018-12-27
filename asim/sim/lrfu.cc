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
#include "prefetch.h"
#include "opt.h"
#include "util.h"

//#define	MAXBUF		35000
//#define	BLKHASHBUCKET	23617
#define	MAXHISTORY	220000

#define	PRIQUEUE	0
#define	LRULIST		1

#define	FALSE		0
#define	TRUE		1

#define	BLOCKSIZE	4096


struct buffer {
	unsigned	blkno;
	unsigned	inode;
	double	value;
	long	lastreftime;
	int	pqindex;
	int	where;
  
  int prefetch;

	struct buffer *hashnext, *hashprev;
	struct buffer *lrunext, *lruprev;
};
struct buffer   buf[MAXBUF];

struct buffer	lrulist;

struct buffer	*pq[MAXBUF+1];

struct buffer	blkhashLRFU[BLKHASHBUCKET];

struct history {
	double value;
	long	lastreftime;
} hist[MAXHISTORY];


unsigned	bufincache = 0;
int	pqused = 0;

long	curtime = 0L;

double	lambda = 0.5;
int	threshold;
int	corperiod = 0;
int	history = 0;
double	defaultinit = 1.0;

struct buffer 	*findblk(unsigned blkno, unsigned inode);
struct buffer 	*allocbuf(void);
struct buffer 	*pqout(void);
struct buffer 	*replace(struct buffer *bp);
struct buffer 	*lruout(void);
void 		reorder(int dir, struct buffer *bp);
double 		calc_defaultinit(double lambda);
int 		calc_threshold(double lambda);
void 		init(void);
void 		printstat(void);
void 		rmfromlrulist(struct buffer *bp);
void 		hashin(struct buffer *bp);
void 		hashout(struct buffer *bp);
void 		pqin(struct buffer *bp);
void 		lruin(struct buffer *bp);
int 		lruempty(void);
void 		downheap(int index);
void 		upheap(int index);
double 		log_half(double x);


void initLRFU(void)
{
  //   printf("cache size = %d, lambda = %1.10lf, corperiod = %d\n",
  //	   cachesize, lambda, corperiod);
    if ((cachesize == 0) || (lambda > 1.0) || (lambda < 0.0) ||	(corperiod < 0))
    {
	printf("Option is not set properly\n");
	exit(1);
    }
    threshold = calc_threshold(lambda);
    defaultinit = calc_defaultinit(lambda);
    //    printf("history = %d, threshold = %d\n", history, threshold);
    
    init();
}

void cacheAccess(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc)
{	
    
    unsigned blockstart, blockend, block;
    //find if exists
    
    if(inode == 0)
	printf("Inode == 0\n");
    
    if(size == 0)
    	return;

    blockstart = position / BLOCKSIZE;
    blockend = (position + size - 1)/ BLOCKSIZE;
    
    for(block = blockstart; block <= blockend; block++) 
    {
	curtime++;
/*	if ((curtime % 10000L) == 0L)
	{
	    putc('#', stdout);
	    fflush(stdout);
	}*/

	LRFUref(block, inode,0);/* page(disk block) reference */
	
    }
}

void do_LRFU(int filD, unsigned inode, unsigned block, int type, int rok)
{
  perform_io(filD,inode,block,type,rok);
}

void init(void)
{
    int	i;
    
	for (i = 0; i < MAXBUF+1; i++) pq[i] = NULL;
	for (i = 0; i < MAXBUF; i++) {
		buf[i].blkno = 0;
		buf[i].inode = 0;
		buf[i].value = 0.0;
		buf[i].lastreftime = 0L;
		buf[i].pqindex = 0;
		buf[i].hashnext = buf[i].hashprev = NULL;
		buf[i].lrunext = buf[i].lruprev = NULL;
	}
	for (i = 0; i < BLKHASHBUCKET; i++) {
		blkhashLRFU[i].blkno = MAXINT;
		blkhashLRFU[i].inode = MAXINT;
		blkhashLRFU[i].value = 0.0;
		blkhashLRFU[i].lastreftime = 0L;
		blkhashLRFU[i].pqindex = 0;
		blkhashLRFU[i].hashnext = blkhashLRFU[i].hashprev = &(blkhashLRFU[i]);
	}
	for (i = 0; i < MAXHISTORY; i++) {
		hist[i].value = 0.0;
		hist[i].lastreftime = 0L;
	}
	lrulist.lrunext = lrulist.lruprev = &(lrulist);
}

int LRFUCHECK(unsigned block, unsigned inode)
{
  struct buffer	*bufp = findblk(block, inode);
  
  //  printf("(%u\t%u) %p\n",inode,block,bufp);
  
  if (bufp)
    return 1;

  return 0;
}

void LRFUref(unsigned blkno, unsigned inode, int prefetch)		/* called when page(disk block) is referenced */
{
	struct buffer	*bufp;
	long		delta;
	double		oldvalue;
	int dontMove = 0;

	if (!prefetch)
	    curtime++;

	bufp = findblk(blkno, inode);
	//	printf("----------------->(%u\t%u) %p\n",inode,blkno,bufp);
	if (bufp) {	/* blk exists in cache */
	  
	  if (bufp->prefetch) { // arb 
	    bufp->prefetch = 0;
	    dontMove = 1;

	    // do some fixing here
	    delta = curtime - bufp->lastreftime;
	    oldvalue = pow(0.5, (double)lambda * (double)delta) * bufp->value;

	    bufp->value = defaultinit;
	    bufp->lastreftime = curtime;
	    if (bufp->where == PRIQUEUE) {
	      //fprintf(stderr,"%d\n",(oldvalue > bufp->value) ? 0 : 1);
	      reorder((oldvalue > bufp->value) ? 0 : 1, bufp);
	      /* 0 : upheap, 1 : downheap */
	    } else { /* bufp->where == LRULIST */
	      rmfromlrulist(bufp);
	      if (pqused < threshold) pqin(bufp);
	      else lruin(replace(bufp));
	    }
	    
	    if (check_group_ready(inode,blkno)) {
	      hit++;
	      prefetch_hits ++;
	    } else {
	      miss++;
	    }
	  } else 
	    hit++;
 
	  if (!dontMove) {
	    delta = curtime - bufp->lastreftime;
	    bufp->lastreftime = curtime;
	    if (delta > corperiod) {
	      oldvalue = pow(0.5, (double)lambda * (double)delta) * bufp->value;
	      bufp->value = defaultinit + pow(0.5, (double)lambda * (double)delta) * bufp->value;
	      if (bufp->where == PRIQUEUE) {
		reorder((oldvalue > bufp->value) ? 0 : 1, bufp);
		/* 0 : upheap, 1 : downheap */
	      }
	      else { /* bufp->where == LRULIST */
		rmfromlrulist(bufp);
		if (pqused < threshold) pqin(bufp);
		else lruin(replace(bufp));
	      }
	    } else {
	      if (bufp->where == PRIQUEUE) {
		reorder(1, bufp);
		/* 0 : upheap, 1 : downheap */
	      }
	      else { /* bufp->where == LRULIST */
		rmfromlrulist(bufp);
		if (pqused < threshold) pqin(bufp);
		else lruin(replace(bufp));
	      }
	    }
	  }
	}
	else {
	  if (!prefetch) 	miss++;
	  if (bufincache < cachesize) {	/* cache is not full. warming up */
	    bufp = allocbuf();
	    bufp->blkno = blkno;
	    bufp->inode = inode;
	    bufp->value = defaultinit;
	    //	    if (!prefetch) 
	      bufp->lastreftime = curtime;
	      //	    else
	      //	      bufp->lastreftime = 0;

	    bufp->prefetch = prefetch;
	    
	    hashin(bufp);
	    //	    fprintf(stderr,"-->%d\n",threshold);
	    if (pqused < threshold) pqin(bufp);
	    else lruin(replace(bufp));
	  }
	  else {
	    if (lruempty()) bufp = pqout();	/* find a victim block */
	    else bufp = lruout();		/* find a victim block */
	    
	    hashout(bufp);

	    if (history) {			/* savehist(bufp); */
	      hist[bufp->blkno].value = bufp->value;
	      hist[bufp->blkno].lastreftime = bufp->lastreftime;
	    }
	    
	    bufp->blkno = blkno;	/* replace buf with new block */
	    bufp->inode = inode;	/* replace buf with new block */
	    
	    if (history && (hist[blkno].lastreftime > 0L)) {
	      long	delta;
	      
	      delta = curtime - hist[blkno].lastreftime;
	      if (delta <= corperiod) {
		bufp->value = hist[blkno].value;
	      }
	      else {
		bufp->value = defaultinit + pow(0.5, (double)lambda * (double)delta) * hist[blkno].value;
	      }
	      /*
		bufp->value = 1.0 +
		pow(0.5, (double)lambda * (double)delta) * (hist[blkno].value  - (delta < corperiod) ? 1.0 : 0.0);
	      */
	    }
	    else {
	      bufp->value = defaultinit;
	    }
	    //	    if (!prefetch) 
	      bufp->lastreftime = curtime;
	      //	    else
	      //	      bufp->lastreftime = 0;

	    bufp->prefetch = prefetch;
	    hashin(bufp);
	    if (pqused < threshold) pqin(bufp);
	    else lruin(replace(bufp));
	  }
	}
}

void reportLRFU(void)
{
    printf("LRFU : Accesses %ld Hit %d Miss %d Ratio %ld Lambda %.6f\n", 
	   curtime, hit, miss, (100*hit)/curtime,lambda);
    //	printf("\ntotal access = %ld, hit = %d, miss = %d\n", curtime, hit, miss);
    //	printf("Hit ratio = %f\n\n", (float) hit / (float) curtime);
}

struct buffer *allocbuf(void)
{
	return(&(buf[bufincache++]));
}

struct {
	double	lambda;
	int	threshold;
} thresholdtable[] =
{		{1.000000, 1},
		{0.900000, 1},
		{0.800000, 1},
		{0.700000, 1},
		{0.600000, 2},
		{0.500000, 3},
		{0.400000, 5},
		{0.300000, 8},
		{0.200000, 14},
		{0.100000, 39},
		{0.090000, 44},
		{0.080000, 52},
		{0.070000, 62},
		{0.060000, 76},
		{0.050000, 97},
		{0.040000, 129},
		{0.030000, 186},
		{0.020000, 309},
		{0.010000, 717},
		{0.009000, 814},
		{0.008000, 937},
		{0.007000, 1098},
		{0.006000, 1318},
		{0.005000, 1635},
		{0.004000, 2124},
		{0.003000, 2970},
		{0.002000, 4747},
		{0.001000, 10495},
		{0.000900, 11830},
		{0.000800, 13521},
		{0.000700, 15727},
		{0.000600, 18719},
		{0.000500, 22989},
		{0.000400, 29541},
		{0.000300, 40772},
		{0.000200, 64082},
		{0.000100, 138165},
		{0.000090, 155205},
		{0.000080, 176730},
		{0.000070, 204729},
		{0.000060, 242557},
		{0.000050, 296330},
		{0.000040, 378460},
		{0.000030, 518448},
		{0.000020, 806920},
		{0.000010, 1713841},
		{0.000009, 1921157},
		{0.000008, 2182542},
		{0.000007, 2521854},
		{0.000006, 2979229},
		{0.000005, 3627681},
		{0.000004, 4615084},
		{0.000003, 6291575},
		{0.000002, 9352540},
		{0.000001, 9998977},
		{0.0,      10000000}
};

int calc_threshold(double lambda)
{
/*
	double	g, sum = 0.0;

	for (g = 10000000.0; sum < 1; g -= 1.0) {
		sum += pow(0.5, (double) lambda * g);
	}
	return(g+1.0);
*/
/*
	int	i;

	if ((lambda > 1.0) || (lambda < 0.0)) {
		printf("Fatal Error : Illegal lambda\n");
		return(10000000);
	}
	for (i = 0; lambda < thresholdtable[i].lambda; i++) ;
	return(thresholdtable[i].threshold);
*/
        return(  (int) ceil( log_half((double) 1.0 - pow((double) 0.5, (double) lambda)) / lambda )  );
}

double log_half(double x)
{
        return(log(x) / log(0.5));
}

/*
double
calc_defaultinit(thres)
int     thres;
{
        if (thres > 100000) return(1000.0);
        if (thres > 10000) return(10000.0);
        if (thres > 1000) return(100000.0);
        if (thres > 100) return(1000000.0);
        return(10000000.0);
}
*/

double calc_defaultinit(double lambda)
{
        if (lambda >= 0.1) return(100000.0);
        if (lambda >= 0.01) return(1000.0);
        if (lambda >= 0.001) return(100.0);
        if (lambda >= 0.0001) return(10.0);
        return(1.0);
/*
        if (lambda >= 0.1) return(10000000.0);
        if (lambda >= 0.01) return(1000000.0);
        if (lambda >= 0.001) return(100000.0);
        if (lambda >= 0.0001) return(10000.0);
        return(1000.0);
*/
}

/* Functions which are related hash to find buffer containing
	disk block whose block number is "blkno" */

struct buffer *findblk(unsigned blkno, unsigned inode)
{
	int	off;
	struct buffer *dp, *bp;

	off = (HASH_FUNC(blkno, inode)) % BLKHASHBUCKET;
	dp = &(blkhashLRFU[off]);
	for (bp = dp->hashnext; bp != dp; bp = bp->hashnext) {
		if (bp->blkno == blkno && bp->inode == inode) {
			return(bp);
		}
	}
	return(NULL);
}

void hashin(struct buffer *bp)
{
	int	off;
	struct buffer *dp;

	off = (HASH_FUNC(bp->blkno, bp->inode)) % BLKHASHBUCKET;
	dp = &(blkhashLRFU[off]);
	bp->hashnext = dp->hashnext;
	bp->hashprev = dp;
	(dp->hashnext)->hashprev = bp;
	dp->hashnext = bp;
}

void hashout(struct buffer *bp)
{
	(bp->hashprev)->hashnext = bp->hashnext;
	(bp->hashnext)->hashprev = bp->hashprev;
}

/* LRU List management functions */

int lruempty(void)
{
	if (lrulist.lrunext == &(lrulist))
		return(TRUE);
	else
		return(FALSE);
}

/* Remove and return LRU(Least Recently Used block from LRU list */
struct buffer *lruout(void)
{
	struct buffer *bp;

	bp = lrulist.lruprev;
	(bp->lruprev)->lrunext = bp->lrunext;
	(bp->lrunext)->lruprev = bp->lruprev;

	return(bp);
}

void lruin(struct buffer *bp)			/* Insert a block to LRU list */
{
	struct buffer *dp;

	dp = &lrulist;
	bp->lrunext = dp->lrunext;
	bp->lruprev = dp;
	(dp->lrunext)->lruprev = bp;
	dp->lrunext = bp;

	bp->where = LRULIST;
}

/* Remove a block from LRU list */
void rmfromlrulist(struct buffer *bp)
{
	(bp->lruprev)->lrunext = bp->lrunext;
	(bp->lrunext)->lruprev = bp->lruprev;
}

/* Priority Queue Management Functions.
   See "Algorithms" written by Robert Sedgewick, pp 132-135 */

/* Replace a buffer whose value is least with new buffer pointed by bp */
/* Note that pq[1] points a buffer containing block whose value is least */
struct buffer *replace(struct buffer *bp)
{
	struct buffer	*tmpbp;

	tmpbp = pq[1];
	tmpbp->pqindex = 0;

	pq[1] = bp;
	bp->pqindex = 1;
	bp->where = PRIQUEUE;
	downheap(1);

	return(tmpbp);
}

/* Remove a block whose value is least from Priority Queue */
struct buffer *pqout(void)
{
	struct buffer *bp;

	bp = pq[1];
	bp->pqindex = 0;

	if (pqused > 1) {
		pq[1] = pq[pqused];
		pq[pqused--] = NULL;

		(pq[1])->pqindex = 1;
		downheap(1);
	}
	else {
		pq[pqused--] = NULL;
	}

	return(bp);
}

/* Calculate a value of a block, based on current time */
double keyvalue(struct buffer *bp)
{
	long	delta;

	delta = curtime - bp->lastreftime;
	return ( pow(0.5, (double) lambda * (double) delta) * bp->value );
}


void downheap(int index)
{
	int		j;
	struct buffer	*tmpbp;

	while (index <= (pqused/2)) {
		j = index+index;
		if (j < pqused) {
/*			if ((pq[j])->value > (pq[j+1])->value) j++; */
			if ((((pq[j])->lastreftime - (pq[j+1])->lastreftime) > (long) threshold) || (keyvalue(pq[j]) >= keyvalue(pq[j+1]))) j++;
		}
/*		if ((pq[index])->value >= (pq[j])->value) { */
		if ((((pq[index])->lastreftime - (pq[j])->lastreftime) > (long) threshold) || (keyvalue(pq[index]) >= keyvalue(pq[j]))) {
			(pq[index])->pqindex = j;
			(pq[j])->pqindex = index;
			tmpbp = pq[index];
			pq[index] = pq[j];
			pq[j] = tmpbp;
		}
		else break;
		index = j;
	}
}

/* Insert a block to Priority Queue */
void pqin(struct buffer *bp)
{
	pqused++;
	pq[pqused] = bp;
	bp->pqindex = pqused;
	bp->where = PRIQUEUE;
	upheap(pqused);
}

void upheap(int index)
{
	struct buffer	tmpbuf, *tmpbp;

	pq[0] = &(tmpbuf);
	tmpbuf.value = -1.0;
	tmpbuf.lastreftime = 0L;

/*	while ( (pq[index/2])->value > (pq[index])->value) { */
	while ((((pq[index/2])->lastreftime - (pq[index])->lastreftime) > (long) threshold)
		|| (keyvalue(pq[index/2]) > keyvalue(pq[index]))) {
		(pq[index])->pqindex = index/2;
		(pq[index/2])->pqindex = index;

		tmpbp = pq[index/2];
		pq[index/2] = pq[index];
		pq[index] = tmpbp;
		index = index/2;
	}
}

/* Reorder a block according to new value in the Priority Queue */
/* dir 0 : upheap, 1 : downheap */
void reorder(int dir, struct buffer *bp)
{
	if (dir == 0) {
		upheap(bp->pqindex);
	}
	else {
		downheap(bp->pqindex);
	}
}

