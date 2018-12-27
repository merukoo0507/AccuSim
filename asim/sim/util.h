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

#ifndef	_util_h
#define	_util_h

#define	LAST_MISS_MAX		4
#define	NEXT_ADDR_MAX		1
#define	NEXT_ADDR_MASK		(NEXT_ADDR_MAX - 1)

#define HASH_FACTOR_PCTAB	512
#define HASH_FACTOR_BLOCKTAB	256000 // 2048
#define HASH_FACTOR_SIG		512
#define FALSE			0
#define TRUE			1
#define MAXINT			0xFFFFFFFF

//#define HASH_FUNC(VALUE) (0x01020304^((unsigned int)(VALUE)))
#define HASH_FUNC(V1, V2) (((unsigned int)(V1)<<16)^((unsigned int)(V2)))
#define HASH_SET(TABLE,addr)		((TABLE).table[addr])


void HASH_INIT_BLOCKTAB(int tablesize, LTAddrHash *tb);
void HASH_ALLOC(int num, LTAddrHash *table);
page_struct *HASH_FIND_BLOCK(unsigned inode, unsigned block,  LTAddrHash *table);
page_struct *HASH_INSERT_BLOCK(unsigned inode, unsigned block, LTAddrHash *table);

#endif
