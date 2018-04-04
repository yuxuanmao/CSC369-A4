#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
// #include "ext2_share.h"

#include <string.h>
#include <errno.h>

unsigned char *disk;
unsigned int findParentDirectoryInode(struct ext2_inode* table, unsigned int indx, char* parsed_name);
unsigned int findAvailableInode(unsigned char* inodeBits);


void append(char* s, char c){
    int len = strlen(s);
    s[len] = c;
    s[len+1] = '\0';
}

// returns the number of directories in the path.
int dirDepth(char* path){
  int depth = 0;
  for(int i=0; i<strlen(path); i++){
    if(path[i] == '/' && path[i+1] != '\0'){
      depth ++;
    }
  }
  depth++;
  return depth;
}


int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    char* target_path = argv[2];
    

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

  
    // directory names
    int num_dirs = dirDepth(target_path);
    char dir_names[num_dirs][255];
      //reuseble single dir name storage
    char dir_name[255];
    dir_name[0] = '\0';
    int indx = 0; //this is used for dir_names
    printf("path = %s\n", target_path);
    //Analyze the content of target_path to get each dir name
    for(int i=0; i<strlen(target_path)+1; i++){
      if(target_path[i] == '/' || target_path[i] == '\0'){
        //finished to find one dir name
        strcpy(dir_names[indx], dir_name);
        dir_name[0] = '\0';
        indx ++;
      }else{
        //in the middle of find one dir name
        append(dir_name, target_path[i]);
      }
    }


    //super block
    // struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // printf("Inodes: %d\n", sb->s_inodes_count);
    // printf("Blocks: %d\n", sb->s_blocks_count);
    //group table
    struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
    //inode table
    struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
    // //block bitmap
    // char block_bitmap = disk + 1024*group_table[0].bg_block_bitmap;
    // inode bitmap
    unsigned char* inode_bitmap = (unsigned char*)(disk + 1024*group_table[0].bg_inode_bitmap);

    // //get the beginning of inode bitmap
    // unsigned char* inodeBits = (unsigned char*)(disk + gd[0].bg_inode_bitmap * 0x400);


    // Checking  
    int i;
    
    char* new_dir_name = dir_names[num_dirs-1];
    unsigned int inode_num = 2;

    // only want the path to parent
    for(i=0; i<num_dirs-1; i++){
        char *parsed_name = dir_names[i];
        printf("%s\n", parsed_name);
        inode_num = findParentDirectoryInode(inode_table, inode_num, parsed_name);
        if (inode_num == 0){
            printf("directory not found\n");
            exit(ENOENT);
        }
    }

    // inode_num should now indicate the parent directory of where we would like to place our new directory
    printf("the inode pointing to parent directory is %d\n", inode_num);
    
    // find the smallest nonreserved inode
    unsigned int smallest_inode = findAvailableInode(inode_bitmap);

    // find the smallest block
    
    // write dictionary info into the block, link inode

    // link ditionary block number with inode index

    // update inode bitmap



  

    
    
    return 0;
}



// Helper function to find the smallest available inode (nonereserved) 
unsigned int findAvailableInode(unsigned char* inodeBits){
    printf("inode bits are %s\n", inodeBits);



    return 0;
}







// This is the helper function to print out the directory information
// returns the inode number if mathing directory name is found, or return 0 if unfound 
unsigned int findParentDirectoryInode(struct ext2_inode* table, unsigned int indx, char* parsed_name){
    //get inode for this indx
    struct ext2_inode* inode = table + (indx - 1);
    //get the type of inode
    if (EXT2_S_IFDIR & inode->i_mode){
        // directory
        for (int i=0;i<15; i++){
            if((inode->i_block)[i] == 0){
                // printf("code reached here\n");
                break;
            }else{
                // this is each dir block num we got 
                unsigned int block = (inode->i_block)[i];
                printf("   DIR BLOCK NUM: %d (for inode %d)\n", block, indx);
                // accumulator for the total size of the linked list, as the size cannot exceed block size
                int index = 0;
                while (index < EXT2_BLOCK_SIZE){
                    struct ext2_dir_entry *entry = (struct ext2_dir_entry*)(disk + 0x400 * block + index);
                    // determine the type of the file
                    char type;
                    char* name;
                    struct ext2_inode* inode = table + entry->inode-1;

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

                    if(type == 'd'){
                        //directory  
                        name = (char*)(disk + 0x400 * block + index +sizeof(struct ext2_dir_entry));
                        printf("name is %s and type is %c\n", name, type);
                        if (strcmp(name, parsed_name) == 0){
                            printf("name is %s\n", name);
                            printf("parsed_name is %s\n", parsed_name);
                            printf("found matching directory!\n");
                            // return the inode related to this directory name
                            printf("inode index is %d\n", entry->inode);
                            return entry->inode;
                        }
                    }
                    index += entry->rec_len;
                }
            }
        }
    } 
    return 0;  
};




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