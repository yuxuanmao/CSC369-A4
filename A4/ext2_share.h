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

/*
This function is designed to be able to adapt 3 types of block pointers
(direct/single/double/triple indirect) when it comes to analyze them.
param: name: name of target
param: pointers: array of block pointers
param: iterate_time: the number of times iterating for loop
param: recursive_time: the number of times recursively calling the this function.

return: ext_dir_entry*: if we found target, it will not be NULL;
*/
struct ext_dir_entry* iterate_search(char* name, int* pointers, int iterate_time, int recursive_time){
  struct ext_dir_entry* dir_entry;

  //search each block pointer
  for(int i=0; i<iterate_time; i++){
    //if the recursive_time is larger than 0 but smaller than 3, it is likely that a user wants to analyze double/triple
    //indirect pointer. If a user wants to analyze direct/single indirect pointer, this function will not recurse.
    if(recursive_time >= 1 && recursive_time <= 2){
      dir_entry = iterate_search(name, (int *)(disk + pointers[i] * EXT2_BLOCK_SIZE), iterate_time, recursive_time--);
      return dir_entry;
    }else if(recursive_time == 0){
      //From now, we actually determine the each pointer in array.
      if(name != NULL && pointers[j] == 0){
        //if name is not null but there is no more direct pointer then return NULL
        return NULL;
      }
      //for each found direct pointer, we look the inside of block
      dir_entry = search_block(name, pointers[j]);
      if(dir_entry != NULL){
        return dir_entry;
      }
    }else{
      perror("iterate_search: invalid recursive_time value");
      exit(-1);
    }
  }

}

struct ext2_dir_entry* search_inode(char* name, struct ext2_inode* inode ){
  struct ext2_dir_entry* dir_entry = NULL;
  //we search i_block[15]
    //search direct block Pointers first
    //First 12 elements are direct pointers and we request no recursion
    dir_entry = iterate_search(name, (inode->i_block), 12, 0);

    if(dir_entry == NULL){
      //when we did not found name at direct pointers, then we now look indirect Pointers
      //each block pointer has the size of 4 bytes so we divide it by 1024 bytes,
      //which is the size of one block => 256 block pointers
        //from (inode->i_block)[12] we get the block number
      int* single_indirect = (disk + (inode->i_block)[12] * 1024);
      dir_entry = iterate_search(name, single_indirect, 256, 0);
    }

    if(dir_entry == NULL){
      //similar to the single indirect pointer, we set iterate_time = 256
      //to search the double indirect pointer
      int* double_indirect = (disk + (inode->i_block)[13] * 1024);
      dir_entry = iterate_search(name, double_indirect, 256, 1);
    }

    if(dir_entry == NULL){
      //similar to the single indirect pointer, we set iterate_time = 256
      //to search the triple indirect pointer
      int* triple_indirect = (disk + (inode->i_block)[14] * 1024);
      dir_entry = iterate_search(name, triple_indirect, 256, 2);
    }
  //if we did not found the "name" then we return NULL.
  return dir_entry;
}

struct ext2_dir_entry* search_block(char* name, struct ext2_inode* block){

  return NULL;
}
