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

#ifndef _arc_h_
#define _arc_h_ 1

#define T1 0
#define T2 2
#define B1 1
#define B2 3


#define	MAXBUF		512000	/* max (real+ghost) buffer cache size */
#define	BLKHASHBUCKET	23617	/* hash bucket size */
				/* We use a hashing function to find a buffer with 
				   a given block numbe */
#if !defined( min )
# define min(a,b) (a<b?a:b)
#endif

#if !defined( max )
# define max(a,b) (a>b?a:b)
#endif

#define __K 2 // arb for LRU-2

struct CDB
{
    unsigned inode; /* page's ID number */
    unsigned block; /* page's ID number */
    int ARC_where; /* not used for LRU */
  
  int prefetch; // arb -- 1 if prefetched new


    struct CDB *lrunext; /* for doubly linked list */
    struct CDB *lruprev; /* for doubly linked list */
    struct CDB *hashnext, *hashprev; /* for hashing */
  struct CDB *freenext;


  int where; /* not used for LRU */
  unsigned expireTime;
  unsigned reference;
  
  unsigned last;
  unsigned HIST[__K];
  int histValid;
  int isResident;


  // from page
  unsigned vblock_num;
  unsigned ref_times;
  unsigned pf_times; 
  unsigned fwd_distance;
  unsigned lastref;
  unsigned nextref;
  int isHIR_block;
  struct CDB * OPT_next;
  struct CDB * OPT_prev;
  struct CDB * LIRS_next;
  struct CDB * LIRS_prev;
  struct CDB * HIR_rsd_next;
  struct CDB * HIR_rsd_prev;
  unsigned    recency;
  int heap_index;
  //  struct page_struct *left,*right,*parent;  
  
  int done; // to show if a page is ready 

};

#define page_struct CDB

CDB *lru_remove(unsigned arcList); //grab LRU from T1
void mru_insert(CDB *block, unsigned arcList); //put it on B1
void remove_from_list(CDB *block); //take off whichever list

CDB *locate(unsigned inode, unsigned block); //find the requested page
void free_CDB(CDB *d);
CDB *get_new_CDB(void);

void hashin(CDB *bp);
void hashout(CDB *bp);

int pgref(unsigned inode, unsigned block, int prefetch);

void initARC(void);
void reportARC(void);

extern long T1Length, T2Length, B1Length, B2Length;
extern CDB *freebufs;
extern CDB *blkhash; //hash table
extern unsigned allocBufInCache;
extern unsigned	curVtime;
extern unsigned	accesses;
extern unsigned memUsed;

extern unsigned heap_used;

void do_ARC(int filD, unsigned inode, unsigned block, int type, int rok);

void do_LRU(int filD, unsigned inode, unsigned block, int type, int rok);
void reportLRU(void);
int LRUref(unsigned inode, unsigned block, int prefetch);

void do_LRU2(int filD, unsigned inode, unsigned block, int type, int rok);
void reportLRU2(void);
int LRU2ref(unsigned inode, unsigned block, int prefetch);



#endif
