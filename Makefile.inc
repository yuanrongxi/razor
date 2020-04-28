CC = gcc
CXX32 = g++
CC32 = gcc

ARRU = ar -ru

CXXFLAG = -Wall -ggdb -D_REENTRANT 
CXXFLAG_R = -Wall -O2 -D_REENTRANT $(DEFINE_SEL)
INCLUDE = -I../ -I. -I../../ -I../common/ -I../cc/ -I../estimator/ -I../bbr/ -I../pacing/ -I../remb/ -I../sim_transport/fec
