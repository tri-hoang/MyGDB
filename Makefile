.PHONY: all clean

mygdb: mygdb.c cmd.c cmd_helper.c
	gcc mygdb.c cmd.c cmd_helper.c -o mygdb -ldwarf -I/usr/include/libdwarf -g

# cmd.o: 
# 	gcc cmd.c -c -o cmd.o -I/usr/include/libdwarf

clean:
	rm mygdb