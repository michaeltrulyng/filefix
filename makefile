# makefile for filefix.c utility

CC = gcc
CFLAGS = -g -Wall

default: compile

compile: filefix
	@echo compiling programs

filefix: filefix.o
	$(CC) $(CFLAGS) -o filefix filefix.o

filefix.o: filefix.c filelib.h
	$(CC) $(CFLAGS) -c filefix.c
# make -B will force compile (even if file is up-to-date)

clean:
	$(RM) filefix *.o *~

run:
	./filefix.sh 