#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

#include <string.h>

unsigned char *disk;


int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    char* path = argv[2];
    

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    printf("path is %s\n", path);
    char* dir_name = strtok(path, "/");
    while (dir_name != NULL){
      printf( " %s\n", dir_name);
      // dir_name = strtok(NULL, "/");
    }


    // code brought back from excercise7
    // struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // printf("Inodes: %d\n", sb->s_inodes_count);
    // printf("Blocks: %d\n", sb->s_blocks_count);
    
    return 0;
}



//  * break up path string to an array of directory names
 

// char*[] process_path(char* path){




// }











//This is the helper function to print out inode information
//we assume indx start from 1 so actual index of inode in table is indx -1
void printInodeDetail(struct ext2_inode* table, unsigned int indx){
    //get inode for this indx
    struct ext2_inode* inode = table + (indx - 1);
    //get the type of inode
    char type;
    if(EXT2_S_IFREG & inode->i_mode){
        //regular file
        type = 'f';
    }else if(EXT2_S_IFDIR & inode->i_mode){
        //directory
        type = 'd';
    }else{
        //symlink or other
        type = '0';
    }
    //print out info
    printf("[%d] type: %c size: %d links: %d blocks: %d\n", 
        indx, type, inode->i_size, inode->i_links_count, inode->i_blocks);
    
    //print actual block number in use by that file/directory
    printf("[%d] Blocks:", indx);
    for(int i = 0; i < 15; i++){
        if((inode->i_block)[i] == 0){
            break;
        }else{
            printf(" %d", (inode->i_block)[i]);
        }
    }
    printf("\n");
}