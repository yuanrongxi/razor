c_file		=$(wildcard *.c)
c_file		+=../../common/platform/posix/posix.c
o_file		=$(patsubst %.c,%.o,$(c_file))
d_file		=$(patsubst %.c,%.d,$(c_file))

CURRENT_DIR 	:=$(shell pwd)
CC		=gcc 
CC_FLAGS	=  -Wall -std=c99 -g -fPIC -I$(CURRENT_DIR)/../../common  -I/usr/local/include -DSU_SERVER
LD_FLAGS	=  -lpthread
CC_DEPFLAGS	=-MMD -MF $(@:.o=.d) -MT $@



all: print_c print_o sim_relay

print_c:
	echo $(c_file)
print_o:
	echo $(o_file)

%.o:  %.c
	$(CC)  $(CC_FLAGS) $(CC_DEPFLAGS) -c $< -o $@

sim_relay: $(o_file)
	gcc -o sim_relay $(o_file) $(LD_FLAGS) 


.PHONY :clean

clean:
	rm -f $(o_file) $(d_file) sim_relay


-include $(wildcard $(o_file:.o=.d))
