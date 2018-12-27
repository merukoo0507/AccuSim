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
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <asm/unistd.h>
#include "trace.h"
#include "fs.h"
#include "arc.h"
#include "prefetch.h" // arb
#include "opt.h"
#include "mq.h"


//#define fopen64 fopen

extern SysTime exectime;		// current time
extern SysTime proctime;		// processor only time
extern SysTime nextappIO;		// time of the next I/O from the application


FileEntry *FileTable=NULL;
ProcessEntry ProcessTable[NPID];

void initPTable(void)
{
    for(int i = 0; i<NPID; i++)
    {
	ProcessTable[i].pid = 0;	
	ProcessTable[i].FileTab = NULL;
    }	
    
}

void clearTables(void)
{
    for(int i = 0; i<NPID; i++)
    {
	ProcessTable[i].pid = 0;
	if(ProcessTable[i].FileTab != NULL)
	{
	    FileTable = ProcessTable[i].FileTab;
	    for(int j = 0; j<NDESC; j++)
	    {
		FileTable[j].position = 0;
		FileTable[j].dirty = 0;
		FileTable[j].inode = 0;			
		
		// arb -- my inits
		  FileTable[j].fsize=0; // arb
		  FileTable[j].f_raend=0; // arb
		  FileTable[j].f_rawin=0; // arb
		  FileTable[j].f_ralen=0; // arb
		  FileTable[j].f_ramax=0; // arb
		  FileTable[j].f_reada=0; // arb 
		// arb -- end my inits
	    }
	}
    }	
		
}

void closeFtable(unsigned pid)
{
    for(int i = 0; i<NPID; i++)
    {
	if(ProcessTable[i].pid == pid)	
	    ProcessTable[i].pid = 0;
    }	
    
}


FileEntry *newFtable(unsigned pid)
{
    int i,j;
    for(i = 0; i<NPID; i++)
    {
	if(ProcessTable[i].pid == 0)
	{
	    ProcessTable[i].pid = pid;
	    // clear file table or allocate;
	    
	    if(ProcessTable[i].FileTab == NULL) //allocate
	    {
		ProcessTable[i].FileTab = (FileEntry *)calloc(NDESC, sizeof(FileEntry));
		if(ProcessTable[i].FileTab == NULL)
		    printf("alloc error\n");
		else
		    return ProcessTable[i].FileTab;
	    }
	    else // clear current one
	    {
		FileTable = ProcessTable[i].FileTab;
		for(j = 0; j<NDESC; j++)
		{
		    FileTable[j].position = 0;
		    FileTable[j].dirty = 0;
		    FileTable[j].inode = 0;			

		// arb -- my inits
		  FileTable[j].fsize=0; // arb
		  FileTable[j].f_raend=0; // arb
		  FileTable[j].f_rawin=0; // arb
		  FileTable[j].f_ralen=0; // arb
		  FileTable[j].f_ramax=0; // arb
		  FileTable[j].f_reada=0; // arb 
		// arb -- end my inits

		}
	    	return FileTable;	
	    }
	}	
	
    }
    return NULL;    
}



FILE *trace;
unsigned currtime = 0;
int count = 0, line = 0;

unsigned depth = 0;
unsigned loops = 1;
unsigned flushtime = 1000; //seconds
int nfiles = 50;

extern long target_T1;

