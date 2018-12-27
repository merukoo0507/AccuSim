/* lirs.c 
 *  
 * See Sigmetrics'02 paper "`LIRS: An Efficient Low Inter-reference 
 * Recency Set Replacement Policy to Improve Buffer Cache Performance"
 * for more description. "The paper" is used to refer to this paper in the 
 * following.
 *  
 * This program is written by Song Jiang (sjiang@cs.wm.edu) Nov 15, 2002
 */

/* Input File: 
 *              trace file(.trc)
 *              parameter file(.par)
 *
 * Output File: 
 *              hit rate file(.cuv): it describes the hit rates for each 
 *              cache size specified in parameter file. Gnuplot can use it 
 *              to draw hit rate curve. 
 *
 *              stack size file (.sln): It is used to produce LIRS 
 *              stack size variance figures for a specific cache size like 
 *              Fig.5 in the paper. Be noted that only the results for the
 *              last cache size are recorded in the file if multiple cache 
 *              sizes are specified in the parameter file.  
 */

/* Input File Format: 
 * (1) trace file: the (UBN) Unique Block Number of each reference, which
 *     is the unique number for each accessed block. It is strongly recommended
 *     that all blocks are mapped into 0 ... N-1 (or 1 ... N) if the total  
 *     access blocks is N. For example, if the accessed block numbers are:
 *     52312, 13456, 52312, 13456, 72345, then N = 3, and what appears in the 
 *     trace file is 0 1 0 1 2 (or 1 2 1 2 3). You can write a program using 
 *     hash table to do the trace conversion, or modify the program. 
 * (2) parameter file: 
 *      one or more cache sizes you want to test 
 *     
 */

/* Command Line Uasge: only prefix of trace file is required. e.g.
   :/ lirs ABC
   It is implied that trace file is "ABC.trc", parameter file is "ABC.par"
   output files are "ABC_LIRS.cuv" and "ABC_LIRS.sln"
*/

/* BE NOTED: If you want to place a limit on LIRS stack, or want to test
 *           hit rates for warm cache, go to lirs.h to change corresponding
 *           parameters.
 */

#include "opt.h"
#include "util.h"
#include "prefetch.h"
unsigned HIR_block_portion_limit, HIR_block_activate_limit;
unsigned MAX_S_LEN;


//LTAddrHash Block_tab;
//LTAddrHash *bt = &Block_tab;
//unsigned free_mem_size = 128, mem_size = 128;
//page_struct * LRU_list_head = NULL;
//page_struct * LRU_list_tail = NULL;
//page_struct * HIR_list_head = NULL;
//page_struct * HIR_list_tail = NULL;
//page_struct * LIR_LRU_block_ptr = NULL; /* LIR block  with Rmax recency */

unsigned total_pg_refs = 0, warm_pg_refs=0;
//unsigned no_dup_refs=0; /* counter excluding duplicate refs */
//unsigned num_pg_flt = 0;
//unsigned cur_lir_S_len = 0;
//unsigned vmPages = 0; //number of unique pages references
//unsigned num_LIR_pgs = 0;

void initLirs()
{
     /* the memory ratio for hirs is 1% */
    HIR_block_portion_limit = (unsigned long)(HIR_RATE/100.0*mem_size); 
    if (HIR_block_portion_limit < LOWEST_HG_NUM)
	HIR_block_portion_limit = LOWEST_HG_NUM;


    MAX_S_LEN = (mem_size*2);
    
    //    fprintf(stderr," Lhirs (cache size for HIR blocks) = %d %u\n", HIR_block_portion_limit,mem_size);
}

#if 1
void reportLirs()
{
  //    printf("total blocks refs = %d  number of misses = %d \nhit rate = %2.2f  mem shortage ratio = %2.1f \n", total_pg_refs, num_pg_flt, (1-(float)num_pg_flt/warm_pg_refs)*100, (float)mem_size/vmPages*100);
    
    printf("LIRS : Accesses %d Hit %d Miss %d Ratio %d\n",total_pg_refs, hit, miss, (100*hit)/(total_pg_refs?total_pg_refs:1));
}
#endif
  

