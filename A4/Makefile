ext2_cp :  ext2_cp.o
	gcc -Wall -g -o ext2_cp $^

%.o : %.c ext2.h ext2_share.h
	gcc -Wall -g -c $<

clean :
	rm -f *.o ext2_cp *~
