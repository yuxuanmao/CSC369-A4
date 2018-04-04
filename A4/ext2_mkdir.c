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
int find_free_inode(struct ext2_super_block *sb, struct ext2_group_desc* group_table);
int find_free_block(struct ext2_super_block *sb, struct ext2_group_desc* group_table);
void write_new_block(unsigned int block_num, char *new_dir_name, unsigned int inode, unsigned int parentInode);



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
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // printf("Inodes: %d\n", sb->s_inodes_count);
    // printf("Blocks: %d\n", sb->s_blocks_count);

    //group table
    struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
    //inode table
    struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
    //block bitmap
    // char block_bitmap = disk + 1024*group_table[0].bg_block_bitmap;
    // inode bitmap
    // char* inode_bitmap = (char*)(disk + 1024*group_table[0].bg_inode_bitmap);

   


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
    
    // find the smallest nonreserved inode and make this inode unavailable
    int smallest_inode = find_free_inode(sb, group_table);
    printf("the smallest inode available is %d\n", smallest_inode);
    if (smallest_inode == -1){
        perror("No more free inodes");
    }


    // find the smallest free block
    int smallest_block = find_free_block(sb, group_table);
    printf("the smallest block available is %d\n", smallest_block);
    if (smallest_block == -1){
            perror("No more free blocks");
    }

    
    // write the dict struct into the new block, link inode
    write_new_block((unsigned int) smallest_block, new_dir_name, (unsigned int) smallest_inode, (unsigned int) inode_num);


    // modify the parent directory to link to the new directory

    // link ditionary block number with inode index


    // update inode bitmap



  

    
    
    return 0;
}



// write dictionary with name and related inode into smallest block,
void write_new_block(unsigned int block_num, char *new_dir_name, unsigned int inode, unsigned int parentInode){
    // index of the entry struct in the 

    struct ext2_dir_entry *selfentry; 
    struct ext2_dir_entry *parententry; 

    selfentry->inode = inode;
    selfentry->name[0] = '.';
    selfentry->name[1] = '\0';
    selfentry->file_type = 'd';
    selfentry->name_len = '1';
    // 8+1+3
    selfentry->rec_len = 12;

    (struct ext2_dir_entry*)(disk + 0x400 * block_num) = selfentry;

    parententry->inode = parentInode;
    parententry->name[0] = '.';
    parententry->name[1] = '.';
    parententry->name[2] = '\0';
    parententry->file_type = 'd';
    parententry->name_len = '2';
    // 8 + 2 + 2
    parententry->rec_len = 12;

    (struct ext2_dir_entry*)(disk + 0x400 * block_num + selfentry->rec_len) = parententry;
}



int find_free_inode(struct ext2_super_block *sb, struct ext2_group_desc* group_table){
    int count = 0;
    //get the beginning of inode bitmap
    unsigned char* inodeBits = (unsigned char*)disk + group_table[0].bg_inode_bitmap * 0x400;
    //get the end of inode bitmap
    unsigned char* inodeLimit = (unsigned char*)disk + group_table[0].bg_inode_bitmap * 0x400 + ((sb->s_inodes_count) >> 3);
    for(; inodeBits<inodeLimit; ++inodeBits){
        for(unsigned int bit=1; bit<=0x80; bit<<=1){
            //shift left for one means bit * 2
            if(count > 11 && (((*inodeBits)&bit) > 0) == 0){
                sb->s_free_inodes_count --;
                group_table->bg_free_inodes_count --;
                return count + 1;
            }
            count ++;
        }
    }
    return -1;
}



int find_free_block(struct ext2_super_block *sb, struct ext2_group_desc* group_table){
    int count = 0;
    //get the beginning of block bitmap
    unsigned char* blockBits = (unsigned char*)disk + group_table[0].bg_block_bitmap * 0x400;
    //get the end of block bitmap
    unsigned char* blockLimit = (unsigned char*)disk + group_table[0].bg_block_bitmap * 0x400 + ((sb->s_blocks_count) >> 3);
    for(; blockBits<blockLimit; ++blockBits){
        for (unsigned int bit=1; bit<=0x80; bit<<=1){
            //shift left for one means bit * 2
            if((((*blockBits)&bit) > 0) == 0){
                sb->s_free_blocks_count --;
                group_table->bg_free_blocks_count --;
                return count + 1;
            }
            count++;
        }
    }
    return -1;
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

                    if(EXT2_S_IFDIR & inode->i_mode){
                        //directory
                        type = 'd';
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