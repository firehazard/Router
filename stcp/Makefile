# CS244a HW#3/4 (Reliable Transport
# This makefile requires GNU make v3.80 or later
#
# Updated Heavily by Paul Tarjan (Feb 16, 2007)

# see the following included file for system-specific settings
include ENVCFG.MK

CC=g++
CFLAGS=-g -D$(ENV) -D_REENTRANT $(ENVCFLAGS) -Wall -W -Wno-unused-function \
       -Wno-unused-parameter #-DDEBUG
LIBS=$(ENVLIBS)
MAKEFILE=Makefile
LN=ln
RM=rm
AR=ar crus

SRCS_MYSOCK = transport.c mysock_api.c stcp_api.c mysock.c network.c \
              connection_demux.c tcp_sum.c network_io.c
SRCS_IO = network_io_tcp.c network_io_socket.c
SRCS = $(SRCS_MYSOCK) $(SRCS_IO)

APP_SRCS = echo_server_main.c echo_client_main.c server.c client.c proxyget.c

# sources for which dependencies are generated with 'make depend'
DEPEND_SRCS = $(SRCS) $(APP_SRCS)

OBJS_MYSOCK = $(SRCS_MYSOCK:.c=.o)
OBJS_IO = $(SRCS_IO:.c=.o)
OBJS = $(OBJS_MYSOCK) $(OBJS_IO)

ECHO_SERVER_OBJS=echo_server_main.o $(OBJS_VNS)
ECHO_CLIENT_OBJS=echo_client_main.o $(OBJS_VNS)

.PHONY: clean all rebuild

LIBSPROXY= proxy.a
PROXY_SRCS = myproxy.cpp
PROXY_OBJS = $(PROXY_SRCS:.cpp=.o)
BINARIES = client server proxyget \
           proxy \
           our.proxy \
           your.proxy \
           stcp_echo_client stcp_echo_server
SR_SRC = sr_src
SR_EXE = sr

# note: for HW4, you'll probably want to take out the dependency on
# 'all_hw3' once you change your proxy code to call sr_api_init().
# if you don't, you'll get link errors when you try to build proxy.
all: all_hw3

all_hw3: client server stcp_proxy

# these rules are required by the grading script--please don't change them!
stcp_proxy: proxy proxyget

sr: force
	-$(MAKE) -C $(SR_SRC) && cp -f $(SR_SRC)/$(SR_EXE) $@ || \
	 echo "***using reference sr***"

force:

# if you specify sources for proxy, this makefile builds your version of
# proxy; otherwise, it uses the pre-built reference solution.  the
# resulting executable is named 'proxy' for the TCP-based network layer

ifeq ($(strip $(PROXY_OBJS)),)
PREFIX=our
else
PREFIX=your
endif

proxy: $(PREFIX).proxy
	$(LN) -sf $< $@

our.proxy: $(OBJS) 
	$(warning ***compiling with reference proxy...***)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LIBSPROXY)

your.proxy: $(OBJS) $(PROXY_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

depend_%:
	$(CC) $(CFLAGS) -MM -MT \
	      '$(subst depend_,,$@).o' $(subst depend_,,$@).c >> $(MAKEFILE).new

rebuild: clean all

clean:
	-$(RM) -f *.o *.c~ *.h~ *.purify core* rcvd $(BINARIES)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

proxyget: proxyget.o $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

client: client.o $(OBJS)
	$(CC) -o $@ $^ $(LIBS) 

server: server.o $(OBJS)
	$(CC) -o $@ $^ $(LIBS) 

stcp_echo_server: $(ECHO_SERVER_OBJS) $(VNS_GLUE)
	$(CC) $(CFLAGS) -o $@ $^ $(VNS_LIBS) $(STCPLIB)

stcp_echo_server.purify: $(ECHO_SERVER_OBJS) $(VNS_GLUE)
	$(PURIFY) $(CC) $(CFLAGS) -o $@ $^ $(VNS_LIBS) $(STCPLIB)

stcp_echo_client: $(ECHO_CLIENT_OBJS) $(VNS_GLUE)
	$(CC) $(CFLAGS) -o $@ $^ $(VNS_LIBS) $(STCPLIB)

stcp_echo_client.purify: $(ECHO_CLIENT_OBJS) $(VNS_GLUE)
	$(PURIFY) $(CC) $(CFLAGS) -o $@ $^ $(VNS_LIBS) $(STCPLIB)

depend: dependinit \
        $(addprefix depend_,$(basename $(DEPEND_SRCS) $(PROXY_SRCS)))
	mv ${MAKEFILE}.new ${MAKEFILE}

dependinit:
	sed -e '/^#START DEPS/,$$d' ${MAKEFILE} > ${MAKEFILE}.new
	echo '#START DEPS - Do not change this line or anything after it.' >> \
	     ${MAKEFILE}.new

dist-clean: clean
	rm -f .*.swp stub.tar.gz

dist: dist-clean
	tar zcvf stcp.tgz .

#START DEPS - Do not change this line or anything after it.
transport.o: transport.c mysock.h stcp_api.h transport.h
mysock_api.o: mysock_api.c mysock.h mysock_impl.h network_io.h \
  connection_demux.h
stcp_api.o: stcp_api.c mysock.h mysock_impl.h network_io.h stcp_api.h \
  network.h connection_demux.h tcp_sum.h transport.h
mysock.o: mysock.c mysock.h mysock_impl.h network_io.h stcp_api.h \
  transport.h
network.o: network.c mysock_impl.h mysock.h network_io.h network.h \
  transport.h
connection_demux.o: connection_demux.c mysock_impl.h mysock.h \
  network_io.h mysock_hash.h transport.h connection_demux.h
tcp_sum.o: tcp_sum.c mysock_impl.h mysock.h network_io.h transport.h \
  tcp_sum.h
network_io.o: network_io.c mysock_impl.h mysock.h network_io.h
network_io_tcp.o: network_io_tcp.c mysock_impl.h mysock.h network_io.h \
  network_io_socket.h
network_io_socket.o: network_io_socket.c mysock_impl.h mysock.h \
  network_io.h network_io_socket.h connection_demux.h mysock_impl.h \
  mysock.h network_io.h connection_demux.h transport.h tcp_sum.h \
  mysock_hash.h
echo_server_main.o: echo_server_main.c mysock.h
echo_client_main.o: echo_client_main.c mysock.h
server.o: server.c mysock.h
client.o: client.c mysock.h
