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
#include "trace.h"
#include "fs.h"
#include "arc.h"
#include "prefetch.h"
#include "opt.h"
#include "mq.h"
#include "util.h"

#define NO_PREFETCH_ON_HIT 1
#undef  NO_PREFETCH_ON_HIT // results were with this option enabled
extern int line;

#define PRINTBLOCKS
#undef PRINTBLOCKS

extern unsigned cur_ref;


LTAddrHash PendingIO_tab;
LTAddrHash *pio = &PendingIO_tab;
page_struct pending_head_tx;
page_struct *pending_head = &pending_head_tx;
unsigned num_pendings = 0;

unsigned num_ios = 0;
unsigned prefetch_hits = 0;
unsigned MAXRA=RCHUNK;
unsigned MINRA=3;
unsigned badPF=0;
unsigned dummyTrack = 0;
int scheme = ARC;

// counters 
unsigned synReq = 0 ;
unsigned synReqSize = 0 ;
unsigned asynReq = 0 ;
unsigned asynReqSize = 0 ;

int noprefetch = 0;

unsigned startBlock = 0;

page_struct *block_info(unsigned inode,unsigned block)
{
  page_struct *page = HASH_FIND_BLOCK(inode,block,pio);
  if (!page) {
    page = HASH_INSERT_BLOCK(inode,block,pio);
    page->isResident = 0;
    page->done = 2;
  } 
  return page;
}


// return values
// 0 not found
// 1 found
// 2 found in ghost caches
inline int checkInCache(int scheme, unsigned inode, unsigned block, 
			int replace)
{
  int retval = 0;
  switch(scheme) {
  default:
  case ARC:
    {
      CDB *cachentry = locate(inode,block);
      if (cachentry) { 
	if (cachentry->ARC_where == T1 || cachentry->ARC_where == T2) {
	  retval = 1; 
	  break;
	}
	// now the entry is in a ghost cache
	if (replace) {
	  if (cachentry->ARC_where == B1)
	    B1Length--;
	  else 
	    B2Length--;
	  remove_from_list(cachentry); // take off whichever list
	  hashout(cachentry);
	  free_CDB(cachentry);
	}

	//  && cachentry->ARC_where != B1 && cachentry->ARC_where != B2)
	retval = 2;
      }
    }
    break;

  case LIRS:
  case OPT:
    {
      retval = OPTCHECK(inode,block);
    }
    break;

  case LRU:
    {
      CDB *cachentry = locate(inode,block);
      if (cachentry) 
	retval = 1; 
      else
	retval = 0;
    }
    break;

  case LRU2:
    {
      CDB *cachentry = locate(inode,block);
      if (cachentry && cachentry->isResident) 
	retval = 1; 
      else
	retval = 0;
    }
    break;

  case LRFU:
    retval = LRFUCHECK(block,inode);
    break;

  case MQ:
    retval = MQCHECK(block,inode);
    break;

  case TQ:
    retval = TQCHECK(block,inode);
    break;

  }
  
  //  printf("%d ",retval);
  return retval;
}

void refit (unsigned inode, unsigned block, int prefetch)
{
  //  if (cur_ref > 21437) {
  //    printf(".");
  //  }
#if 0
  if (prefetch) 
    printf("F: %d %d \n",cur_ref,block);
  else
    printf("S: %d %d \n",cur_ref,block);
#endif

  switch(scheme) {
  default:
  case ARC:
    pgref(inode,block, prefetch);
    break;
    
  case OPT:
    OPTref(inode,block,prefetch);
    break;

  case LRU:
    LRUref(inode,block, prefetch);
    break;
    
  case LIRS:
    LIRSref(inode,block,prefetch);
    break;

  case LRFU:
    LRFUref(block,inode,prefetch);
    break;

  case MQ:
    MQref(inode,block,prefetch);
    break;

  case TQ:
    TQref(inode,block,prefetch);
    break;

  case LRU2:
    LRU2ref(inode,block, prefetch);
    break;


  }

}
int asyncA = 0;
int lastsync = 0; // for last request type

