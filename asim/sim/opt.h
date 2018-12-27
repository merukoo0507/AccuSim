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

#ifndef _opt_h_
#define _opt_h_ 1


#include "stdio.h"
#include "string.h"
#include "stdlib.h"


/********************** Two Parameters You may Want to Change ************/
/* if you want to limit the maximum LIRS stack size (e.g. 3 times of LRU stack  *  size, you can change the "2000" to "3"
 */  
#define MAX_S_LEN_FACTOR 2 // was 2500

/* the size percentage of HIR blocks, default value is 1% of cache size */
#define HIR_RATE 10.0 // was 1.0
 

/* This specifies from what virtual time (reference event), the counter for 
 * block miss starts to collect. You can test a warm cache by changin the "0"
 * to some virtual time you desire.*/
#define STAT_START_POINT 0

#define LOWEST_HG_NUM 32 // was 2
/**************************************************************************/




#include "arc.h"

#define TRUE 1
#define FALSE 0

/* used to mark comparison of recency and Smax */
#define S_STACK_IN 1
#define S_STACK_OUT 0

void OPT_Repl();
void reportOPT();
void initOPT();
void initLirs();
void reportLirs(void);
//void cacheAccess(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc);

void do_OPT(int filD, unsigned inode, unsigned block, int type, int rok);
void OPTref( unsigned inode, unsigned block, int prefetch);
void cacheAccessX(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc,int filD);
int OPTCHECK (unsigned inode, unsigned block);

void do_LIRS(int filD, unsigned inode, unsigned block, int type, int rok);
void LIRSref(unsigned inode, unsigned ref_block,int prefetch);

page_struct *find_last_LIR_LRU();
void add_HIR_list_head(page_struct * new_rsd_HIR_ptr);
void add_LRU_list_head(page_struct *new_ref_ptr);
void insert_LRU_list(page_struct *old_ref_ptr, page_struct *new_ref_ptr);
page_struct *prune_LIRS_stack();
void LIRS_Repl(unsigned inode, unsigned ref_block);
int remove_LIRS_list(page_struct *page_ptr);
int remove_HIR_list(page_struct *HIR_block_ptr);

struct LTAddrHash
{
    page_struct **table;
    int  size;
    page_struct *free;
};

extern LTAddrHash *bt;
extern unsigned free_mem_size,mem_size;
extern page_struct * LRU_list_head;
extern page_struct * LRU_list_tail;
extern page_struct * HIR_list_head;
extern page_struct * HIR_list_tail;
extern page_struct * LIR_LRU_block_ptr; /* LIR block  with Rmax recency */

extern unsigned no_dup_refs, num_pg_flt, cur_lir_S_len, vmPages, num_LIR_pgs;


typedef struct heap_f {
  unsigned key;
  unsigned time;
  struct page_struct *page;
} heap_t;
extern heap_t *PQ;
extern unsigned heap_used;
void insert_heap(page_struct *page, unsigned key);
void remove_heap(page_struct *page);
page_struct * heap_max(void);

//void insert_heap2(page_struct *page, unsigned key);
//void remove_heap2(page_struct *page);
//page_struct * heap_min(void);


#endif


// Lhirs (cache size for HIR blocks) = 5
//total blocks refs = 158667  number of misses = 31782 
//hit rate = 80.0  mem shortage ratio = 4.9 
//  512  80.0
