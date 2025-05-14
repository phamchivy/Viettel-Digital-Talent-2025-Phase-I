# Makefile for Paging Simulation

CC = gcc
CFLAGS = -Wall
TARGETS = ue gnb amf

all: $(TARGETS)

ue: UE.c
	$(CC) $(CFLAGS) -o ue UE.c

gnb: gNodeB.c
	$(CC) $(CFLAGS) -o gnb gNodeB.c -lpthread

amf: AMF.c
	$(CC) $(CFLAGS) -o amf AMF.c

clean:
	rm -f $(TARGETS)