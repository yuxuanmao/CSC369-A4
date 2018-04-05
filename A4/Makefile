FLAGS = -Wall 
DEPENDENCIES = ext2.h ext2_share.h

all: ext2_cp ext2_mkdir

ext2_cp: ext2_cp.o 
	gcc ${FLAGS} -o $@ $^

ext2_mkdir: ext2_mkdir.o 
	gcc ${FLAGS} -o $@ $^

%.o: %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean:  
	rm -rf *.o 
	rm -rf ext2_cp ext2_mkdir*~