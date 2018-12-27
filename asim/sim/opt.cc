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

#include "arc.h"
#include "opt.h"
#include "util.h"
#include "fs.h"
#include "prefetch.h"
#define MAX_OPT_ENTS 90000000
#define OLDMETHOD
#undef OLDMETHOD
// -------------------- util functions ---------------------------------
#include <stdlib.h>

/********************************************************/
/* member functions: misc.				*/
/********************************************************/
void HASH_INIT_BLOCKTAB(int tablesize, LTAddrHash *tb)
{
    int i;
    tb->table = (page_struct **) calloc(tablesize, sizeof(page_struct *));
    for (i=0; i<tablesize; i++)
	tb->table[i] = NULL;
    
    tb->size = tablesize;
    tb->free = NULL;
}

void HASH_ALLOC(int num, LTAddrHash *table)
{ 
    int i; 
    page_struct *tp;
    tp = (page_struct *) calloc(num, sizeof(page_struct));
    for (i=0; i<num; i++)
    {
	tp->hashnext = table->free;
	table->free = tp;
	tp = tp + 1;
    }
}

page_struct *HASH_FIND_BLOCK(unsigned inode, unsigned block,  LTAddrHash *table)
{
    page_struct * HP = table->table[HASH_FUNC(inode,block) % table->size];
    while (HP != NULL)
    {
	if (HP->inode == inode && HP->block == block)
	    break;
	HP = HP->hashnext;
    }
    return HP;
}

page_struct *HASH_INSERT_BLOCK(unsigned inode, unsigned block, LTAddrHash *table)
{
    if (table->free == NULL)
	HASH_ALLOC(256,table);
    
    page_struct *HP = table->free;
    table->free = HP->hashnext;
    HP->hashnext = table->table[HASH_FUNC(inode,block) % table->size];
    table->table[HASH_FUNC(inode,block) % table->size] = HP;
    HP->inode=inode;
    HP->block=block;

    HP->isResident = 0; 
    HP->ref_times=0;
    HP->pf_times=0; 
    HP->fwd_distance=MAXINT;
    HP->lastref=0;
    HP->nextref=MAXINT;

    HP->OPT_next=NULL;
    HP->OPT_prev=NULL;


    HP->isHIR_block = 1;

    HP->LIRS_next = NULL;
    HP->LIRS_prev = NULL;

    HP->HIR_rsd_next = NULL;
    HP->HIR_rsd_prev = NULL;
       
    HP->recency = S_STACK_OUT;

    //    HP->left = HP->right = HP->parent = NULL;

   return HP;
}
// ---------------------------------------- end of util functions ------
LTAddrHash Block_tab;
LTAddrHash *bt = &Block_tab;

unsigned free_mem_size = 128, mem_size = 128;
long vm_size, ref_trc_len;

page_struct * LRU_list_head = NULL;
page_struct * LRU_list_tail = NULL;

page_struct * HG_list_head;
page_struct * HG_list_tail;

page_struct * OPT_list_head;
page_struct * OPT_list_tail;

page_struct * LG_LRU_page_ptr;


page_struct * HIR_list_head = NULL;
page_struct * HIR_list_tail = NULL;

page_struct * LIR_LRU_block_ptr = NULL; /* LIR block  with Rmax recency */

unsigned refs = 0;
unsigned no_dup_refs=0; /* counter excluding duplicate refs */
unsigned num_pg_flt = 0;
unsigned cur_lir_S_len = 0;
unsigned vmPages = 0; //number of unique pages references
unsigned num_LIR_pgs = 0;
unsigned *ref_inode = NULL;
unsigned *ref_block = NULL;
unsigned *fdist = 0;
unsigned *rdist = 0;
unsigned ref_index = 0;
//unsigned miss = 0, hit = 0;

// ------------------------------------------ heap functions -----------
//OPT : Accesses 158667 Hit 138371 Miss 20296 Ratio 87
//        Total:158667 hr:87.21% hr2:87.21% prefetchHits:0
//        Unused prefetches: 0 Number of ios: 11573  
heap_t *PQ = NULL;
unsigned heap_used = 0;

