#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"

#define GET_SPACE 1
#define FIND_NAME 0

unsigned char *disk;

void append(char* s, char c);
int dirDepth(char* path);
struct ext2_dir_entry* iterate_search(char* name, int* pointers, int iterate_time, int recursive_time, int flag);
struct ext2_dir_entry* search_inode(char* name, struct ext2_inode* inode, int flag);
struct ext2_dir_entry* search_block(char* name, int block, int flag);

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

/*
This function is designed to be able to adapt 3 types of block pointers
(direct/single/double/triple indirect) when it comes to analyze them.
param: name: name of target
param: pointers: array of block pointers
param: iterate_time: the number of times iterating for loop
param: recursive_time: the number of times recursively calling the this function.

return: ext_dir_entry*: if we found target, it will not be NULL;
*/
struct ext2_dir_entry* iterate_search(char* name, int* pointers, int iterate_time, int recursive_time, int flag){
  struct ext2_dir_entry* dir_entry;

  //search each block pointer
  for(int i=0; i<iterate_time; i++){
    //if the recursive_time is larger than 0 but smaller than 3, it is likely that a user wants to analyze double/triple
    //indirect pointer. If a user wants to analyze direct/single indirect pointer, this function will not recurse.
    if(recursive_time >= 1 && recursive_time <= 2){
      dir_entry = iterate_search(name, (int *)(disk + pointers[i] * EXT2_BLOCK_SIZE), iterate_time, recursive_time--, flag);
      return dir_entry;
    }else if(recursive_time == 0){
      //From now, we actually determine the each pointer in array.
      if(name != NULL && pointers[i] == 0){
        //if name is not null but there is no more direct pointer then return NULL
        return NULL;
      }
      //for each found direct pointer, we look the inside of block
      dir_entry = search_block(name, pointers[i], flag);
      if(dir_entry != NULL){
        return dir_entry;
      }
    }else{
      perror("iterate_search: invalid recursive_time value");
      exit(-1);
    }
  }
  return NULL;
}

struct ext2_dir_entry* search_inode(char* name, struct ext2_inode* inode, int flag){
  struct ext2_dir_entry* dir_entry = NULL;
  //we search i_block[15]
    //search direct block Pointers first
    //First 12 elements are direct pointers and we request no recursion
    dir_entry = iterate_search(name, (int*)(inode->i_block), 12, 0, flag);

    if(dir_entry == NULL){
      //when we did not found name at direct pointers, then we now look indirect Pointers
      //each block pointer has the size of 4 bytes so we divide it by 1024 bytes,
      //which is the size of one block => 256 block pointers
        //from (inode->i_block)[12] we get the block number
      int* single_indirect = (int*)(disk + (inode->i_block)[12] * 1024);
      dir_entry = iterate_search(name, single_indirect, 256, 0, flag);
    }

    if(dir_entry == NULL){
      //similar to the single indirect pointer, we set iterate_time = 256
      //to search the double indirect pointer
      int* double_indirect = (int*)(disk + (inode->i_block)[13] * 1024);
      dir_entry = iterate_search(name, double_indirect, 256, 1, flag);
    }

    if(dir_entry == NULL){
      //similar to the single indirect pointer, we set iterate_time = 256
      //to search the triple indirect pointer
      int* triple_indirect = (int*)(disk + (inode->i_block)[14] * 1024);
      dir_entry = iterate_search(name, triple_indirect, 256, 2, flag);
    }
  //if we did not found the "name" then we return NULL.
  return dir_entry;
}

//This is used to check the given directory entry size whether aligned 4 bytes boundaries or not.
int adjust_dir_size(int input){
  if(input%4 == 0){
    //the size of directory entry aligned on 4 bytes boundaries
    return input;
  }else{
    int quotient = input/4;
    return (quotient+1)*4;
  }
}

struct ext2_dir_entry* search_block(char* name, int block, int flag){
  unsigned char* block_ptr = disk + block * EXT2_BLOCK_SIZE;
  unsigned char* cur_ptr = block_ptr;
  //unsigned char* pre_ptr = NULL;
  struct ext2_dir_entry* dir_entry = NULL;

  if(name == NULL && block == 0 && ((struct ext2_dir_entry*)cur_ptr)->rec_len == 0){
    dir_entry = (struct ext2_dir_entry*)cur_ptr;
    return dir_entry;
  }else if(name == NULL){
    perror("search_block: name cannot be NULL");
    exit(-2);
  }

  int cur_dir_size;
  //int pre_dir_size;


  while((cur_ptr - block_ptr) < EXT2_BLOCK_SIZE){//if cur_ptr exceed the block size then we should stop our loop
    //pre_dir_size = cur_dir_size;
    //we calculate the directory entry size based on the rule that
    //The directory entries must be aligned on 4 bytes boundaries
      //the size of directory entry is based on 8 bits information with file/dir name length
    cur_dir_size = 8 + ((struct ext2_dir_entry*)cur_ptr)->name_len;
      //however, we cannot just get the size by just adding file/dir name length, but also
      //we need to adjust the size to the nearest size which is multiple of 4.
    cur_dir_size = adjust_dir_size(cur_dir_size);

    if(flag == GET_SPACE){
      //user called this function to get the space but not finding
      if(((struct ext2_dir_entry*)cur_ptr)->rec_len - cur_dir_size >= strlen(name)){
        //we have enough space to store "name"
          //Now, we readjust the size of cur_ptr by resetting the size to cur_dir_size
        ((struct ext2_dir_entry*)(cur_ptr))->rec_len = cur_dir_size;
          //then, we set the next available space for "name"
        ((struct ext2_dir_entry*)(cur_ptr + cur_dir_size))->rec_len = ((struct ext2_dir_entry*)cur_ptr)->rec_len - cur_dir_size;
        return ((struct ext2_dir_entry*)(cur_ptr + cur_dir_size));
      }
    }else if(flag == FIND_NAME){
      //user called this function to find the name
      //we check the name and length of name from directory entry matches "name"
      if(((struct ext2_dir_entry*)cur_ptr)->name_len == strlen(name)+1 && strcmp(((struct ext2_dir_entry*)cur_ptr)->name, name) == 0){
        return (struct ext2_dir_entry*)cur_ptr;
      }
    }else{
      perror("search_block: invlaid flag");
      exit(-1);
    }

    //we move on by updating cur_ptr
    //pre_ptr = cur_ptr;
    cur_ptr += ((struct ext2_dir_entry*)cur_ptr)->rec_len;
  }

  return NULL;
}
