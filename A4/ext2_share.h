#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

//used to add character to char array
void append(char* s, char c){
    int len = strlen(s);
    s[len] = c;
    s[len+1] = '\0';
}

//used to determine the depth level of given path
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

struct ext_dir_entry* iterate_search(char* name, struct ext2_inode* inode, int start_point, int times){
  int* pointers;
  if(start_point == 0){
    pointers = inode->i_block;
  }else if(start_point == 12){
    pointers = (int*)(disk + (inode->i_block)[start_point] * 1024);
  }else if(start_point == 13){
    pointers = (int*)(disk + (inode->i_block)[start_point] * 1024);
  }
}


struct ext2_dir_entry* search_inode(char* name, struct ext2_inode* inode ){
  struct ext2_dir_entry* dir_entry = NULL;
  //we search i_block[15]
    //search direct block Pointers first
    for(int i=0; i<12; i++){
      if(name != NULL && (inode->i_block)[i] == 0){
        //if name is not null but there is no more direct pointer then return NULL
        return NULL;
      }
      //for each found direct pointer, we look the inside of block
      dir_entry = search_block(name, (inode->i_block)[i]);
      if(dir_entry != NULL){
        return dir_entry;
      }
    }

    if(dir_entry == NULL){
      //when we did not found name at direct pointers, then we now look indirect Pointers
      //each block pointer has the size of 4 bytes so we divide it by 1024 bytes, which is the size of one block => 256 block pointers
        //from (inode->i_block)[12] we get the block number
      int* single_indirect = (disk + (inode->i_block)[12] * 1024);
      //search block pointers in the block pointed by the single indirect pointer
      for(int i=0; i<256; i++){
        if(name != NULL && (single_indirect[i] == 0){
          return NULL;
        }
        //for each found pointer, we look the inside of block
        dir_entry = search_block(name, single_indirect[i]);
        if(dir_entry != NULL){
          return dir_entry;
        }
      }
    }

  return NULL;
}

struct ext2_dir_entry* search_block(char* name, struct ext2_inode* block){

  return NULL;
}