void do_LIRS(int filD, unsigned inode, unsigned block, int type, int rok)
{
  perform_io(filD,inode,block,type,rok);
  //  cur_ref++;
  //  LIRS_Repl(inode,block);
}

void LIRSref(unsigned inode, unsigned ref_block,int prefetch)
{
    page_struct *page;
    
    if (!prefetch) {
      total_pg_refs++;
      warm_pg_refs++;
    }
    page = HASH_FIND_BLOCK(inode, ref_block, bt);
    if(page == NULL)// new block
    {
	page = HASH_INSERT_BLOCK(inode, ref_block, bt);
	page->vblock_num = vmPages;
	vmPages++;
    }
    
    
    if (!page->isResident)
    {  /* block miss */
      if (!prefetch) {
	num_pg_flt++;
	miss++;
      }
      if (free_mem_size == 0)
	{ 
	  /* remove the "front" of the HIR resident page from cache (queue Q), 
	     but not from LIRS stack S 
	  */ 
	  /* actually Q is an LRU stack, "front" is the bottom of the stack,
	     "end" is its top
	  */
	  HIR_list_tail->isResident = FALSE;
	  remove_HIR_list(HIR_list_tail);
	  free_mem_size++;
	}
      else if (free_mem_size > HIR_block_portion_limit)
	{
	  if (!prefetch) { // arb for np pages only
	    page->isHIR_block = FALSE;
	    num_LIR_pgs++;
	  }
	}
      free_mem_size--;
    } 
    /* hit in the cache */
    else { 
      if (page->isResident == 2) {
	page->isResident = 1;
	if (check_group_ready(inode,ref_block)) {
	  prefetch_hits++;
	  hit++;
	} else {
	  miss++;
	}
      } else {
	hit++;
      }
      if (page->isHIR_block) 
	remove_HIR_list(page);
    }

    if (!prefetch) {
      remove_LIRS_list(page);
      /* place newly referenced page at head */
      add_LRU_list_head(page);
    }
    page->isResident = TRUE+prefetch; // arb to mark a prefetched page
    
    if (!prefetch) {
      if (page->recency == S_STACK_OUT)
	cur_lir_S_len++;
    }
	
    if (!prefetch && page->isHIR_block && (page->recency == S_STACK_IN))
    {   
	page->isHIR_block = FALSE;
	num_LIR_pgs++; 

	if (num_LIR_pgs > mem_size-HIR_block_portion_limit)
	{
	    add_HIR_list_head(LIR_LRU_block_ptr);
	    HIR_list_head->isHIR_block = TRUE;
	    HIR_list_head->recency = S_STACK_OUT;
	    num_LIR_pgs--; 
	    LIR_LRU_block_ptr = find_last_LIR_LRU();
	}
	//	else 
	//	    printf("Warning2!\n");
    }
    else if (page->isHIR_block)
	add_HIR_list_head(page); 
    
    if (!prefetch) {
      page->recency = S_STACK_IN;
      prune_LIRS_stack();
    }
    
    /*  To reduce the *.sln file size, ratios of stack size are 
     *  recorded every 10 references */  
    
    return;
}


/* remove a block from memory */ 
int remove_LIRS_list(page_struct *page_ptr)
{ 
    if (!page_ptr)
	return FALSE;
    
    if (!page_ptr->LIRS_prev && !page_ptr->LIRS_next)
	return TRUE;
    
    if (page_ptr == LIR_LRU_block_ptr)
    {
	LIR_LRU_block_ptr = page_ptr->LIRS_prev;
	LIR_LRU_block_ptr = find_last_LIR_LRU();
    }
    
    if (!page_ptr->LIRS_prev)
	LRU_list_head = page_ptr->LIRS_next;
    else     
	page_ptr->LIRS_prev->LIRS_next = page_ptr->LIRS_next;
    
    if (!page_ptr->LIRS_next)
	LRU_list_tail = page_ptr->LIRS_prev; 
    else
	page_ptr->LIRS_next->LIRS_prev = page_ptr->LIRS_prev;
    
    page_ptr->LIRS_prev = page_ptr->LIRS_next = NULL;
    return TRUE;
}