// w = 1 for writes
void perform_io(int filD, unsigned inode, unsigned block, int w, int reada_ok)
{
  FileEntry *f = &FileTable[filD];
  unsigned b = block+1;
  unsigned fileEndBlock = f->fsize / BLOCKSIZE;
  int pVal ;
  
  unsigned long end_index;
  unsigned long index = block;
  unsigned long max_ahead, ahead;
  unsigned long raend;
  unsigned int max_readahead = MAXRA;
  int isitHit = 0;
  int newReq = 0;
  int PageLocked = 1;
  page_struct *page = NULL, *page2=NULL;
  //  rag_t * rag = NULL;

  //  printf("X %d: %d %d \n",cur_ref, FileTable[filD].f_cb,
  //  	 FileTable[filD].f_needed);

  //  printf("readaOK: %d\n",reada_ok);

#ifdef PRINTT
  fprintf(stderr,"A: %u %u\n",inode,block);
#endif
  // access the on demand block
  pVal = checkInCache(scheme,inode,block, 0);
  page = block_info(inode,block); // check it with I/O system
  
  // first page is on-demand always

  if (pVal != 1) {
#ifdef PRINTT
    fprintf(stderr,"R: %u %u \n",inode,block);
#endif
    asyncA = 0;
    //+    queue_io(block,inode,0,w);
    // if we had prefetched this block page->done will be either 0 or 1
    switch (page->done) {
    case 0: // prefetching not complete on this yet 
      //      printf("problem %d\n", page->isResident);
      //      exit(0);
      //+      disksimCheckIssued(inode, block);     // should actually cancel
      //previous one and fall through ... but it works okay for now
       break;
      
    case 1: // was not resident, we need to issue arequest on it 
    case 2: // miss and we havent seen this page 
      page->done = 0;
      //      if (w==TWRITE) // only do this for writes
      //+	disksimInserRequest(DEMAND, inode, block, 1, w);
    }
    queue_io(block,inode,0,w);

  } else {
    if (!page->done) {
      // called on blocks for which i/os are not completed
      disksimCheckIssued(inode, block);     
    }    
    isitHit = 1;    
  }

  if (!isitHit) 
    clear_all_pending_ios();

  //  if (async<0 || (async>=0 && !isitHit))
  //    async = isitHit; // arb to be used by the system for determing I/O kind
      //  else if (!isitHit) 
      //    async = isitHit;

  //  printf("A: %d %u\n",cur_ref,block);    
  refit(inode,block, 0);
  f->f_cb++;

#ifdef NO_PREFETCH_ON_HIT
  if (isitHit) 
    return;
#endif  

  if (w == TWRITE || noprefetch) {
    //    if (!isitHit) 
    //      ll_iorequest(0);
    return;
  }
  
  // determine if we want to prefetch
  end_index = fileEndBlock;
  raend = f->f_raend;
  max_ahead = 0;

  //PageLocked = reada_ok;
  // PageLocked is true if have pending IOs i.e. page is not ready
  PageLocked = !page->done;
  //PageLocked = check_group_ready(inode,block);

#ifdef SYNTRACE1
  printf("windows:  (%u--%u) %u\n",
	 raend-f->f_rawin,raend-1,PageLocked);
#endif




  // check the following for locked page stuff
  if (PageLocked) {
    if (!f->f_ralen || index >= raend || index + f->f_rawin < raend) {
      raend = index;
      if (raend < end_index)
	max_ahead = f->f_ramax;
      f->f_rawin = 0;
      f->f_ralen = 1;
      if (!max_ahead) {
	f->f_raend  = index + f->f_ralen;
	f->f_rawin += f->f_ralen;
      }
    }
  } else if (reada_ok && f->f_ramax && raend >= 1 &&
	     index <= raend && index + f->f_ralen >= raend) {
    raend -= 1;
    if (raend < end_index)
      max_ahead = f->f_ramax + 1;
    if (max_ahead) {
      f->f_rawin = f->f_ralen;
      f->f_ralen = 0;
      //reada_ok      = 2;
    }
  }

  // end of stuff

  //  extern unsigned cur_ref;
  //  printf("X %d: %d %d %d %d\n",cur_ref, FileTable[filD].f_cb,
  //	 FileTable[filD].f_needed
  //	 ,raend+1,max_ahead);

  // lets ref all the blocks that will not be done otherwise as
  // window has moved beyond them -- aggressive prefetching perhaps
  for (b=f->f_cb; b < raend +1 && b <f->f_needed; b++) {
    // are they already there .... we need to mark prefetched ones 
    // as synchronous
    int st = checkInCache(scheme,inode,b, 0);
    page2 = block_info(inode,b); // check it with I/O system

    if (st!=1) {
      asyncA = 0;
      if (!page2->done) {
	//+disksimCheckIssued(inode, b);
      } else {
	page2->done = 0;
	//+disksimInserRequest(DEMAND, inode, b, 1, w);
      }
      queue_io(b,inode,0,w);
    } else {
      if (!page2->done)
	disksimCheckIssued(inode, b);
    }

    //    printf("B: %d %u\n",cur_ref,b);    
    refit(inode,b, 0); 

    f->f_cb++;
  } 

  // do prefetching here
  ahead = 0;
  while (ahead < max_ahead) {
    ahead ++;
    if ((raend + ahead) >= end_index)
      break;

    b = raend+ahead;
    pVal=checkInCache(scheme,inode,b,1);// will also remove ghost enteries
    page = block_info(inode,b); // check it with I/O system

    // page is in cache ... lets continue over this
    if (pVal == 1) {
      if (b>=f->f_cb) {
	if (f->f_cb<f->f_needed) {
	  //	disksimInserRequest(DEMAND, inode, b, 1, w);
	  if (!page->done)
	    disksimCheckIssued(inode, b);
	  //	  printf("C: %d %u\n",cur_ref,b);    
	  assert(b==f->f_cb);
	  refit(inode,b, 0); 
	  f->f_cb++;
	}  else {
	  //disksimInserRequest(PREFETCH, inode, b, 1, w);
	  //fprintf(stderr,"Will this happen\n");
	  //	refit(inode,b, pVal); // 1 prefetch, 2 prefetch from ghost
	  //	printf("This may be the problem\n");
	  //		f->f_cb++;
	}
      }
      continue;
      //      break;
    }

    
    if (pVal == 0)  
      pVal = 1;
    
    // and then go ahead and prefetch        
#ifdef PRINTT
    fprintf(stderr,"F: %u %u\n",inode,b); 
#endif

    if (isitHit) {
      add_to_pending_io(&newReq,inode,b);
    }
    //    if (!page->done) {
    //      printf("problem with prefetching maintainence\n");
    //      exit(0);
    //    }      

    //    	queue_io(b,inode, isitHit);
    // check the following to make sure all is okay
    if (f->f_cb<f->f_needed) {
      if (page->done) {
	page->done = 0;
	//+disksimInserRequest(DEMAND, inode, b, 1, w);
      }
      asyncA = 0;
      queue_io(b,inode, 0,w);
      //      printf("D: %d %u\n",cur_ref,b);    
      if (b != f->f_cb)
	fprintf(stderr,">>> %u %u\n",b,f->f_cb);
      
      refit(inode,b, 0); 
      f->f_cb++;
    } else {
      if(page->done) {
	page->done = 0;
	//+disksimInserRequest(PREFETCH, inode, b, 1, w);
      }
      queue_io(b,inode, 1,w);
      assert (pVal !=0);
      //      printf("E: %d %u\n",cur_ref,b);    
      refit(inode,b, pVal); // 1 prefetch, 2 prefetch from ghost
    }

    //if (page_cache_readX(f, raend + ahead) < 0)
    //      break;
  }
  
  // end of prefetching
  if (ahead) {
    //  printf("%lu ",ahead);

    f->f_ralen += ahead;
    f->f_rawin += f->f_ralen;
    f->f_raend = raend + ahead + 1;

    f->f_ramax += f->f_ramax;

    if (f->f_ramax > max_readahead)
      f->f_ramax = max_readahead;
  }

  //  if (!isitHit) 
  //    clear_all_pending_ios();

  // count each bundled access as one io
  //  num_ios++;
  //  if (!isitHit) 
  //    ll_iorequest(0);

}

