
#include "ext2_share.h"
#include "ext2.h"

extern unsigned char *disk;

int get_parent_dir(char** dir_names, int lst_length, struct ext2_inode* inode_table){
  //set the root Directory
  struct ext2_inode* root_inode = inode_table + (EXT2_ROOT_INO - 1);
  int i = 0;
  if(strcmp(dir_names[0], ".") == 0){
    i = 1;
  }

  while(1){
    //check whether the name is directory or not
    if(S_ISDIR(root_inode->i_mode) == 0){
      perror("get_root_dir: not directory");
      exit(-1);
    }

    if(i >= lst_length){
      //this is end of directory list
      struct ext2_dir_entry* sub_dir = search_inode(dir_names[i], root_inode, FIND_NAME);
      if(sub_dir == NULL){
        return -ENOENT;
      }
      return sub_dir->inode + (EXT2_ROOT_INO - 1);
    }else{
      //not end of directory list
      struct ext2_dir_entry* sub_dir = search_inode(dir_names[i], root_inode, FIND_NAME);
      if(sub_dir == NULL){
        return -ENOENT;
      }
      root_inode = inode_table + (sub_dir->inode - 1);
      i++;
    }
  }
}

int find_free_inode(struct ext2_super_block *sb, struct ext2_group_desc* group_table, struct ext2_inode* inode_table, char* inode_bitmap){
  //we do not need to consider reserved inodes
  for(int i=11; i<sb->s_inode_count; i++){
    if(inode_bitmap[i>>3] & (1 << (i % 8)) == 0){
      //we found the space
      inode_bitmap[i >> 3] |= (1 << (i % 8));
      memset(inode_table + i, 0, (struct ext2_inode));
      sb->s_free_inodes_count --;
      group_table->bg_free_inodes_count --;
      return i+1;
    }
  }
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
    char filename[255];
    int file_indx = argv[2] -1;
    while(file_indx >= 0){
      if(argv[2][file_indx] != '/'){
        file_indx --;
      }else{
        break;
      }
    }
    for(int i=file_indx+1; i<strlen(argv[2]); i++){
      append(filename, argv[2][i]);
    }
    FILE* fp = fopen(argv[2], "r");
    if(fp == NULL){
      perror("Failed to open the file");
      exit(-1);
    }

    //cut target path to each directory name and store it in a list.
    char* target_path = argv[3];
    int total_dir_length = dirDepth(target_path);
      //initialize dir list
    char dir_names[total_dir_length][255];
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
    for(int i=0; i<total_dir_length; i++){
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
    char* block_bitmap = (char*)(disk + 1024*group_table[0].bg_block_bitmap);
    //inode bitmap
    char* inode_bitmap = (char*)(disk + 1024*group_table[0].bg_inode_bitmap);

    //check root dir from target_path to get inode
    int dir_inode = EXT2_ROOT_INO - 1;
    if(strcmp(dir_names[0], ".") != 0){
      //if the root of target_path is not ".", we try to cd to it
      dir_inode = get_parent_dir((char**)dir_names, total_dir_length, inode_table);
      if(dir_inode == -ENOENT){
        printf("found error at get parent dir");
        exit(ENOENT);
      }
    }

    //check whether the file is already existed in our target_path
    struct ext2_dir_entry* possible_dup = search_inode(filename, inode_table + dir_inode, FIND_NAME);
    if(possible_dup != NULL){
      //there is already a file with the same name as filename.
      printf("found the file is already existed");
      exit(EEXIST);
    }

    //Now, we begin to find space and put Data of file
      //get the space
      struct ext2_dir_entry* file_space = search_inode(filename, inode_table + dir_inode, GET_SPACE);
      file_space->name_len = strlen(filename);
      file_space->file_type = EXT2_FT_REG_FILE;
      strncpy(file_space->name, filename, strlen(filename));

      //set the inode
      int inode_num = find_free_inode(sb, group_table, inode_table, inode_bitmap);


    return 0;
}
