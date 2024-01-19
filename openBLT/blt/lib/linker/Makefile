# $Id: //depot/blt/lib/linker/Makefile#2 $

include ../../make.conf

##### BLT stuff #####
#CFLAGS = $(CF) -DBLT -I../../include

##### Linux stuff #####
CFLAGS = -DLINUX -g

.c.o:
	cc $(CFLAGS) -c $<

#################################

OBJS = start.o elf.o file.o link.o

all: linker

linker: $(OBJS)
	$(LD) -o linker $(OBJS) -Ttext 200000 -e _dl_start -Bstatic -L../../lib/obj -lc -lblt
	chmod a-x linker

clean:
	rm -f linker *.o *~ core

linux: $(OBJS)
	cc -o linker $(OBJS)