unsigned lastBlock = 0;
unsigned lastInode = 0;
unsigned num_reqs = 0;
unsigned lastType;

void queue_io(unsigned block, unsigned inode, int async, int type) 
{
  if (lastInode != 0) { 
    if (inode != lastInode 
	|| (inode == lastInode && block != lastBlock + 1)
	|| (lastsync==1 && async == 0)) {
      ll_iorequest(lastsync);
    } 
  }
  if (startBlock == 0) {
    lastsync = async;
    startBlock = block;
  }

  if (async == 0) 
    lastsync = async;
  
  lastBlock = block;
  lastInode = inode;
  lastType = type;
  
  num_reqs++;
  if (num_reqs>RCHUNK)
    ll_iorequest(async);
  
  
}

void ll_iorequest(int async)
{
  if (num_reqs>0) {

#ifdef PRINTBLOCKS
    fprintf(stderr,"%d a: %d | s: %2u | i: %7u  | %6u -- %6u (%u)\n",
	    line, async, 
	    num_reqs,lastInode,
	   startBlock,lastBlock , lastBlock-startBlock+1) ;
#endif

#ifdef SYNTRACE
    printf("\t\t\t\t\t%d\n",lastsync);
#endif
    if (lastsync) {
      asynReq++ ;
      asynReqSize += num_reqs;
      disksimInserRequest(PREFETCH, lastInode, startBlock, 
			  lastBlock-startBlock+1, lastType);
    } else {
      synReq++ ;
      synReqSize += num_reqs;
      disksimInserRequest(DEMAND, lastInode, startBlock, 
			  lastBlock-startBlock+1, lastType);

    }

    num_reqs = 0;
    num_ios++;
    lastInode = 0;
    startBlock = 0;
  }
}