int hParent(int i) 
{
  return i/2;
}

int hLeft(int i)
{
  return 2*i;
}

int hRight(int i) 
{
  return 2*i+1;
}

void init_heap(void)
{
    PQ = (heap_t *)calloc(HEAPSIZE+1, sizeof(heap_t));
    assert(PQ);
    //  heap_used = 1; // we dont use the zeroth elementh here
}

void hExchange( heap_t *a, heap_t *b)
{
  heap_t dummy;
  int indexA,indexB;

#if 0
  if (a->key != MAXINT-a->page->HIST[1]) {
    printf("problem1a (%u %u)\n",a->page->HIST[1],MAXINT - a->key);
    exit(0);
  }
  if (b->key != MAXINT-b->page->HIST[1]) {
    printf("problem2a (%u %u)\n",b->page->HIST[1],MAXINT-b->key);
    exit(0);
  }
#endif

  dummy.key = a->key;
  dummy.time = a->time;
  dummy.page = a->page;
  indexA = a->page->heap_index;
  indexB = b->page->heap_index;

  assert(indexA);
  assert(indexB);

  a->key =  b->key;
  a->time = b->time;
  a->page =  b->page;
  a->page->heap_index = indexA; //b->page->heap_index;

  b->key = dummy.key;
  b->time = dummy.time;
  b->page = dummy.page;
  b->page->heap_index = indexB;

#if 0
  if (a->key != MAXINT-a->page->HIST[1]) {
    printf("problem1 (%u %u)\n",MAXINT-a->page->HIST[1],a->key);
    exit(0);
  }
  if (b->key != MAXINT-b->page->HIST[1]) {
    printf("problem2 (%u %u)\n",b->page->HIST[1],MAXINT-b->key);
    exit(0);
  }
#endif
}

void insert_heap(page_struct *page, unsigned key)
{
  heap_t *entry;
  int i, p;
  static unsigned hTime = 0;
  if (heap_used == HEAPSIZE) {
    printf("We have a problem with heap\n");
    exit(0);
  }
  heap_used++; // we dont use the zeroth element
  //  if (scheme != LRU2) 
    hTime ++;
  entry = &PQ[heap_used];
  i = heap_used;

  entry->key = key; //page->fwd_distance;
  entry->time = hTime;
  entry->page = page;
  page->heap_index = i;
  
  //  if (entry->key > MAXINT-10) {
  //  printf("-->%u:::",entry->key);
  //  }
  //key = entry->key;

  // no problem with time here
  while (i > 1 && PQ[(p = hParent(i))].key < key) {
    // exchange PQ[i] with PQ[hParent[i]]
    //    printf("%d ",p);
    hExchange(&PQ[i],&PQ[p]);
 
    i = p;
    //    if (i == 2) {
    //    printf("(%u %u)",PQ[(p = hParent(i))].key,entry->key);
    //    }
  }
  //  printf("::::");
  

#if 0
    for (int k = 1; k<=(heap_used<8?heap_used:8); k++) {
      printf("%u:%u ",PQ[k].page->heap_index,PQ[k].key);
    }
    printf("\n");
#endif


#if 0
    printf("-->%u %u %d\n",MAXINT - PQ[page->heap_index].key, 
	   page->HIST[1],
	   page == PQ[page->heap_index].page
	   );

    for (unsigned k = 2; k<=heap_used; k++) {
      if (PQ[k].key > PQ[hParent(k)].key) {
	printf("Problem: %d:%u %d:%u\n ",
	       k,PQ[k].key,hParent(k),PQ[hParent(k)].key);
      }
      
      if (PQ[k].key != MAXINT-PQ[k].page->HIST[1]) {
	printf("%d %d problem (%u %u)\n",hTime,k, 
	       MAXINT - PQ[k].key, PQ[k].page->HIST[1]);
	exit(0);
      }
    }
#endif


#if 0
  for (i = 1; i<=heap_used; i++) {
    //    printf("%u:%u ",PQ[i].page->heap_index,PQ[i].key);
    if (PQ[PQ[i].page->heap_index].page != PQ[i].page) {
      //      for (int k = 1; k<=heap_used; k++) {
      //      	printf("%u:%u ",PQ[k].page->heap_index,PQ[k].key);
      //      }
      //      printf("\n");

      printf("insertion problemX %d (%d %d)\n",
	     heap_used,i,PQ[i].page->heap_index
	     //,PQ[PQ[i].page->heap_index].page,PQ[i].page  
	     );

      exit(0);
    }
  }
  //  printf("\n");
#endif


  //  return;
#ifdef OLDMETHOD
  // find the place for this guy to go to 
  
  page_struct *temp_OPT_ptr;
  temp_OPT_ptr = OPT_list_head;
  // now also keep the page on a sorted list by page->fwd_distance
  while (temp_OPT_ptr != NULL && page->fwd_distance > temp_OPT_ptr->fwd_distance)
    temp_OPT_ptr = temp_OPT_ptr->OPT_next;
  if (!temp_OPT_ptr)
    {  
      if (OPT_list_tail)
	{
	  OPT_list_tail->OPT_next = page; 
	  page->OPT_prev = OPT_list_tail;
	  page->OPT_next = NULL;
	  OPT_list_tail = OPT_list_tail->OPT_next;
	}
      else
	{
	  OPT_list_head = OPT_list_tail = page;
	  OPT_list_tail->OPT_prev = OPT_list_tail->OPT_next = NULL;
	}
    }
  else 
    {  /* place just before "*temp_OPT_ptr" */
      page->OPT_prev = temp_OPT_ptr->OPT_prev; 
      page->OPT_next = temp_OPT_ptr; 
      
      
      if (!temp_OPT_ptr->OPT_prev)	
	OPT_list_head = page; 
      else
	temp_OPT_ptr->OPT_prev->OPT_next = page;
      
      temp_OPT_ptr->OPT_prev = page;     
    }
  
  // end 
#endif
}

