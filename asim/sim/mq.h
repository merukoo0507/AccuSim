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

#ifndef _mq_h_
#define _mq_h_ 1

#define CACHE  0
#define GHOST  1 

#define A1  0
#define Am  1
//#define GHOSTQ  2
#define FREEQ  3


#define	MAXQ		11	/* max (real+ghost) queues */
#define	GHOSTQ		MAXQ-1	/* ghost is the lat queue*/

CDB *get_lru(unsigned queue);
unsigned QueueNum(unsigned refs);
CDB *EvictBlock(unsigned type);
void Adjust(void);
int MQref(unsigned inode, unsigned block, int prefetch);
int MQCHECK(unsigned block, unsigned inode);

CDB *MQlru_remove(unsigned queue);
void MQmru_insert(CDB *bp, unsigned queue);
void MQremove_from_list(CDB *bp);

void initMQ(void);
void reportMQ(void);
void accessMQ(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc);

void do_MQ(int filD, unsigned inode, unsigned block, int type, int rok);

extern unsigned ghostSize, numQ, lifeTime;
extern unsigned K;
extern unsigned allocBufs; //number of allocated buffers

extern unsigned	ghostSize;
extern unsigned	ghosthit;
extern CDB qLists[]; //q3 is always ghost
extern unsigned qLength[]; //size of each list


CDB *TQEvictBlock(unsigned type);
int TQref(unsigned inode, unsigned block, int prefetch);
int TQCHECK(unsigned block, unsigned inode);
void reportTQ(void);
void accessTQ(unsigned type, unsigned inode, unsigned position, unsigned size, unsigned pc);
void do_TQ(int filD, unsigned inode, unsigned block, int type, int rok);


#endif
