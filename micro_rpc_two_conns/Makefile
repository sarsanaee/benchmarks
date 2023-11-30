TARGETS = unidir_ll_server

TAS_CODE?=${HOME}/tas

CFLAGS = -std=gnu99 -g -Wall -O3 -march=native \
	-I$(TAS_CODE)/include -I$(TAS_CODE)/lib/tas/include
LDFLAGS =-m64 -g -O3 -pthread  -march=native -no-pie
LDLIBS = -lrt

include ../common/Makefile.mtcp

# all: unidir_ll_server unidir_ll_client

all: unidir_ll_server

unidir_ll_server: unidir_ll_server.o unidir_ll_comon.o $(TAS_CODE)/lib/libtas.so
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# unidir_ll_client: unidir_ll_client.o $(TAS_CODE)/lib/libtas.so
# 	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o ${TARGETS}

.PHONY: all clean