void fix_heap(unsigned i)
{
  unsigned l = hLeft(i);
  unsigned r = hRight(i);
  unsigned largest;
  //  printf("insertion\n");
  if (l <= heap_used 
      && (PQ[l].key > PQ[i].key
	  || ( PQ[l].key == PQ[i].key && PQ[l].time < PQ[i].time)))
    largest = l;
  else
    largest = i;

  if (r <= heap_used 
      && (PQ[r].key > PQ[largest].key
      || ( PQ[r].key == PQ[largest].key && PQ[r].time < PQ[largest].time)))
    largest = r;
  
  if (largest != i) {
    //        printf("fixing \n");
    hExchange(&PQ[i], &PQ[largest]);
    
    fix_heap(largest);
  }
}

void remove_heap(page_struct *page)
{
  int p,i = page->heap_index;
  unsigned key;
  //    printf("remove \n");
  if (!i) {
    fprintf(stderr,"removal on something not on heap\n");
    return;
  }

  hExchange(&PQ[i],&PQ[heap_used]);
  heap_used--;
  page->heap_index = 0;

  //  printf("fix \n");
  fix_heap(i);

  key = PQ[i].key;  
  while (i > 1 
	 && (PQ[(p = hParent(i))].key < key
	     //	     || (PQ[p].key == key && PQ[p].time < PQ[i].time) ) 
	 ))  {
    //        printf("up \n");
    hExchange(&PQ[i],&PQ[p]);
    i = p;
  }



#if 0
    for (int k = 2; k<=heap_used; k++) {
      if (PQ[k].key > PQ[hParent(k)].key) {
	printf("Problem: %d %d:%u %d:%u\n ",i,
	       k,PQ[k].key,hParent(k),PQ[hParent(k)].key);
	exit(0);
      }
    }
#endif




#ifdef OLDMETHOD  
  if (page->OPT_prev)
    page->OPT_prev->OPT_next = page->OPT_next;
  else 
    OPT_list_head = page->OPT_next; 

  if (page->OPT_next)
    page->OPT_next->OPT_prev = page->OPT_prev;
  else
    { /* hit at the queue tail */
      OPT_list_tail = page->OPT_prev;
      if (OPT_list_tail)
	OPT_list_tail->OPT_next = NULL;
    }
#endif
}


