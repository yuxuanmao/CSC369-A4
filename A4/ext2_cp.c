
#include "ext2_share.h"
#include "ext2.h"

extern unsigned char *disk;

int get_parent_dir(char** dir_names, int lst_length, struct ext2_inode* inode_table){
  printf("enter get_parent_dir\n");
  //set the root Directory
  struct ext2_inode* root_inode = inode_table + (EXT2_ROOT_INO - 1);
  int i = 0;
  printf("before check dir_names\n");
  if(strcmp(dir_names[0], ".") == 0){
    i = 1;
  }
  printf("before enter while\n");
  while(i < lst_length){
    //check whether the name is directory or not
    if(S_ISDIR(root_inode->i_mode) == 0){
      perror("get_root_dir: not directory");
      exit(-1);
    }
    printf("checked inode mode\n");
    if(i >= lst_length-1){
      //this is end of directory list
      struct ext2_dir_entry* sub_dir = search_inode(dir_names[i], root_inode, FIND_NAME);
      printf("finished to search_inode\n");
      if(sub_dir == NULL){
        printf("exit get_parent_dir\n");
        return -ENOENT;
      }
      printf("exit get_parent_dir\n");
      return sub_dir->inode + (EXT2_ROOT_INO - 1);
    }else{
      //not end of directory list
      struct ext2_dir_entry* sub_dir = search_inode(dir_names[i], root_inode, FIND_NAME);
      printf("finished to search_inode\n");
      if(sub_dir == NULL){
        printf("exit get_parent_dir\n");
        return -ENOENT;
      }
      root_inode = inode_table + (sub_dir->inode - 1);
      i++;
    }
  }
  printf("exit get_parent_dir\n");
  return -2;
}



char* get_filename(char* full_path){
  int start = 0;
  for(int i = strlen(full_path)-1; i>=0; i--){
      if(full_path[i] == '/'){
          start = (i + 1);
          //printf("%d\n", start);
          break;
      }
  }
  return full_path + start;
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

    FILE* fp = fopen(argv[2], "r");
    if(fp == NULL){
      perror("Failed to open the file");
      exit(-1);
    }

    char* filename = get_filename(argv[2]);

    struct ext2_dir_entry* copied_filename = cp_copy_filename(filename, argv[3]);
    //printf("got dir entry for %s\n", filename);
    //printf("inode = %d, rec_len=%d, name_len=%d, file_type=%d\n", copied_filename->inode, copied_filename->rec_len, copied_filename->name_len, copied_filename->file_type);

    //now we put actual data in filename
    // //group table
    struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
    //inode table
    struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
    struct ext2_inode* inode = inode_table + copied_filename->inode - 1;
    inode->i_mode = EXT2_S_IFREG;
    inode->i_links_count ++;


    //get a block to put data and data of file may reach indirect blocks
    int num_read = 0;
    unsigned char buf[EXT2_BLOCK_SIZE];
    for(int i=0; i<12; i++){
      if((num_read = fread(buf, 1, EXT2_BLOCK_SIZE, fp)) > 0){
        int new_block_num = allocate_block();
        inode->i_block[i] = new_block_num;
        inode->i_blocks = (inode->i_blocks*512 + EXT2_BLOCK_SIZE)/512;
        //add buf to block memory
        unsigned char* block_ptr = disk + new_block_num * EXT2_BLOCK_SIZE;
        memcpy(block_ptr, buf, num_read);
        inode->i_size += num_read;
      }
    }
    //printf("finished to write data to direct pointers\n");
    for(int i=12; i<15; i++){
      if((num_read = fread(buf, 1, EXT2_BLOCK_SIZE, fp)) > 0){
        //printf("need indirect pointers to fill rest of data\n");
        //we did not finished to read file so now, we use indirect block pointers
        int new_indirect_block_num = allocate_block();
        inode->i_block[i] = new_indirect_block_num;
        inode->i_blocks = (inode->i_blocks*512 + EXT2_BLOCK_SIZE)/512;
        unsigned int* indirect_block_ptr = (unsigned int*)(disk + new_indirect_block_num * EXT2_BLOCK_SIZE);
        //this is single indirect pointer
        for(int j=0; j<256; j++){
          //for each indirect pointer it has 256 direct pointers
          //as 1024/4bytes = 256
          int second_direct_block_num = allocate_block();
          //add this direct block num to indirect block
          *indirect_block_ptr = second_direct_block_num;
          //toggle indirect block ptr for next data
          indirect_block_ptr++;
          inode->i_blocks = (inode->i_blocks*512 + EXT2_BLOCK_SIZE)/512;
          //write the data into block
          unsigned char* second_direct_block_ptr = disk + second_direct_block_num * EXT2_BLOCK_SIZE;
          memcpy(second_direct_block_ptr, buf, num_read);
          //increase the size by bytes
          inode->i_size += num_read;
        }

        if(i==13){
          //double indirect
          printf("want to write data to double indirect\n");  

        }else{
          //triple indirect
          printf("want to write data to triple indirect\n");
        }
      }
    }
    
    fclose(fp);

    


    return 0;
}
