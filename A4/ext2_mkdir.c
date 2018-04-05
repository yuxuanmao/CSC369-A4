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
void write_new_block(unsigned int block_num, unsigned int inode, unsigned int parentInode);
void modify_parent_block(unsigned int inode, char *new_dir_name, unsigned int parentBlock);
void update_inode(unsigned int block_num, unsigned int inode_num, struct ext2_inode* inode_table);



void append(char *s, char c)
{
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
    // get rid of the first slash 
    char* target_path = argv[2];
    if (target_path[0] != '/'){
        perror("please give a root path");
        exit(-1);
    }

    target_path += 1;
    

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
       
        inode_num = findParentDirectoryInode(inode_table, inode_num, parsed_name);
        if (inode_num == 0){
            
            exit(ENOENT);
        }
    }

    // inode_num should now indicate the parent directory of where we would like to place our new directory
    
    // find parent block
    struct ext2_inode* parent_inode = inode_table + (inode_num - 1);
    unsigned int parent_block_num = (parent_inode->i_block)[0];
   

    


    // find the smallest nonreserved inode and make this inode unavailable
    int smallest_inode = find_free_inode(sb, group_table);
   
    if (smallest_inode == -1){
        perror("No more free inodes");
        exit(-1);
    }


    // find the smallest free block
    int smallest_block = find_free_block(sb, group_table);
   
    if (smallest_block == -1){
        perror("No more free blocks");
        exit(-1);
    }

    
    
    // modify the parent directory to link to the new directory
    modify_parent_block((unsigned int)smallest_inode, new_dir_name, parent_block_num);

    // write the dict struct into the new block, link inode
    write_new_block((unsigned int) smallest_block, (unsigned int) smallest_inode, (unsigned int) inode_num);

    // update inode 
    update_inode((unsigned int) smallest_block, (unsigned int) smallest_inode, inode_table);

    return 0;
}



void update_inode(unsigned int block_num, unsigned int inode_num, struct ext2_inode* inode_table){
    struct ext2_inode* inode = inode_table + (inode_num - 1);
    inode->i_mode = EXT2_S_IFDIR;
    inode->i_size = 1024;
    inode->i_uid = '0';
    inode->i_links_count ++;
    inode->i_blocks = ((inode->i_blocks*512) + EXT2_BLOCK_SIZE)/512;
    inode->i_gid = '0';
    inode->i_block[0] = block_num;
    inode->osd1 = 0;
    inode->i_generation = 0;
    inode->i_file_acl = 0;
}



// modify the block where the parent directory is to include the new directory name
// find the smallest available place in the dir, check remaining space. 
void modify_parent_block(unsigned int inode, char *new_dir_name, unsigned int parentBlock){
    // find where is available in this block
    struct ext2_dir_entry *checkingEntry = (struct ext2_dir_entry *)(disk + 0x400 * parentBlock);
    unsigned int rec_len_total = checkingEntry->rec_len;
    // rec lengh of the most current entry. It is to keep track of the length before last entry
    unsigned int cur_rec_len;

    while (rec_len_total<EXT2_BLOCK_SIZE){
        
        checkingEntry = (struct ext2_dir_entry *)(disk + 0x400 * parentBlock+rec_len_total);
        
        // check that same name don't already exist
        char *checkingEntry_name = checkingEntry->name;
      
        if (strcmp(checkingEntry_name, new_dir_name) == 0){
           
            exit(ENOENT);
        }
        cur_rec_len = checkingEntry->rec_len;
        rec_len_total += cur_rec_len;
    }

    // right now check entry is the last entry, we will extract space in the last entry
    unsigned int size_before_last = rec_len_total - cur_rec_len;
    unsigned int needed_name_size;
    if (checkingEntry->name_len%4 == 0){
        needed_name_size = checkingEntry->name_len;
    }else{
        needed_name_size = checkingEntry->name_len + 4-checkingEntry->name_len%4;
    }
    
    unsigned int last_entry_needed_size = 8 + needed_name_size;

    checkingEntry->rec_len = last_entry_needed_size;


   

    // now checkingEntry is at the start of free space in the block
    if(sizeof(struct ext2_dir_entry *) > EXT2_BLOCK_SIZE - (size_before_last)){
        perror("not enough space in parent directory");
    }


    // now place new entry here 
    struct ext2_dir_entry *childEntry = (struct ext2_dir_entry *)(disk + 0x400 * parentBlock + size_before_last + last_entry_needed_size);
    childEntry->inode = inode;
    strncpy(childEntry->name, new_dir_name, strlen(new_dir_name));
    childEntry->file_type = EXT2_FT_DIR;
   
    childEntry->name_len = (unsigned char) strlen(new_dir_name);
    
    // this entry takes up all the remaining space in this block
    childEntry->rec_len = 1024 - (size_before_last+last_entry_needed_size);
    
}



// write dictionary with name and related inode into smallest block
// since we know exactly what we are writing into the new block and the size of the content, we don't need to consider the case where the 
// the content exceed block size.
void write_new_block(unsigned int block_num, unsigned int inode, unsigned int parentInode){
    // index of the entry struct in the 


    struct ext2_dir_entry *selfentry = (struct ext2_dir_entry *)(disk + 0x400 * block_num);
    

    selfentry->inode = inode;
    strncpy(selfentry->name, ".", strlen("."));
    selfentry->file_type = EXT2_FT_DIR;
    selfentry->name_len = 1;
    // 8+1+3
    selfentry->rec_len = 12;

    struct ext2_dir_entry *parententry = (struct ext2_dir_entry *)(disk + 0x400 * block_num + selfentry->rec_len);

    parententry->inode = parentInode;
    strncpy(parententry->name, "..", strlen(".."));
    parententry->file_type = EXT2_FT_DIR;
    parententry->name_len = 2;
    // 8 + 2 + 2
    parententry->rec_len = 1024 - 12;

}



// find smallest unreserved free inode
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



// find smallest unreserved free block
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
                // accumulator for the total size of the linked list, as the size cannot exceed block size
                int index = 0;
                while (index < EXT2_BLOCK_SIZE){
                    struct ext2_dir_entry *entry = (struct ext2_dir_entry*)(disk + 0x400 * block + index);
                    // determine the type of the file
                    
                    char* name;
                    struct ext2_inode* inode = table + entry->inode-1;

                    if(EXT2_S_IFDIR & inode->i_mode){
                        //directory  
                        name = (char*)(disk + 0x400 * block + index +sizeof(struct ext2_dir_entry));
                        if (strcmp(name, parsed_name) == 0){
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