page_struct * heap_max(void)
{
  struct page_struct * page = NULL;
#if 0
  for (int k = 1; k<=heap_used; k++) {
    printf("%u:%u ",PQ[k].page->heap_index,PQ[k].key);
  }
  printf("\n");
#endif

  if (heap_used) {
    page = PQ[1].page;
    assert(page->heap_index);

    hExchange(&PQ[1],&PQ[heap_used]);
    heap_used--;
    fix_heap(1);
    
    page->heap_index = 0;
    //      printf("max \n");

  }

#ifndef OLDMETHOD
  if (page) {
    page->isResident = 0;
    page->OPT_prev = NULL;
    page->OPT_next = NULL;
  }
#endif

  page_struct *temp_OPT_ptr = page;
#ifdef OLDMETHOD    

  OPT_list_tail->isResident = 0;  
  temp_OPT_ptr = OPT_list_tail;
  
  OPT_list_tail = OPT_list_tail->OPT_prev;
  OPT_list_tail->OPT_next = NULL;
	  
  temp_OPT_ptr->OPT_prev = NULL;
  temp_OPT_ptr->OPT_next = NULL;
#endif
  if (page !=  temp_OPT_ptr) {
    printf("mismatch between two techniques %d (%u %u)\n",heap_used,
	   page->fwd_distance
	   ,temp_OPT_ptr->fwd_distance);

    exit(0);
  }
  return page;
}


// ------------------------------------end of heap functions -----------


void initOPT()
{
    HASH_INIT_BLOCKTAB(HASH_FACTOR_BLOCKTAB, bt);
    HASH_ALLOC(1024, bt);
    
    free_mem_size = mem_size = cachesize;

   
    ref_inode = (unsigned *)calloc(MAX_OPT_ENTS, sizeof(unsigned));
    ref_block = (unsigned *)calloc(MAX_OPT_ENTS, sizeof(unsigned));
    fdist = (unsigned *)calloc(MAX_OPT_ENTS, sizeof(unsigned));
    rdist = (unsigned *)calloc(MAX_OPT_ENTS, sizeof(unsigned));
    assert(ref_inode && ref_block && fdist && rdist);

    for(int i=0; i<MAX_OPT_ENTS; i++) {
    	fdist[i]=MAXINT;
    	rdist[i]=MAXINT;
    }
    
    init_heap();
    //printf(" Lhirs (cache size for HIR blocks) = %d\n", HIR_block_portion_limit);
    
}

void reportOPT()
{
    printf("OPT : Accesses %d Hit %d Miss %d Ratio %d\n", refs, hit, miss, (100*hit)/(refs?refs:1));

}
  
page_struct * put_page_in(unsigned inode, unsigned block)
{
  page_struct *page;

  ref_inode[ref_index]=inode;
  ref_block[ref_index]=block;
  
  page = HASH_FIND_BLOCK(inode, block, bt);
  if(page == NULL)// new block
    {
      page =  HASH_INSERT_BLOCK(inode, block, bt);
      page->vblock_num = vmPages;
      vmPages++;
    }
  else
    {
      //use last access to update fwd distance
      fdist[page->lastref] = ref_index;
      rdist[ref_index] = page->lastref;
    }

  if (page->nextref == MAXINT) 
    page->nextref = ref_index;
  
  //build an array of references
  page->lastref = ref_index;
  
  ref_index++;
  if (ref_index > MAX_OPT_ENTS)
    {
      printf("Overflow.\n");
      exit(1) ;
    }
  
  return page;
}