void report_ios(void)
{
  printf("\tTotal:%u hr:%.2f%% hr2:%.2f%% prefetchHits:%u\n",
	 hit+miss,100.0*hit/(hit+miss),
	 100.0*(hit-prefetch_hits)/(hit+miss),
	 prefetch_hits);

  printf("\tUnused prefetches: %u Number of ios: %u  \n",badPF,num_ios);
  printf("\tSynReq: %u SynReqSize %u ASynReq %u ASynReqSize %u \n",
	 synReq, synReqSize, asynReq, asynReqSize);
  
}

void print_cache(int filD, unsigned block,unsigned inode) 
{
  
  FileEntry *f = &FileTable[filD];
  unsigned b;
  static unsigned iox= 0;
  int alpha = cachesize;
  printf("%c & [",'a'+block);
  //printf("[");
  for (b=0;b<16;b++) {
    if (checkInCache(scheme,inode,b, 0)) {
      printf("%c ",'a'+b);
      alpha--;
    }
  }
  while(alpha-->0) 
    printf("- ");

  if (iox != num_ios) {
    printf("] & y ");
    iox = num_ios;
  } else
    printf("] & n ");

   printf("& %d \\\\\n",f->f_rawin);
   //   printf("&\n");

}

/*
OPT : Accesses 1119161 Hit 67779 Miss 1051382 Ratio 6
Number of ios:1051382

 */
