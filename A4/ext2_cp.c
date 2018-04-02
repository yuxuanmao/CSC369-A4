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

struct ext2_dir_entry* search_inode(char* name, struct ext2_inode* inode ){
  struct ext2_dir_entry* dir_entry = NULL;
  return NULL;
}

void append(char* s, char c){
    int len = strlen(s);
    s[len] = c;
    s[len+1] = '\0';
}

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

    if(argc != 4) {
        fprintf(stderr, "Usage: %s <image file name> <source file path> <target path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    //open file
    FILE* fp = fopen(argv[2], "r");
    if(fp == NULL){
      perror("Failed to open the file");
      exit(-1);
    }

    //cut target path to each directory name and store it in a list.
    char* target_path = argv[3];
      //initialize dir list
    char dir_names[dirDepth(target_path)][255];
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
    for(int i=0; i<dirDepth(target_path); i++){
      printf("%s\n", dir_names[i]);
    }

    //super block
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    //group table
    struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
    //inode table
    struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
    //block bitmap
    char block_bitmap = disk + 1024*group_table[0].bg_block_bitmap;
    //inode bitmap
    char inode_bitmap = disk + 1024*group_table[0].bg_inode_bitmap;

    //check root dir from target_path to get inode
    int root_inode = EXT2_ROOT_INO - 1;
    if(strcmp(dir_names[0], ".") != 0){
      //if the root of target_path is not ".", we try to cd to it

    }

    return 0;
}
