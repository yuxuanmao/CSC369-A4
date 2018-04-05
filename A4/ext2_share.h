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
struct ext2_dir_entry* cp_copy_filename(char* filename, char* target);
struct ext2_dir_entry* search_direntry(struct ext2_inode* inode, char* name);
struct ext2_dir_entry* create_direntry(struct ext2_inode* inode, char* name, unsigned char file_type);
int allocate_inode();
int allocate_block();
int find_free_inode(struct ext2_super_block *sb, struct ext2_group_desc* group_table);
int find_free_block(struct ext2_super_block *sb, struct ext2_group_desc* group_table);
int adjust_dir_size(int input);



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

struct ext2_dir_entry* cp_copy_filename(char* filename, char* target){
  //target has to begin with '/'
  if(target[0] != '/'){
    perror("path has to begin with '/'\n");
    exit(-1);
  }
  int target_length = strlen(target) + 1;
  char* cut_target = target + 1;
  //copy target
  char target_cpy[target_length];
  strncpy(target_cpy, target, target_length);
  //get the first directory
  char* cur_dir = strtok(target_cpy, "/");
  if(cur_dir != NULL){
    //remove the cur_dir from full path
    cut_target += strlen(cur_dir);
  }

  //group table
  struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
  //inode table
  struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);

  //set the inode of current directory
  struct ext2_inode* cur_inode = inode_table + (EXT2_ROOT_INO - 1);
  //find the directory entry of "cur_dir" from cur_inode
  //printf("search_direntry 1\n");
  struct ext2_dir_entry* cur_dir_entry = search_direntry(cur_inode, cur_dir);

  while(cur_dir_entry != NULL){// if cur_dir_entry is null, it did not found the "cur_dir"
    if(cur_dir_entry->file_type == EXT2_FT_DIR){
      //if cur_dir is directory
      if(cut_target[0] == '/')cut_target ++;
    }else{
      //if cur_dir is not directory
      if(strlen(cut_target) != 0){
        perror("invalid path\n");
        exit(ENOENT);
      }else{
        //we reached the file already there?
        perror("existed file\n");
        exit(EEXIST);
      }
    }
    //prepare the next cur_dir
    cur_dir = strtok(NULL, "/");
    if(cur_dir != NULL){
      cut_target += strlen(cur_dir);
    }
    cur_inode = inode_table + (cur_dir_entry->inode - 1);
    //printf("search_direntry 2\n");
    cur_dir_entry = search_direntry(cur_inode, cur_dir);
  }

  if(cur_dir != NULL){
    filename = cur_dir;
  }
  printf("want to create dir entry for %s at %d\n", filename, cur_inode->i_size);
  return create_direntry(cur_inode, filename, EXT2_FT_REG_FILE);
}

struct ext2_dir_entry* search_direntry(struct ext2_inode* inode, char* name){
  if(name == NULL){
    //perror("cannot find directory entry with null name");
    return NULL;
  }

  //name cannot exceed 255 for ext2
  int name_length;
  if(strlen(name) <= 255){
    name_length = strlen(name);
  }else{
    exit(-1);
  }

  for(int i=0; i<12; i++){
    if((inode->i_block)[i] == 0){
      //if pointer is 0 it is invalid
      break;
    }
    unsigned char* block_ptr = disk + (inode->i_block)[i] * EXT2_BLOCK_SIZE;
    unsigned char* cur_ptr = block_ptr;

    while((cur_ptr - block_ptr) < EXT2_BLOCK_SIZE){
      struct ext2_dir_entry* dir_entry = (struct ext2_dir_entry*) cur_ptr;
      // if directory entry is in use, inode number is not 0
      if(dir_entry->inode != 0){
        //check whether this directory entry means "name" given
        if(name_length == dir_entry->name_len && strcmp(name, dir_entry->name) == 0){
          //we found "name"
          return dir_entry;
        }
      }
      //move to the next directory entry
      cur_ptr += dir_entry->rec_len;
    }
  }  
  return NULL;
}

struct ext2_dir_entry* create_direntry(struct ext2_inode* inode, char* name, unsigned char file_type){
  //check whether "name" already exists or not
  if(search_direntry(inode, name) != NULL){
    printf("there is already %s\n", name);
    exit(EEXIST);
  }

  int name_length = strlen(name);
  int new_size = adjust_dir_size(8+ name_length);

