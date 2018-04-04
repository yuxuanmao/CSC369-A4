
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

    //now we put actual data in filename
    // //group table
    struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
    //inode table
    struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
    struct ext2_inode* inode = inode_table + copied_filename->inode - 1;

    //get a block to put data and data of file may reach indirect blocks
    int num_read = 0;
    unsigned char buf[EXT2_BLOCK_SIZE];
    for(int i=0; i<12; i++){
      if((num_read = fread(buf, 1, EXT2_BLOCK_SIZE, fp)) > 0){
        int new_block_num = allocate_block();
        inode->i_block[i] = new_block_num;
        inode->i_blocks = (inode->blocks*512 + EXT2_BLOCK_SIZE)/512;
        //add buf to block memory
        unsigned char* block_ptr = disk + new_block_num * EXT2_BLOCK_SIZE;
        memcpy(block_ptr, buf, num_read);
      }
    }
    for(int i=12; i<15; i++){
      if((num_read = fread(buf, 1, EXT2_BLOCK_SIZE, fp)) > 0){
        //we did not finished to read file so now, we use indirect block pointers
        int new_indirect_block_num = allocate_block();
        inode->i_block[i] = new_indirect_block_num;
        inode->i_blocks = (inode->blocks*512 + EXT2_BLOCK_SIZE)/512;
        unsigned int* indirect_block_ptr = disk + new_indirect_block_num * EXT2_BLOCK_SIZE;
        //this is single indirect pointer
        for(int j=0; j<256; j++){
          //for each indirect pointer it has 256 direct pointers
          //as 1024/4bytes = 256
          int second_direct_block_num = allocate_block();
          //add this direct block num to indirect block
          *indirect_block_ptr = second_direct_block_num;
          //toggle indirect block ptr for next data
          indirect_block_ptr++;
          inode->i_blocks = (inode->blocks*512 + EXT2_BLOCK_SIZE)/512;
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
    
    // //open file
    // char filename[255];
    // int file_indx = argv[2] -1;
    // while(file_indx >= 0){
    //   if(argv[2][file_indx] != '/'){
    //     file_indx --;
    //   }else{
    //     break;
    //   }
    // }
    // for(int i=file_indx+1; i<strlen(argv[2]); i++){
    //   append(filename, argv[2][i]);
    // }
    // FILE* fp = fopen(argv[2], "r");
    // if(fp == NULL){
    //   perror("Failed to open the file");
    //   exit(-1);
    // }

    // //cut target path to each directory name and store it in a list.
    // char* target_path = argv[3];
    // int total_dir_length = dirDepth(target_path);
    //   //initialize dir list
    // char** dir_names;
    // dir_names = malloc(sizeof(char*)*total_dir_length);
    // for(int i=0; i<total_dir_length; i++){
    //   dir_names[i] = malloc(sizeof(char)*255);
    // }
    //   //reuseble single dir name storage
    // char dir_name[255];
    // dir_name[0] = '\0';
    // int indx = 0; //this is used for dir_names
    // printf("path = %s\n", target_path);
    // //Analyze the content of target_path to get each dir name
    // for(int i=0; i<strlen(target_path)+1; i++){
    //   if(target_path[i] == '/' || target_path[i] == '\0'){
    //     //finished to find one dir name
    //     strncpy(dir_names[indx], dir_name, strlen(dir_name));
    //     dir_name[0] = '\0';
    //     indx ++;
    //   }else{
    //     //in the middle of find one dir name
    //     append(dir_name, target_path[i]);
    //   }
    // }
    // for(int i=0; i<total_dir_length; i++){
    //   printf("%s\n", dir_names[i]);
    // }

    // //super block
    // struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // printf("Inodes: %d\n", sb->s_inodes_count);
    // printf("Blocks: %d\n", sb->s_blocks_count);
    // //group table
    // struct ext2_group_desc* group_table = (struct ext2_group_desc*)(disk + 1024*2);
    // //inode table
    // struct ext2_inode* inode_table = (struct ext2_inode*)(disk + 1024*group_table[0].bg_inode_table);
    // //block bitmap
    // char* block_bitmap = (char*)(disk + 1024*group_table[0].bg_block_bitmap);
    // //inode bitmap
    // char* inode_bitmap = (char*)(disk + 1024*group_table[0].bg_inode_bitmap);

    // //check root dir from target_path to get inode
    // int dir_inode = EXT2_ROOT_INO - 1;
    // // if(strcmp(dir_names[0], ".") != 0){
    // //   //if the root of target_path is not ".", we try to cd to it
    // //
    // // }
    // dir_inode = get_parent_dir(dir_names, total_dir_length, inode_table);
    // printf("parent directory inode is = %d\n", dir_inode);
    // if(dir_inode == -ENOENT){
    //   printf("found error at get parent dir");
    //   exit(ENOENT);
    // }

    // //check whether the file is already existed in our target_path
    // printf("===================================================\n");
    // struct ext2_dir_entry* possible_dup = search_inode(filename, inode_table + dir_inode, FIND_NAME);
    // printf("finished to search inode\n");
    // if(possible_dup != NULL){
    //   //there is already a file with the same name as filename.
    //   printf("found the file is already existed");
    //   exit(EEXIST);
    // }

    // //Now, we begin to find space and put Data of file
    //   //get the space
    //   struct ext2_dir_entry* file_space = search_inode(filename, inode_table + dir_inode, GET_SPACE);
    //   file_space->name_len = strlen(filename);
    //   file_space->file_type = EXT2_FT_REG_FILE;
    //   strncpy(file_space->name, filename, strlen(filename));

    //   //set the inode
    //   int inode_num = find_free_inode(sb, group_table);

    


    return 0;
}
