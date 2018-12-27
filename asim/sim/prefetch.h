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

#ifndef _prefetch_h
#define _prefetch_h 1 

#include <assert.h>
#include "disk.h"

#define BLOCKSIZE 4096

#define HEAPSIZE 2*cachesize+1

void prefetch_init(void);
void perform_io(int filD, unsigned inode, unsigned block, int w, int rok);
void report_ios(void);
void queue_io(unsigned block, unsigned inode, int async, int type);
void ll_iorequest(int async);
void accessCache(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc, int filD);
void report(void);
int check_group_ready(unsigned inode, unsigned block);
int check_group_ready2(unsigned inode, unsigned block);
void clear_all_pending_ios(void);
void add_to_pending_io(int *newR,unsigned inode,unsigned block);
//unsigned transform(unsigned pos, int filD);
unsigned transform(unsigned pos, int filD,unsigned *s);

void initLRFU(void);
void do_LRFU(int filD, unsigned inode, unsigned block, int type, int rok);
void reportLRFU(void);
void LRFUref(unsigned blkno, unsigned inode, int prefetch);
int LRFUCHECK(unsigned block, unsigned inode);

extern unsigned prefetch_hits;
extern unsigned MAXRA;
extern unsigned MINRA;


extern int scheme;
extern unsigned cachesize;
extern unsigned badPF;
extern int noprefetch;
extern unsigned dummyTrack;
extern unsigned	hit,miss;

extern double lambda;
extern int corperiod;
extern int history;

#define SYNTRACE 1 // for creating the synthetic trace for example scenario
#undef SYNTRACE

#ifdef SYNTRACE
#define RCHUNK 8 // arb blocks per chunk
#else
#define RCHUNK 32 // arb blocks per chunk
#endif

#define NOTTHERE -1

#define PRINTT 1
#undef PRINTT 

#define ARC 1
#define OPT 2
#define LRU 3
#define LIRS 4
#define LRFU 5
#define MQ 6
#define LRU2 7
#define TQ 8


#define PREFETCHQ	14	/* partition to hold prefetch references*/
#define DEMAND 77
#define PREFETCH 78

void compleInCache(unsigned inode, unsigned startBlock, unsigned size);


#endif