/* remove a block from its teh front of HIR resident list */
int remove_HIR_list(page_struct *HIR_block_ptr)
{
    if (!HIR_block_ptr)
	return FALSE;
    
    if (!HIR_block_ptr->HIR_rsd_prev)
	HIR_list_head = HIR_block_ptr->HIR_rsd_next;
    else 
	HIR_block_ptr->HIR_rsd_prev->HIR_rsd_next = HIR_block_ptr->HIR_rsd_next;
    
    if (!HIR_block_ptr->HIR_rsd_next)
	HIR_list_tail = HIR_block_ptr->HIR_rsd_prev; 
    else
	HIR_block_ptr->HIR_rsd_next->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_prev;

    HIR_block_ptr->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_next = NULL;

    return TRUE;
}

page_struct *find_last_LIR_LRU()
{
    
    if (!LIR_LRU_block_ptr)
    {
	printf("Warning*\n");
	exit(1);
    }
    
    while (LIR_LRU_block_ptr->isHIR_block == TRUE)
    {
	LIR_LRU_block_ptr->recency = S_STACK_OUT;
	cur_lir_S_len--;
	LIR_LRU_block_ptr = LIR_LRU_block_ptr->LIRS_prev;
    }    
 
    return LIR_LRU_block_ptr;
}

page_struct *prune_LIRS_stack()
{
    page_struct * tmp_ptr;
    //    fprintf(stderr,"doing pruning %u %u \n",cur_lir_S_len,MAX_S_LEN);
    if (cur_lir_S_len <=  MAX_S_LEN)
	return NULL;
    //    fprintf(stderr,"doing pruning \n");
    tmp_ptr = LIR_LRU_block_ptr;
    while (tmp_ptr->isHIR_block == 0)
	tmp_ptr = tmp_ptr->LIRS_prev;

    tmp_ptr->recency = S_STACK_OUT;
    remove_LIRS_list(tmp_ptr);
    insert_LRU_list(tmp_ptr, LIR_LRU_block_ptr);
    cur_lir_S_len--;
    
    return tmp_ptr;
}


/* put a HIR resident block on the end of HIR resident list */ 
void add_HIR_list_head(page_struct * new_rsd_HIR_ptr)
{
    new_rsd_HIR_ptr->HIR_rsd_next = HIR_list_head;
    if (!HIR_list_head)
	HIR_list_tail = HIR_list_head = new_rsd_HIR_ptr;
    else
	HIR_list_head->HIR_rsd_prev = new_rsd_HIR_ptr;
    HIR_list_head = new_rsd_HIR_ptr;
    
    return;
}

/* put a newly referenced block on the top of LIRS stack */ 
void add_LRU_list_head(page_struct *new_ref_ptr)
{
  new_ref_ptr->LIRS_next = LRU_list_head; 
  
  if (!LRU_list_head)
  {
      LRU_list_head = LRU_list_tail = new_ref_ptr;
      LIR_LRU_block_ptr = LRU_list_tail; /* since now the point to lir page with Smax isn't nil */ 
  } 
  else
  {
      LRU_list_head->LIRS_prev = new_ref_ptr;
      LRU_list_head = new_ref_ptr;
  }
  
  return;
}

/* insert a block in LIRS list */ 
void insert_LRU_list(page_struct *old_ref_ptr, page_struct *new_ref_ptr)
{
    old_ref_ptr->LIRS_next = new_ref_ptr->LIRS_next;
    old_ref_ptr->LIRS_prev = new_ref_ptr;
    
    if (new_ref_ptr->LIRS_next)
	new_ref_ptr->LIRS_next->LIRS_prev = old_ref_ptr;
    new_ref_ptr->LIRS_next = old_ref_ptr;
    
    return;
}

