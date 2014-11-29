include var.mk

CC = gcc

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a -lpthread

FLAGS = -g  -O2 -I/home/courses/cse533/Stevens/unpv13e/lib

all: ${LOGIN}_tour  ${LOGIN}_arp
	
prhwaddrs: prhwaddrs.o get_hw_addrs.o 
	${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c

tour.o : tour.c lib.h 
	${CC} ${FLAGS} -DPROTO=${ID} -c tour.c

arp.o : arp.c lib.h
	${CC} ${FLAGS} -DPROTO=${ID} -c arp.c

${LOGIN}_tour : tour.o lib.o get_hw_addrs.o
	${CC} -g -o $@ tour.o lib.o get_hw_addrs.o ${LIBS}

${LOGIN}_arp : arp.o lib.o get_hw_addrs.o
	${CC} -g -o $@ arp.o lib.o get_hw_addrs.o ${LIBS}

lib.o : lib.c lib.h
	${CC} ${FLAGS} -DPROTO=${ID} -c lib.c

clean:
	rm prhwaddrs *.o ${LOGIN}_*

