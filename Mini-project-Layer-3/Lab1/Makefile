# Makefile for SNF sync - Completed 

CC = gcc
CFLAGS = -Wall
TARGETS = ue gnb

all: $(TARGETS)

ue: UE.c
	$(CC) $(CFLAGS) -o ue UE.c

gnb: gNodeB.c
	$(CC) $(CFLAGS) -o gnb gNodeB.c 

clean:
	rm -f $(TARGETS)