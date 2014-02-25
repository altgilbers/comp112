CFLAGS= -g

all: messenger help.c help.h help.o 

clean: 
	rm -rf messenger

messenger: messenger.c help.o help.h 
	gcc $(CFLAGS) messenger.c help.o -o messenger -lssl -lpthread

help.o: help.c help.h
	gcc $(CFLAGS) -c help.c 

# link files that might change
#help.c: /comp/112/a/a3/help.c
#	ln -s /comp/112/a/a3/help.c .
#help.h: /comp/112/a/a3/help.h
#	ln -s /comp/112/a/a3/help.h .
#help.o: /comp/112/a/a3/help.o
#	ln -s /comp/112/a/a3/help.o .
#MD5_example.c: /comp/112/a/a3/MD5_example.c
#	ln -s /comp/112/a/a3/MD5_example.c .
#
#test1: /comp/112/a/a3/test1
#	ln -s /comp/112/a/a3/test1 .
#test2: /comp/112/a/a3/test2
#	ln -s /comp/112/a/a3/test2 .
#test3: /comp/112/a/a3/test3
#	ln -s /comp/112/a/a3/test3 .
#test4: /comp/112/a/a3/test4
#	ln -s /comp/112/a/a3/test4 .
#test5: /comp/112/a/a3/test5
#	ln -s /comp/112/a/a3/test5 .
