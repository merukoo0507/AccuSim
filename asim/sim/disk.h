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

#ifndef _disk_h_
#define _disk_h_ 1

extern "C"
{
#include "../disksim/src/syssim_driver.h"
}

#define NUMREQ 128
#define	SECTOR	512
#define	BLOCK2SECTOR	(BLOCKSIZE/SECTOR)

void initDisksim(void);
extern "C" void syssim_schedule_callback(void (*f)(void *,double), SysTime t);
extern "C" void syssim_deschedule_callback(void (*f)());
extern "C" void syssim_report_completion(SysTime t, Request *req);
void disksimCheckIssued(unsigned inode, unsigned blkno);
void disksimInserRequest(unsigned type, unsigned inode, unsigned startblock, unsigned size, unsigned reqtype);
void disksimComplete(void);
void disksimInternal(void);
void disksimShutdown(void);

#endif
