c_file		=$(wildcard *.c)
c_file		+=$(wildcard ../win_log/*.c)
c_file		+=../../common/platform/posix/posix.c
c_file		+=$(wildcard ../../common/*.c)
c_file		+=$(wildcard ../../bbr/*.c)
c_file		+=$(wildcard ../../cc/*.c)
c_file		+=$(wildcard ../../pacing/*.c)
c_file		+=$(wildcard ../../remb/*.c)
c_file		+=$(wildcard ../../estimator/*.c)
c_file		+=$(wildcard ../../sim_transport/*.c)
c_file		+=$(wildcard ../../sim_transport/fec/*.c)

o_file		=$(patsubst %.c,%.o,$(c_file))
d_file		=$(patsubst %.c,%.d,$(c_file))


CURRENT_DIR 	:=$(shell pwd)
CC		=gcc

INCLUDE_DIR :=	$(CURRENT_DIR) \
  				$(CURRENT_DIR)/../../common \
  				$(CURRENT_DIR)/../../cc \
				$(CURRENT_DIR)/../../bbr \
				$(CURRENT_DIR)/../../pacing \
				$(CURRENT_DIR)/../../remb \
				$(CURRENT_DIR)/../../estimator \
				$(CURRENT_DIR)/../../sim_transport \
				$(CURRENT_DIR)/../../sim_transport/fec \
				$(CURRENT_DIR)/../win_log \
				/usr/local/include

CC_FLAGS	=  -Wall -std=c99 -g -fPIC $(addprefix -I, $(INCLUDE_DIR)) \
			  -DSU_SERVER -D_POSIX_C_SOURCE=200809L
LD_FLAGS	=  -lpthread -lm
CC_DEPFLAGS	=-MMD -MF $(@:.o=.d) -MT $@



all: print_c print_o send

print_c:
	echo $(c_file)
print_o:
	echo $(o_file)

%.o:  %.c
	$(CC)  $(CC_FLAGS) $(CC_DEPFLAGS) -c $< -o $@

send: $(o_file)
	gcc -o send $(o_file) $(LD_FLAGS) 


.PHONY :clean

clean:
	rm -f $(o_file) $(d_file) send


-include $(wildcard $(o_file:.o=.d))
