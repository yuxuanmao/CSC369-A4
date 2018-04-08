#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;


void printInodeDetail(struct ext2_inode* table, unsigned int indx);
void printDirectoryDetail(struct ext2_inode* table, unsigned int indx);

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }


    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    //printf("free inodes: %d\n", sb->s_free_inodes_count);

    struct ext2_group_desc* gd = (struct ext2_group_desc*)(disk + 2048);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd[0].bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd[0].bg_inode_bitmap);
    printf("    inode table: %d\n", gd[0].bg_inode_table);
    printf("    free blocks: %d\n", gd[0].bg_free_blocks_count);
    printf("    free inodes: %d\n", gd[0].bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd[0].bg_used_dirs_count);

    //print bitmap of block and inode
    printf("Block bitmap: ");
    //get the beginning of block bitmap
    unsigned char* blockBits = (unsigned char*)disk + gd[0].bg_block_bitmap * 0x400;
    //get the end of block bitmap
    unsigned char* blockLimit = (unsigned char*)disk + gd[0].bg_block_bitmap * 0x400 + ((sb->s_blocks_count) >> 3);
    for(; blockBits<blockLimit; ++blockBits){  
        for(unsigned int bit=1; bit<=0x80; bit<<=1){
            //shift left for one means bit * 2
            printf("%d", ((*blockBits)&bit) > 0);
        }
        printf(" ");
    }
    printf("\n");

    printf("Inode bitmap: ");

    //get the beginning of inode bitmap
    unsigned char* inodeBits = (unsigned char*)disk + gd[0].bg_inode_bitmap * 0x400;
    //get the end of inode bitmap
    unsigned char* inodeLimit = (unsigned char*)disk + gd[0].bg_inode_bitmap * 0x400 + ((sb->s_inodes_count) >> 3);
    for(; inodeBits<inodeLimit; ++inodeBits){  
        for(unsigned int bit=1; bit<=0x80; bit<<=1){
            //shift left for one means bit * 2
            printf("%d", ((*inodeBits)&bit) > 0);  
        }
        printf(" ");
    }
    printf("\n\n");

    //print root inode information at index 2
    struct ext2_inode* inodeTable = (struct ext2_inode*)(disk + gd[0].bg_inode_table * 0x400);
    printInodeDetail(inodeTable, 2);
    inodeBits = (unsigned char*)disk + gd[0].bg_inode_bitmap * 0x400;
    int inodeIndx = 1;
    //get other inode info from index 12
    for(; inodeBits<inodeLimit; ++inodeBits){  
        for(unsigned int bit=1; bit<=0x80; bit<<=1){
            if(inodeIndx >= 12 && ((*inodeBits)&bit) > 0){
                //we only get info after index 12 based on inode bitmap
                printInodeDetail(inodeTable, inodeIndx);
            }
            inodeIndx ++;
        }
    }

    printf("\n");
    printf("Directory Blocks:\n");
    // print the directories of the root inode
    printDirectoryDetail(inodeTable, 2);
    // print directories related to other inodes
    inodeIndx = 1;
    inodeBits = (unsigned char*)disk + gd[0].bg_inode_bitmap * 0x400;
    for(; inodeBits<inodeLimit; ++inodeBits){  
        for(unsigned int bit=1; bit<=0x80; bit<<=1){
            if(inodeIndx >= 12 && ((*inodeBits)&bit) > 0){
                //we only get info after index 12 based on inode bitmap
                printDirectoryDetail(inodeTable, inodeIndx);
                // printf("code got in here where it should\n");
            }
            inodeIndx ++;
        }
    }



    return 0;
}

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


// This is the helper function to print out the directory information
void printDirectoryDetail(struct ext2_inode* table, unsigned int indx){
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
                    // find the name of the directory or the name of the file
                    char* name;
                    if (type == 'd'){
                         name = (char*)(disk + 0x400 * block + index +sizeof(struct ext2_dir_entry));
                    }else{
                        name = (char*)(disk + 0x400 * block + index +sizeof(struct ext2_dir_entry));
                        name[entry->name_len] = '\0';
                    }


                    printf("Inode: %d rec_len: %d name_len: %d type= %c name=%s\n", entry->inode, entry->rec_len, entry->name_len, type, name);
                    index += entry->rec_len;
                }
            }
        }
    }   
};