void cacheAccessX(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc, int filD)
{	
  //    page_struct *page;
    unsigned blockstart, blockend, block,offset,b2;
    //find if exists
    position = transform(position,-1,&size);
    if (position == MAXINT)
      return;

    if(inode == 0)
	printf("Inode == 0\n");
    
    if(size == 0)
    	return;

    blockstart = position / BLOCKSIZE;
    //    blockend = (position + size )/ BLOCKSIZE+1;
    blockend = (position + size - 1)/ BLOCKSIZE;


    //    offset = position % BLOCKSIZE;
    //    blockend = ((offset + size)/BLOCKSIZE);
    //    blockend += blockstart;

    //    if (b2>blockend)
    //      printf("%d %d\n",b2,blockend);

    //    fprintf(stderr,"PP %d: %d %d\n",ref_index,blockstart,blockend);
    for(block = blockstart; block <= blockend; block++) 
    {
      //      printf("%d: %d %d\n",ref_index,inode,block);
      //      printf("E %d: %d %d\n",ref_index, block, blockend+1);
      put_page_in(inode,block);
    }
}

unsigned cur_ref = 0;

void do_OPT(int filD, unsigned inode, unsigned block, int type, int rok)
{
  //  int k = cur_ref;
  //  printf("A %d: %d %d\n",k, ref_inode[k], ref_block[k]);
  //  printf("A %d: %d %d\n",k, FileTable[filD].f_cb,FileTable[filD].f_needed );
  perform_io(filD,inode,block,type,rok);
  //  cur_ref++;
}

void OPTref( unsigned inode, unsigned block, int prefetch)
{
  page_struct *page;
  page_struct *temp_OPT_ptr;
  //  int badder = 0;

#if 0
  if (!prefetch) {
    printf("N %d: %d %d\n",cur_ref,inode,block);
  } 
  //  else
  //    printf("P %d: %d %d\n",cur_ref,inode,block);
#endif
  // lets do the opt stuff here
  if (!prefetch)
    if (inode != ref_inode[cur_ref] || 
	block != ref_block[cur_ref]) {
      printf("Trace read mismatch in do_OPT\n");
      //      int k = cur_ref-10;
      //      for (k=k<0?0:k;k<cur_ref+2;k++)
      //      	printf("O %d: %d %d\n",k, ref_inode[k], ref_block[k]);
      //      printf("M %d: %d %d\n",cur_ref, inode, block);
      exit(0);
    }

  page = HASH_FIND_BLOCK(inode,block, bt);       
  if (prefetch) {
    // this is a prefetched block, and may not be in cache
    if (!page) {
      badPF++;
      //      badder = 1;
      // lets add it .. we know its never going to be accessed again
      page = put_page_in(inode,block);
      //      page->lastref = MAXINT;
    }
  }

  if (!prefetch) {
    page->ref_times++;
    refs++;
  }

  if (!page->isResident)
    {  /* page fault */
      if (!prefetch) miss++;
      
      if (free_mem_size == 0)
	{             /* free the LRU page */
	  temp_OPT_ptr = heap_max();
#ifdef PRINTT
	  fprintf(stderr,"K: %u \n",temp_OPT_ptr->block); 
#endif	  
	  free_mem_size++;
	}
      page->isResident = 1+prefetch;
      free_mem_size--;
    }
  else 
    {                             /* hit in memroy */
      if (page->isResident == 2) {
	page->isResident = 1;
	
	if (check_group_ready(inode,block)) {
	  prefetch_hits++;
	  hit++;
	} else {
	  miss++;
	}
      } else {
	hit++;
      }
      remove_heap(page);
    }
  
  if (prefetch) {
    // we have to scan the list and see when we are accessed next
    page->fwd_distance = page->nextref;
  } else {
    page->fwd_distance = fdist[cur_ref];
    page->nextref = fdist[cur_ref];
  }
  insert_heap(page,page->fwd_distance);

  if (!prefetch)
    cur_ref++;

}

int OPTCHECK (unsigned inode, unsigned block)
{
  page_struct *page = HASH_FIND_BLOCK(inode,block, bt); 
  
  if (page && page->isResident)
    return  1;

  //  if (!page) {
  //    put_in_list(inode,block);
  //  }

  return 0;

}
