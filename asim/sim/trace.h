/**************************************************************************
*                                                                         *
*                                                                         *
*  Copyright (c) 2005                                                     *
*  by Ali R. Butt, Chris Gniady, Y. Charlie Hu                            *
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

#ifndef _trace_h_
#define _trace_h_

#include <sys/time.h>


#define TNEW 0
#define TFORK 1
#define TEXIT 2
#define TREAD 3
#define TWRITE 4
#define TOPEN 5
#define TCLOSE 6
#define TSEEK 7
#define TSEEKL 8
#define TSTAT 9

//Zheng -- fix pread/pwrite bug, add two types
#define TPREAD 13
#define TPWRITE 14

typedef struct _write_thead
  {
        unsigned type:5; /* from the above defines*/
        unsigned pid:27;
	union
	{
		unsigned inode;
		unsigned child;
		unsigned fnamesize;
	};
		
  } write_thead;

typedef struct _write_open
  {
        unsigned fsize;
	unsigned fnamesize:16;
	unsigned filedes:16;
  } write_open;

typedef struct _write_close
  {
	unsigned filedes:16;
  } write_close;

//asko does seek
typedef struct _write_rws  
  {
        unsigned pc; /*first pc of the stack my be in the library*/
        unsigned pcf; /*first pc in the application*/
        unsigned pcall; /* signature of the call stack */
	int iosize; /* requested number of bytes for read, write or seek*/
	int iosizer; /* number of bytes read or written or seeked */
	unsigned filedes:16;
	unsigned iotime; /* difference in us between two i/Os */ 
	unsigned fsize;

  } write_rws;

//Zheng
//supplementary
typedef struct _write_prws
{
  unsigned poffset; //Zheng
  //For pread() and pwrite(), it is the offset to be read and written
  //Useless for others
} write_prws;

extern FILE *my_trace;

#ifdef TDEBUG
#define TDEBUG(fmt, args...)  printf(fmt, ## args)
#else
#define TDEBUG(fmt, args...)
#endif




#endif
