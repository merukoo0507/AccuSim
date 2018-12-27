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

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trace.h"
#include "prefetch.h"

SysTime exectime = 0;		// current time
SysTime proctime = 0;		// processor only time
SysTime nextappIO = 0;		// time of the next I/O from the application
SysTime next_event = -1;	/* next event */

int completed = 0;	/* last request was completed */
struct disksim *disk;
int nsectors;
int reqid = 0;  // assign unique id to each request
int demandreq = -1; //request that the app is waiting to complete
int maxcluster = 0;

//request stats
unsigned reqdemandR = 0;
unsigned reqdemandW = 0;
unsigned reqprefetch = 0;
unsigned reqlatepref = 0;
unsigned demandNum = 0;
unsigned prefetchNum = 0;

Request *freeReq=NULL;
Request *pendingReq=NULL;

void initDisksim(void)
{
    char paramFile[100] = "cheetah9LP.parv";
    //char paramFile[100] = "3disks2.parv";
    //char paramFile[100] = "synthraid5.parv";
    char outFile[100] = "syssim.out";
    int len = 8192000;
    Request *req;

    nsectors = 17783240;
    
    disk =  (struct disksim *)malloc (len);
    disk = disksim_interface_initialize(disk, len, paramFile, outFile);

    for(int i = 0; i < NUMREQ; i++)
    {
	
	req = new(Request);
	req->pending = 0;
	req->next = freeReq;
	freeReq = req;
    }

}
/*
 * Schedule next callback at time t.
 * Note that there is only *one* outstanding callback at any given time.
 * The callback is for the earliest event.
 */

extern "C" void syssim_schedule_callback(void (*f)(void *,double), SysTime t)
{
  next_event = t;
}
/*
 * de-scehdule a callback.
 */
extern "C" void syssim_deschedule_callback(void (*f)())
{
  next_event = -1;
}

extern "C" void syssim_report_completion(SysTime t, Request *req)
{
    // request completed
  completed = 1;
  if(req->id == demandreq) //we are waiting for this request
  {
      exectime = t;
      demandreq = -1;
  }

  //remove request from pending Q
  
  if(req == pendingReq) //head of the queue
  {
      pendingReq = req->next;      
  }
  else
  {  
      if(req->next != NULL) //at the end of the q
	  req->next->prev = req->prev;

      req->prev->next = req->next;
  }

  //insert in free Q
  req->pending = 0;
  req->next = freeReq;
  freeReq = req;

  compleInCache(req->inode, req->block, req->blockcount);


  //  printf("C C id %6d inode %8x block %10d time %4.6f\n", req->id, 0, 0, exectime);

}

// returns hit if match in pending buffers
void disksimCheckIssued(unsigned inode, unsigned blkno)
{
    Request *req;
    unsigned long block = ((inode << 11)+blkno*BLOCK2SECTOR)%nsectors;

    for(req = pendingReq; req != NULL; req = req->next)
    {
	if(req->blkno <= block && block < req->blkno + req->blockcount) //the reqest in prefetch?
	{
	    if(req->id > demandreq) //set to last request
		demandreq = req->id; //last demand request got to wait for that
	    req->access_type = DEMAND;

	    reqlatepref++; //demand is waiting for prefetch
	    
	    return;
	}
    }
}


void disksimInserRequest(unsigned type, unsigned inode, unsigned startblock, unsigned size, unsigned reqtype)
{
    unsigned i, j;
    unsigned long stblock = ((inode << 11)+startblock*BLOCK2SECTOR)%nsectors;
    Request *req;

    // get free req from the reg list
    
    if(type == DEMAND)
    {
    	demandNum++;
    	if(reqtype == TREAD)
    		reqdemandR += size;
	else
   		reqdemandW += size;
    }
    else
    {
    	prefetchNum++;
	reqprefetch += size;
    }

    if(freeReq == NULL)
    {
	//printf("Getting more requests \n");
	for(int i = 0; i < NUMREQ/10; i++)
	{
	    req = new(Request);
	    req->pending = 0;
	    req->next = freeReq;
	    freeReq = req;
	}
    }

    req = freeReq;
    freeReq = freeReq->next;
    
    //insert into pendignQ
    if(pendingReq != NULL)
    	pendingReq->prev = req;
    req->next = pendingReq;
    pendingReq = req;

    req->start = exectime;
    req->type = (reqtype == TREAD)?'R':'W';
    req->devno = 0;

    /* NOTE: it is bad to use this internal disksim call from external... */
    req->blkno = stblock;
    req->bytecount = BLOCKSIZE * size;
    req->blockcount = size;
    req->pending = 1;
    completed = 0;
    req->inode = inode;
    req->block = startblock;
    
    req->access_type = type;

    if(type == DEMAND && demandreq < reqid && reqtype == TREAD) //it is a demand read
    {
	demandreq = reqid; //last demand request got to wait for that
                          // no wait for prefetch
	/*	if(reqtype == TREAD)
	  	printf("D R id %6d inode %8x block %10d time %4.6f\n", reqid, vnode,pblkno, exectime);
		else
		printf("D W id %6d inode %8x block %10d time %4.6f\n", reqid,vnode, pblkno, exectime);*/
			
    }
    // else
    //printf("P P id %6d inode %8x block %10d time %4.6f\n", reqid, vnode, pblkno, exectime);

    req->id = reqid++;
    
    
    disksim_interface_request_arrive(disk, exectime, req);
    
}
void disksimComplete(void) //wait for I/O completion
{
    while(next_event >= 0 && demandreq != -1) // we are waiting for an event to complete
    {
	exectime = next_event;
	next_event = -1;
	disksim_interface_internal_event(disk, exectime);
    }
}
void disksimInternal(void) //perform internal disk op after I/O completion
{
    while(next_event >= 0 && next_event < nextappIO) // perform all beore IO
    {
	exectime = next_event;
	next_event = -1;
	disksim_interface_internal_event(disk, exectime);
    }
}

void disksimShutdown(void) //perform internal disk op after I/O completion
{
    printf("DemandR %d DemandW %d Preftech %d Late %d \n", reqdemandR, reqdemandW, reqprefetch, reqlatepref);
    printf("Number of I/Os: Demand %d Preftech %d Total %d \n", demandNum, prefetchNum, demandNum+prefetchNum);
    printf("Processor %f I/O %f Total %f \n", proctime, exectime-proctime, exectime);

    disksim_interface_shutdown(disk, exectime);
}