void accessCache(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc, int filD)
{
  
  //  cacheAccessX(type,inode,position,size,pc);
  //  return;


    unsigned blockstart, blockend, block;
    unsigned index, offset;
    int reada_ok = 0; 
    FileEntry *f = &FileTable[filD];
    
    //    filD = f->filD;
    //    inode = f->inode;
    
    position = transform(position,filD,&size);
    if (position == MAXINT)
      return;



    //find if exists
    // 		perform_io(filD,size,type); // arb

    if(inode == 0)
	printf("Inode == 0\n");

    if(size == 0)
    	return;
    blockstart = position / BLOCKSIZE;
    blockend = (position + size - 1)/ BLOCKSIZE;
    f->f_needed = 1+blockend;
  
    index = blockstart;
    offset = position % BLOCKSIZE;
    //    fprintf(stderr,"---------->%u\n",f->f_raend);
    // also do the following for writes
    //if (!index) 
    //  printf("%u %u \n",f->f_raend,f->f_rawin);

    //    printf("window:%u max:%u rok:%u rlen:%u end:%u\n",
    //	   f->f_rawin,f->f_ramax,reada_ok,f->f_ralen,
    //	   f->f_raend
    //	   );


    if (type == TWRITE  // || !index 
	|| index > f->f_raend || index + f->f_rawin < f->f_raend) {
      reada_ok = 0;
      f->f_raend = 0;
      f->f_ralen = 0;
      f->f_ramax = 0;
      f->f_rawin = 0;
      //            fprintf(stderr,"reseeting window %u\n",f->f_raend);
      //printf("reseeting window %u\n",f->f_raend);
    } else {
      reada_ok = 1;
    }

   



    if (!index && offset + size <= (BLOCKSIZE>>1)) {
      f->f_ramax = 0;
    } else {
      unsigned long needed;
      
      needed = ((offset + size)/BLOCKSIZE) + 1;
      //      f->f_needed += needed;

      if (f->f_ramax < needed)
	f->f_ramax = needed;
      
      if (reada_ok && f->f_ramax < MINRA)
	f->f_ramax = MINRA;
      if (f->f_ramax > MAXRA)
	f->f_ramax = MAXRA;
    }


    asyncA = 1;
    f->f_cb = blockstart;
    //    f->f_needed += blockstart;

#if 0
    fprintf(stderr,
    	    "-------->(%u:%u->%u) %u %u %u %u %u\n",size,blockstart,f->f_needed,
	    f->f_ramax,f->f_raend,f->f_rawin,
    	    f->f_ralen, f->f_reada);
#endif


    //    for(block = f->f_cb; f->f_cb<f->f_needed;block=f->f_cb) 
    //    extern unsigned cur_ref;
    //    printf("AA %d: %d %d\n",cur_ref, FileTable[filD].f_cb,FileTable[filD].f_needed-1 );

    //    fprintf(stderr,"PP %d: %u %u %u\n",line,inode, f->f_needed - blockstart+1,
    //	    f->fsize/BLOCKSIZE);
    while (f->f_cb<f->f_needed) 
    {
      block = f->f_cb;
      //      printf("::%d -> %d\n",block,f->f_needed);
      switch (scheme) {
      default:
      case ARC:
	do_ARC(filD, inode, block, type,reada_ok); // arb
	break;
	
      case OPT:
	do_OPT(filD, inode, block, type,reada_ok); // arb	
	break;

      case LRU:
	do_LRU(filD, inode, block, type,reada_ok); // arb
	break;
	
      case LIRS:
	do_LIRS(filD,inode,block, type,reada_ok);
	break;
	
      case LRFU:
	do_LRFU(filD,inode,block, type,reada_ok);
	break;

      case MQ:
	do_MQ(filD,inode,block, type,reada_ok);
	break;

      case TQ:
	do_TQ(filD,inode,block, type,reada_ok);
	break;

      case LRU2:
	do_LRU2(filD, inode, block, type,reada_ok); // arb
	break;
      }
    }
    //    if (f->f_cb!=f->f_needed)
    //      printf("%u %u\n",f->f_cb,f->f_needed);
    
    ll_iorequest(asyncA); // arb we need to do any pending ios now

#ifdef SYNTRACE
    //      ll_iorequest(1); // arb we need to do any pending ios now
    print_cache(filD,blockstart,inode);
#endif


}

void add_to_pending_io(int *newR,unsigned inode,unsigned block)
{
  page_struct *page = HASH_FIND_BLOCK(inode,block,pio);
  if (!page) {
    page = HASH_INSERT_BLOCK(inode,block,pio);
    page->done = 1;
  } else {
    page->isResident = 0;
  }
  if (page->OPT_next || page->OPT_prev) {
    // printf("ERROR: queued io already queued, should'nt have happened\n");
    return;
    exit(0);
 }

  page->OPT_next = pending_head->OPT_next;
  page->OPT_prev = pending_head;
  
  pending_head->OPT_next->OPT_prev = page;
  pending_head->OPT_next = page;
  num_pendings++;
  
}

