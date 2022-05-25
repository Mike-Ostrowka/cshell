
CC = gcc
CFLAGS = -g -Wall

all: 	cshell
		
cshell: 	cshell.c
				$(CC) $(CFLAGS) -o cshell cshell.c

clean:
	$(RM) *.o cshell
