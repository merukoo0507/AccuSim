PROG = predict

BISON = bison
FLEX = flex

# make this point to the correct disksim/src directory
DSHOME = ../disksim/src


include $(DSHOME)/.paths
LDFLAGS = -lm $(LIBPARAM_LDFLAGS) $(DISKMODEL_LDFLAGS) $(LIBDDBG_LDFLAGS)
HP_FAST_OFLAGS = +O4
NCR_FAST_OFLAGS = -g -Hoff=BEHAVED 
FREEBLOCKS_OFLAGS =
DEBUG_OFLAGS = -g -DASSERTS # -DDEBUG=1
PROF_OFLAGS = -g -DASSERTS -p
GPROF_OFLAGS = -g -DASSERTS -pg
CFLAGS = $(DISKMODEL_CFLAGS) $(LIBPARAM_CFLAGS) $(LIBDDBG_CFLAGS) $(DEBUG_OFLAGS) -I $(DSHOME) $(FREEBLOCKS_OFLAGS) -D_INLINE 

CC = g++ -g -Wall -Wno-unused -MD


#CC = g++
#PROG = predict   
#CCOPT = -O5 -Wall

# Standard CFLAGS
#CFLAGS = $(CCOPT) -ggdb

DISKSIM_SRC = $(DSHOME)/disksim.c $(DSHOME)/disksim_intr.c $(DSHOME)/disksim_pfsim.c \
	$(DSHOME)/disksim_pfdisp.c $(DSHOME)/disksim_synthio.c $(DSHOME)/disksim_iotrace.c $(DSHOME)/disksim_iosim.c \
	$(DSHOME)/disksim_logorg.c $(DSHOME)/disksim_redun.c $(DSHOME)/disksim_ioqueue.c $(DSHOME)/disksim_iodriver.c \
	$(DSHOME)/disksim_bus.c $(DSHOME)/disksim_controller.c $(DSHOME)/disksim_ctlrdumb.c \
	$(DSHOME)/disksim_ctlrsmart.c $(DSHOME)/disksim_disk.c $(DSHOME)/disksim_diskctlr.c \
	$(DSHOME)/disksim_diskcache.c \
	$(DSHOME)/disksim_statload.c $(DSHOME)/disksim_stat.c $(DSHOME)/disksim_rand48.c $(DSHOME)/disksim_malloc.c \
	$(DSHOME)/disksim_cache.c $(DSHOME)/disksim_cachemem.c $(DSHOME)/disksim_cachedev.c \
	$(DSHOME)/disksim_simpledisk.c $(DSHOME)/disksim_device.c \
	$(DSHOME)/disksim_loadparams.c \
	$(DSHOME)/raw_layout.c 
#	$(DSHOME)/disksim_interface.c

DISKSIM_OBJ = $(DISKSIM_SRC:%.c=%.o) 


#.c.o:
#	@rm -f $@
#	$(CC) $(CFLAGS) -c $(srcdir)/$*.c

SRC =	predict.cc arc.cc prefetch.cc opt.cc lru.cc lirs.cc lrfu.cc mq.cc\
	lru2.cc 2q.cc disk.cc

# We would like to say "OBJ = $(SRC:.c=.o)" but Ultrix's make cannot
# hack the extra indirection
OBJ =	$(SRC:.cc=.o)
HDR =   .trace.h arc.h fs.h

TAGFILES = $(SRC) $(HDR)

CLEANFILES = $(PROG) $(OBJ)

all: $(PROG) 
$(OBJ): %.o: %.cc
	$(CC) -c $(CFLAGS) $< -o $@

#predict.o: predict.cc 
#	$(CC) $(CFLAGS) -c -o predict.o predict.cc 
#arc.o: arc.cc 
#	$(CC) $(CFLAGS) -c -o arc.o arc.cc 
#prefetch.o: prefetch.cc 
#	$(CC) $(CFLAGS) -c -o prefetch.o prefetch.cc 
#opt.o: opt.cc 
#	$(CC) $(CFLAGS) -c -o opt.o opt.cc 
#lru.o: lru.cc 
#	$(CC) $(CFLAGS) -c -o lru.o lru.cc 
#lirs.o: lirs.cc 
#	$(CC) $(CFLAGS) -c -o lirs.o lirs.cc 
#lrfu.o: lrfu.cc 
#	$(CC) $(CFLAGS) -c -o lrfu.o lrfu.cc 
#mq.o: mq.cc
#	$(CC) $(CFLAGS) -c -o mq.o mq.cc
#lru2.o: lru2.cc 
#	$(CC) $(CFLAGS) -c -o lru2.o lru2.cc 
#2q.o: 2q.cc
#	$(CC) $(CFLAGS) -c -o 2q.o 2q.cc

$(PROG): $(OBJ) 
	@rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DSHOME)/disksim_interface.o $(OBJ) $(DISKSIM_OBJ) $(LDFLAGS)


clean:
	rm -f $(CLEANFILES)

predict.o: predict.cc trace.h fs.h arc.h
arc.o: arc.cc arc.h fs.h
prefetch.o: prefetch.cc prefetch.h arc.h fs.h
opt.o: opt.cc opt.h util.h
lru.o: lru.cc arc.h fs.h
lirs.o: lirs.cc opt.o util.h
lrfu.o: lrfu.cc
mq.o: mq.cc mq.h fs.h
lru2.o: lru2.cc arc.h fs.h
2q.o: 2q.cc mq.h fs.h
