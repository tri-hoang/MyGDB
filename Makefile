CFLAGS=-gdwarf-2 -g -I/usr/include/libdwarf

.PHONY: all clean

all: mygdb test

mygdb: mygdb.c cmd.c cmd_helper.c
	gcc $(CFLAGS) mygdb.c cmd.c cmd_helper.c -ldwarf -lelf -o mygdb

test: test.c
	gcc test.c -o test -g
# cmd.o: 
# 	gcc cmd.c -c -o cmd.o -I/usr/include/libdwarf

clean:
	rm mygdb test