  for(int i=0; i<12; i++){
    if(inode->i_block[i] == 0){
      //if we did not find pointer that has a space, then we create a
      //new block till find 0, then we make new block
      int new_block_num = allocate_block();

      //update the block properties
      inode->i_block[i] = new_block_num;
      inode->i_size += EXT2_BLOCK_SIZE;
      //in i_blocks, each bock is 512 bytes so we calculate out and add to it
      inode->i_blocks = (inode->i_blocks*512 + EXT2_BLOCK_SIZE)/512;

      unsigned char* block_ptr = disk + (new_block_num * EXT2_BLOCK_SIZE);
      struct ext2_dir_entry* dir_entry = (struct ext2_dir_entry*) block_ptr;

      //now fill out directory entry properties.
      //the size of new directory entry will take all rest of empty space to rec_len 
      //as this block is the last block in array
      int new_inode_num = allocate_inode();
      dir_entry->inode = new_inode_num;
      dir_entry->rec_len = EXT2_BLOCK_SIZE;
      dir_entry->name_len = name_length;
      dir_entry->file_type = file_type;
      strncpy(dir_entry->name, name, name_length);
      //printf("created block for %s\n", name);
      return dir_entry;
    }

    unsigned char* block_ptr = disk + (inode->i_block)[i] * EXT2_BLOCK_SIZE;
    unsigned char* cur_ptr = block_ptr;

    while((cur_ptr - block_ptr) < EXT2_BLOCK_SIZE){
      struct ext2_dir_entry* dir_entry = (struct ext2_dir_entry*) cur_ptr;

      if(dir_entry->inode == 0 && new_size <= dir_entry->rec_len){
        //if inode number is 0, then this dir_entry is not in use.
        //and if the size of this dir_entry is large enough, we can use this one
        int new_inode_num = allocate_inode();
        //struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
        //struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
        //struct ext2_inode* new_inode = (struct ext2_inode*)(inode_table + (new_inode_num - 1));
        //new_inode->i_links_count

        //update the inode properties
        dir_entry->inode = new_inode_num;
        dir_entry->rec_len = new_size;
        dir_entry->name_len = name_length;
        dir_entry->file_type = file_type;
        strncpy(dir_entry->name, name, name_length);
        //printf("found block for %s\n", name);
        return dir_entry;
      }else{
        //find whether it is possible to insert between two directory entry
        //calculate the actual directory entry size of current dir_entry
        int dir_entry_size = adjust_dir_size(8 + dir_entry->name_len);

        //if there is a space in dir_entry that is large enough to insert "name"
        if(new_size <= (dir_entry->rec_len - dir_entry_size)){
          //get the size of this "name"
          int actual_size = (dir_entry->rec_len - dir_entry_size);
          //now we create a new dir_entry by using this space
          //struct ext2_dir_entry* pre_dir_entry = dir_entry;
          //resize dir_entry
          dir_entry->rec_len = dir_entry_size;
          //move on to the new dir_entry
          cur_ptr = cur_ptr + dir_entry_size;
          dir_entry = (struct ext2_dir_entry*)cur_ptr;

          //assign information in new dir_entry
          int new_inode_num = allocate_inode();
          //update the inode properties
          dir_entry->inode = new_inode_num;
          dir_entry->rec_len = actual_size;
          dir_entry->name_len = name_length;
          dir_entry->file_type = file_type;
          strncpy(dir_entry->name, name, name_length);
          //printf("found a space for %s at inode = %d, name=%s, size=%d\n", name, pre_dir_entry->inode, pre_dir_entry->name, pre_dir_entry->rec_len);
          return dir_entry;
        }
      }
      //update the cur_ptr to check the next pointer
      cur_ptr = cur_ptr + dir_entry->rec_len;
    }
  }

  //no more memory
  exit(ENOMEM);

}

int allocate_inode(){
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
  struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
  //unsigned char* inode_bitmap = (unsigned char*)(disk + 1024*group_table[0].bg_inode_bitmap);
  //int inodes_count = sb->s_inodes_count;
  int inode_num = find_free_inode(sb, group_table);

  //make sure to clean out the content in inode_num
  struct ext2_inode* new_inode = inode_table + (inode_num - 1);
  memset(new_inode, 0, sizeof(struct ext2_inode));
  //printf("found inode: %d for use\n", inode_num);
  return inode_num;
}

int allocate_block(){
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
  //unsigned char* block_bitmap = (unsigned char*)(disk + 1024*group_table[0].bg_block_bitmap);
  //int blocks_count = sb->s_blocks_count;
  int block_num = find_free_block(sb, group_table);

  //make sure to clean out the content in block_num
  unsigned char* new_block_ptr = disk + block_num * EXT2_BLOCK_SIZE;
  memset(new_block_ptr, 0, EXT2_BLOCK_SIZE);
  //printf("found block: %d for use\n", block_num);
  return block_num;
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
            (*inodeBits)|=bit;
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
                (*blockBits)|=bit;
                sb->s_free_blocks_count --;
                group_table->bg_free_blocks_count --;
                return count + 1;
            }
            count++;
        }
    }
    return -1;
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
  printf("entered iterate search with iterate_time = %d and recursive time = %d\n", iterate_time, recursive_time);
  struct ext2_dir_entry* dir_entry;

  //search each block pointer
  for(int i=0; i<iterate_time; i++){
    //if the recursive_time is larger than 0 but smaller than 3, it is likely that a user wants to analyze double/triple
    //indirect pointer. If a user wants to analyze direct/single indirect pointer, this function will not recurse.
    if(recursive_time >= 1 && recursive_time <= 2){
      dir_entry = iterate_search(name, (int *)(disk + pointers[i] * EXT2_BLOCK_SIZE), iterate_time, recursive_time-1, flag);
      return dir_entry;
    }else if(recursive_time == 0){
      //From now, we actually determine the each pointer in array.
      if(name != NULL && pointers[i] == 0){
        printf("iterate_searc return NULL\n");
        //if name is not null but there is no more direct pointer then return NULL
        return NULL;
      }
      //for each found direct pointer, we look the inside of block
      dir_entry = search_block(name, pointers[i], flag);
      printf("finished to serach block\n");
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
  printf("entered search_inode\n");
  struct ext2_dir_entry* dir_entry = NULL;
  //we search i_block[15]
    //search direct block Pointers first
    //First 12 elements are direct pointers and we request no recursion
    dir_entry = iterate_search(name, (int*)(inode->i_block), 12, 0, flag);
    printf("finished to search direct pointers\n");

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
  printf("entered search_block with name = %s, block = %d, flag = %d\n", name, block, flag);
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
      printf("entered GET_SPACE");
      //user called this function to get the space but not finding
      if(((struct ext2_dir_entry*)cur_ptr)->rec_len - cur_dir_size >= adjust_dir_size(8 + strlen(name))){
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
      if(((struct ext2_dir_entry*)cur_ptr)->name_len == strlen(name) && strcmp(((struct ext2_dir_entry*)cur_ptr)->name, name) == 0){
        printf("we found the same directory entry\n");
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