void clear_all_pending_ios(void)
{
  page_struct *p2, *page = pending_head->OPT_next;
  while (page != pending_head) {
    page->isResident = 1;
    page->OPT_prev->OPT_next = page->OPT_next;
    page->OPT_next->OPT_prev = page->OPT_prev;
    page->OPT_prev = NULL;
    p2 = page->OPT_next;
    page->OPT_next = NULL;
    num_pendings--;

    page = p2;
  }
  //  if (num_pendings) 
  //    printf("In queue %d\n",num_pendings);
}

int check_group_ready2(unsigned inode, unsigned block)
{
  page_struct *page = HASH_FIND_BLOCK(inode,block,pio);
  if (!page) 
    return 1;
  
  if (page->isResident == 0) {
    return 0;
  }
  
  return 1;
}


int check_group_readyx(unsigned inode, unsigned block)
{
  page_struct *page = HASH_FIND_BLOCK(inode,block,pio);
    if (!page) 
  return 1;
  
  if (page->isResident == 0) {
    //    page->isResident = 1;
    //    printf("In queue %d\n",num_pendings);
    clear_all_pending_ios();
    return 0;
  }
  
  return 1;
}
int check_group_ready(unsigned inode, unsigned block)
{
  page_struct *page = HASH_FIND_BLOCK(inode,block,pio);
  //    if (!page) 
  //  return 1;
  
  if (page && page->isResident == 0) {
    //    page->isResident = 1;
    //    printf("In queue %d\n",num_pendings);
    clear_all_pending_ios();
    //+    return 0;
  }
  

  //+  return 1;

  page = block_info(inode,block); // check it with I/O system
  return page->done;

}

void report(void)
{
  switch(scheme) {
  default:
  case ARC:
    reportARC();
    break;

  case OPT:
    //    OPT_Repl();
    reportOPT();
    break;

  case LRU:
    reportLRU();
    break;

  case LIRS:
    reportLirs();
    break;

  case LRFU:
    reportLRFU();
    break;  

  case MQ:
    reportMQ();
    break;

  case TQ:
    reportTQ();
    break;

  case LRU2:
    reportLRU2();
    break;
  }
}

void prefetch_init(void)
{ 
  HASH_INIT_BLOCKTAB(HASH_FACTOR_BLOCKTAB, pio);
  HASH_ALLOC(1024, pio);

  pending_head->OPT_next = pending_head->OPT_prev = pending_head;
}

/*
  OPT : Accesses 158667 Hit 138371 Miss 20296 Ratio 87
*/

unsigned transform(unsigned pos, int filD,unsigned *s)
{
#ifdef SYNTRACE
  static unsigned size=16;
  unsigned alpha[] = {
    0,2,4,6,8,10,12,14,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    //    1,3,5,7,9,11,13,15,
    //    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
    //    0,1,2,3,4,5,6,
    MAXINT};
  unsigned val = alpha[dummyTrack];

  if (filD > 0) {
    FileEntry *f = &FileTable[filD];
    f->fsize = size * 4096;
  };

  
  if (val == MAXINT) 
    return MAXINT; 

  //*s = (4096*1);

  pos = (          val) * 4096;
  dummyTrack++;
  
  //  if (dummyTrack  > size) 
  //    size = dummyTrack;
#endif

  return pos;
}

void compleInCache(unsigned inode, unsigned startBlock, unsigned size)	/*called to mark blocks completed after the disk access*/
{
    struct CDB *bufp;
    unsigned block;

    for(block = startBlock; block < startBlock + size; block++)
    {
      bufp = HASH_FIND_BLOCK(inode,block,pio);
      //bufp = locate(inode, block);
      if (bufp)
	bufp->done = 1;
    }
}

