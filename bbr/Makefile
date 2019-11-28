include ../Makefile.inc

SRCS=$(wildcard *.c platform/posix/*.c)
SRC_COMM=$(patsubst %.c, %.o, $(SRCS))

#SRC_COMM = cf_crc32.c  cf_hex.c  cf_list.c  cf_skiplist.c  cf_stream.c  cf_unwrapper.c \
			platform/posix/posix.c

OBJ_COMM  = $(SRC_COMM:.cpp=.o)
SLIB=libbbr.a
DIR=../lib/

.SUFFIXES: .o .cpp
.cpp.o:
	$(CXX) $(CXXFLAG) ${INCLUDE} -c -o $@ $<

.SUFFIXES: .o .c
.c.o:
	$(CC) $(CXXFLAG) ${INCLUDE} -c -o $@ $<


all: $(SLIB)

$(SLIB): $(OBJ_COMM) | ../lib
	$(ARRU) $(DIR)/$(SLIB) $(OBJ_COMM)
	ranlib  $(DIR)/$(SLIB)

../lib:
	mkdir ../lib
  
depend:
	mkdep $(CXXFLAG) $(SRC_COMM)

install:

clean: 
	rm -f *.o $(DIR)/$(SLIB) 
