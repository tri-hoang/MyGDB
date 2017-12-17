.PHONY: all clean

all: mygdb test

mygdb: mygdb.c cmd.c cmd_helper.c
	gcc mygdb.c cmd.c cmd_helper.c -o mygdb -ldwarf -I/usr/include/libdwarf -g

test: test.c
	gcc test.c -o test -g
# cmd.o: 
# 	gcc cmd.c -c -o cmd.o -I/usr/include/libdwarf

clean:
	rm mygdb test