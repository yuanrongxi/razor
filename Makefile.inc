DEFINE_SEL=-DHAVE_EPOLL

CC = gcc
CXX32 = g++
CC32 = gcc

ARRU = ar -ru

gccVer = $(shell gcc -dumpversion | cut -f1 -d'.')
ifeq ($(gccVer), 4)
    CXXFLAG = -Wall -ggdb -D_REENTRANT $(DEFINE_SEL)
    CXXFLAG_R = -Wall -O2 -D_REENTRANT $(DEFINE_SEL)
    INCLUDE = -I../ -I. -I../../ -I../common/ -I../cc/ -I../estimator/ -I../bbr/ -I../pacing/ -I../remb/
else
    CXXFLAG = -Wall -ggdb -D_REENTRANT $(DEFINE_SEL)
    CXXFLAG_R = -Wall -O2 -D_REENTRANT $(DEFINE_SEL)
    INCLUDE = -I../ -I. -I../../ -I../common/ -I../cc/ -I../estimator/ -I../bbr/ -I../pacing/ -I../remb/
endif
