
#include "ext2_share.h"
#include "ext2.h"

extern unsigned char *disk;

int main(int argc, char **argv){
	if(argc != 5){
		if(argc != 4){
			fprintf(stderr, "Usage: %s <image file name> [-s] <source file path> <link path>\n", argv[0]);
        	exit(1);
		}
	}else{
		if(strcmp(argv[2], "-s") != 0){
			fprintf(stderr, "Usage: %s <image file name> [-s] <source file path> <link path>\n", argv[0]);
        	exit(1);
		}
	}

	

	return 0;
}