void readTrace(int *newtrace, int ddx)
{
    write_open topen;
    write_close tclose;
    write_rws trws;
    write_thead thead;
    unsigned ptype, mainpid;
    unsigned currpid=0;
   char appname[80];
    int dirty = 0;
    write_prws tprws; //Zheng
    unsigned pos;

	while(fread(&thead, sizeof(write_thead), 1, trace))
	{
	    int i;
	    currtime++;
	    line++;
	    if(*newtrace)	
	    {
		*newtrace = 0;
		mainpid = thead.pid;
	    }

	    ptype = thead.type;
	    
	    if(currpid != thead.pid || FileTable == NULL) //get ftable
	    {
	    	currpid = thead.pid;
		//		fprintf(stderr,"New process\n");
		for(i = 0; i<NPID; i++)
		{
		    if(ProcessTable[i].pid == currpid)
		    {	
			FileTable = ProcessTable[i].FileTab;
		    	break;
		    }
		}
		if(i==NPID)
 			FileTable = newFtable(thead.pid);
		
	    }

	    if(dirty) //check for outstanding writes
	    {
		/*if((last_write + flushtime) < thead.atime.tv_sec)
		{ //flush has to be performed
		    pcapFlush();
		    dirty = 0;
		}*/
	    }
	    
	    switch(thead.type)
	    {
	      case TNEW:
		fread(appname, thead.fnamesize, 1, trace);
		appname[thead.fnamesize] = 0;
		
		break;
		
	      case TFORK:
		break;
		
	      case TEXIT:
		// scan and close filedesc table
		closeFtable(thead.pid);
		//printf("count %d line %d pid %d EXIT\n", count, line, thead.pid);
		
		/* if(thead.pid == mainpid)
		   pcapCycleExitAll(mstime); */
		
		
		/*if(thead.pid == mainpid)
		{
		    if(dirty)
		    {
			pcapFlush();
			dirty = 0;
		    }
		}	*/					
		
		
		break;
		
            case TREAD:
            case TPREAD:
              assert(fread(&trws, sizeof(write_rws), 1, trace) == 1);
		//get table
		if(FileTable[trws.filedes].inode != thead.inode)
		{
		  //	  fprintf(stderr,"happebning %d\n",trws.filedes);
		  int j = trws.filedes;
		  FileTable[trws.filedes].position = 0;
		    FileTable[trws.filedes].dirty = 0;
		    FileTable[trws.filedes].access = 0;
		    FileTable[trws.filedes].inode = thead.inode;
		    
		    
		    // arb -- my inits
		    FileTable[j].fsize=0; // arb
		    FileTable[j].f_raend=0; // arb
		    FileTable[j].f_rawin=0; // arb
		    FileTable[j].f_ralen=0; // arb
		    FileTable[j].f_ramax=0; // arb
		    FileTable[j].f_reada=0; // arb 
		    // arb -- end my inits
		    

		}
		FileTable[trws.filedes].fsize = trws.fsize;//arb
		FileTable[trws.filedes].access++;

		// another potential I/O activity from the app
		// perform any pending disk operations that are due before
		// the time the app has an I/O
		
		nextappIO = exectime + ((double)trws.iotime)/1000000+0.000001;
		//nextappIO = 0.01 + ((double)trws.iotime)/1000000+0.000001;
		//		nextappIO = exectime + 0.01;  // arb
		disksimInternal();
		
		// perform app IO

		if(exectime != 0 && ((int)trws.iotime) > 0) //new ignore the start time
		{
		    exectime = nextappIO;
		    proctime += ((double)trws.iotime)/1000000;
		}

		FileTable[trws.filedes].filD = trws.filedes;
		//	fprintf(stderr,"%d ",FileTable[trws.filedes].filD);

		//Zheng
		if(thead.type == TREAD)
		  pos = FileTable[trws.filedes].position;
		else{
		  assert(fread(&tprws, sizeof(write_prws), 1, trace) == 1);
		  pos = tprws.poffset;
		}
		//Zheng end

		// call cache with position and inode
		//cacheAccess(TREAD, FileTable[trws.filedes].inode, FileTable[trws.filedes].position, trws.iosizer, trws.pc+trws.pcf);
		if (ddx)
		  cacheAccessX(TREAD, FileTable[trws.filedes].inode, pos/*was FileTable[trws.filedes].position*/, trws.iosizer, trws.pc+trws.pcf+trws.pcall,trws.filedes);
		else
		  accessCache(TREAD, FileTable[trws.filedes].inode, pos/*was FileTable[trws.filedes].position*/, trws.iosizer, trws.pc+trws.pcf+trws.pcall,trws.filedes);
		//		  accessCache(TREAD, trws.filedes, FileTable[trws.filedes].position, trws.iosizer, trws.pc+trws.pcf+trws.pcall,trws.filedes);

		//FileTable[trws.filedes].position += trws.iosizer;
                if(thead.type == TREAD)//Zheng
		  FileTable[trws.filedes].position += trws.iosizer;

		/*if(miss)
		  dirty = 0;		*/
		break;
		
	      case TWRITE:
	      case TPWRITE:
		assert(fread(&trws, sizeof(write_rws), 1, trace) == 1);

		if(FileTable[trws.filedes].inode != thead.inode)
		{
		    FileTable[trws.filedes].position = 0;
		    FileTable[trws.filedes].dirty = 0;
		    FileTable[trws.filedes].access = 0;
		    FileTable[trws.filedes].inode = thead.inode;
		    {
		      int j = trws.filedes;
		      // arb -- my inits
		      FileTable[j].fsize=0; // arb
		      FileTable[j].f_raend=0; // arb
		      FileTable[j].f_rawin=0; // arb
		      FileTable[j].f_ralen=0; // arb
		      FileTable[j].f_ramax=0; // arb
		      FileTable[j].f_reada=0; // arb 
		      // arb -- end my inits
		    }

		}
		FileTable[trws.filedes].fsize = trws.fsize;//arb
		FileTable[trws.filedes].access++;

		// another potential I/O activity from the app
		// perform any pending disk operations that are due before
		// the time the app has an I/O
		
		nextappIO = exectime + ((double)trws.iotime)/1000000+0.000001;
		//nextappIO = 0.01 + ((double)trws.iotime)/1000000+0.000001;
		//		nextappIO = exectime + 0.01;  // arb
		disksimInternal();
		
		// perform app IO
		
		if(exectime != 0 && ((int)trws.iotime) > 0) //new ignore the start time
		{
		    exectime = nextappIO;
		    proctime += ((double)trws.iotime)/1000000;
		}
		
		FileTable[trws.filedes].filD = trws.filedes;

		//Zheng
		if(thead.type == TWRITE)
		  pos = FileTable[trws.filedes].position;
		else{
		  assert(fread(&tprws, sizeof(write_prws), 1, trace) == 1);
		  pos = tprws.poffset;
		}
		//Zheng end

		//call cache 
		//cacheAccess(TWRITE,FileTable[trws.filedes].inode, FileTable[trws.filedes].position, trws.iosizer, trws.pc+trws.pcf);
		if (ddx)
		  cacheAccessX(TWRITE, FileTable[trws.filedes].inode, pos/*was FileTable[trws.filedes].position*/, trws.iosizer, trws.pc+trws.pcf+trws.pcall,trws.filedes);
		else
		  //		  accessCache(TWRITE, trws.filedes, FileTable[trws.filedes].position, trws.iosizer, trws.pc+trws.pcf+trws.pcall,trws.filedes);

		  accessCache(TWRITE, FileTable[trws.filedes].inode, pos/*was FileTable[trws.filedes].position*/, trws.iosizer, trws.pc+trws.pcf+trws.pcall,trws.filedes);
		
		FileTable[trws.filedes].dirty = 1;

		if(thead.type == TWRITE)//Zheng
		  FileTable[trws.filedes].position += trws.iosizer;
		
		dirty = 1;

		break;
		
	      case TOPEN:
		fread(&topen, sizeof(write_open), 1, trace);
		fread(appname, topen.fnamesize, 1, trace);
		appname[topen.fnamesize] = 0;
		
		FileTable[topen.filedes].position = 0;
		FileTable[topen.filedes].dirty = 0;
   	        FileTable[topen.filedes].access = 0;
		FileTable[topen.filedes].inode = thead.inode;
		{
		  int j = topen.filedes;
		  // arb -- my inits
		  FileTable[j].fsize=0; // arb
		  FileTable[j].f_raend=0; // arb
		  FileTable[j].f_rawin=0; // arb
		  FileTable[j].f_ralen=0; // arb
		  FileTable[j].f_ramax=0; // arb
		  FileTable[j].f_reada=0; // arb 
		  // arb -- end my inits
		}
		//fprintf(stderr,"open %d %s\n",topen.filedes,appname);
		// add to descriptor table
		break;
		
	      case TCLOSE:
		fread(&tclose, sizeof(write_close), 1, trace);
		
		// call cache
		if(FileTable && tclose.filedes < NDESC)
		FileTable[tclose.filedes].inode = 0;
		//fprintf(stderr,"close\n",tclose.filedes);
		
		// remove from desc table
		break;
		
	      case TSEEK:
		fread(&trws, sizeof(write_rws), 1, trace);
		if(FileTable[trws.filedes].inode != thead.inode)
		{
		    FileTable[trws.filedes].position = 0;
		    FileTable[trws.filedes].dirty = 0;
		    FileTable[trws.filedes].inode = thead.inode;
		}
		// cacheEvict(FileTable[trws.filedes].inode);
		FileTable[trws.filedes].position = trws.iosizer;
		// adjust position in desc table
		break;
	    }
            disksimComplete();
	    
	}


}


