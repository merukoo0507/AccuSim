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

#ifndef _fs_h_
#define _fs_h_ 1

#define PC_NUMBER               1024
#define NDESC 1024
#define NPID 128

struct FileEntry
{
  unsigned inode;
  unsigned position; // :31; //arb was :31
  unsigned dirty; // :1; arb
  unsigned access; //:1; arb

  unsigned fsize; // arb
  unsigned f_raend; // arb
  unsigned f_rawin; // arb
  unsigned f_ralen; // arb
  unsigned f_ramax; // arb
  unsigned f_reada; // arb 
  unsigned f_needed;//arb
  unsigned f_cb; //arb 
  unsigned filD;
};

struct ProcessEntry
{
    unsigned pid;
    FileEntry *FileTab;
};

extern FileEntry *FileTable;
//extern ProcessEntry ProcessTable[NPID];




#endif