int main(int argc, char* argv[])
{
    char fname[80], infile[80];

     int newtrace;
    int c1;
    char c;

    //sprintf(infile,"/root/trace/cache_traces/%s",argv[1]);
    
    
    while ((c1=getopt(argc,argv,"a:d:f:l:n:s:r:m:t:e:p:x:c:h:k:g:q:")) != -1)
    {
	
	c=c1;
	switch(c)
	{
	  case 'a': // # skip in l2 data// purdue
	    sprintf(infile,"%s",optarg);
	    break;	  
	    
	  case 's':
	    cachesize = atoi(optarg);
	    break;
	    
	  case 'p':
	    scheme = atoi(optarg);
	    break;

	  case 'f':
	    flushtime = atoi(optarg);
	    break;

	  case 'l':
	    //	    loops = atoi(optarg); // used to be loops
	    lambda = atof(optarg);
	    break;

	  case 'n':
	    nfiles = atoi(optarg);
	    break;

	  case 'c':
	    corperiod = atoi(optarg);
	    break;

	  case 'h':
	    history = atoi(optarg);
	    break;

	  case 't':
	    //   target_T1 = atoi(optarg);
	    lifeTime = atoi(optarg);
	    break;

	  case 'x':
	    noprefetch = !atoi(optarg);
	    break;

          case 'k':
            K = atoi(optarg);
            break;

          case 'q':
            numQ = atoi(optarg);
            break;

          case 'g':
            ghostSize = atoi(optarg);
            break;

	  default:
	    printf("Please refer to the predict.cc for a detailed\ndescription of the PCAP command line options.\n");
	    return 1;
	    break;
	}
    }
    
    {
      fprintf(stderr,"Name:%s Size:%u Scheme:%d Pref:%d\n",
	      infile,cachesize,scheme,!noprefetch);
    }

    prefetch_init();
    initLRFU(); // for LRFU
    initARC(); // for ARC
    initOPT(); // for OPT
    initLirs(); // for Lirs requires OPT initialized as well
    initPTable();
    initMQ(); // for MQ
    initDisksim();

    while(1)
    {
    	nfiles--;	
	if(nfiles == 0)
		break;
	
    	sprintf(fname,"%s",infile);
    	count++;
    	
    	trace=fopen64(fname,"r");
    	if (!trace) {
	  printf("Unable to open file: %s\n",fname);
	  exit(0);
	}
	  

    	if(count == 2)
	{
	    if(loops > 1)
	    {
		count = 0;
		loops--;
//		resetStat();
		continue;
	    }
	    else
		break;
	    
	}
	
    	line = 0;
	
	clearTables();

	newtrace = 1;
	
	////
	if (scheme == OPT) {
	  // arb do a preread of the whole thing
	  readTrace(&newtrace,1);
	  clearTables();
	  currtime = 0;
	  line = 0;
	  fseek(trace,0,SEEK_SET);
	  dummyTrack = 0;

	}
	readTrace(&newtrace,0);
	

    }
    report();
    report_ios();
    disksimShutdown();    
    return 0;
}
