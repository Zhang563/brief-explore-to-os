#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fs.h"
#include "disk.h"
#define min(a, b) ((a) < (b) ? (a) : (b))
struct super_block {
    int fat_idx; // First block of the FAT
    int fat_len; // Length of FAT in blocks
    int dir_idx; // First block of directory
    int dir_len; // Length of directory in blocks int data_idx; // First block of file-data
    int id; // File system identifier
};

struct dir_entry {
    char name [MAX_F_NAME + 1]; // DOH!
    int used; // Is this file-”slot” in use 
    int size; // file size
    int head; // first data block of file 
    int ref_cnt; // >0 cannot delete
};

struct file_descriptor {
    int used;// fd in use
    int file;// the first block of the file (f) to which fd refers too 
    int offset; // position of fd within f
};

struct super_block fs;
struct file_descriptor fildes[MAX_FILDES]; // 32 
short *FAT = NULL; // Will be populated with the FAT data 
struct dir_entry *DIR = NULL; // Will be populated withthe directory data
int mounted = 0; // 0 if not mounted, 1 if mounted
int LAZY = -5; // Used to indicate lazy creation
//read file, starting at block b, into buf, for n bytes, return 0 on success, -1 on failure
//use block_read to read block, follow FAT to next block
int read_file(int b, char *buf, size_t n){
    //will follow the fat table
    int i = 0;
    size_t bytes_read = 0;
    
    while(b != END){
        if(bytes_read + BLOCK_SIZE > n){
            //partial read
            char temp[BLOCK_SIZE];
            if(block_read(b, temp) == -1){ //read block b into temp
                return -1;
            }
            memcpy(buf + i * BLOCK_SIZE, temp, n - bytes_read); //copy n - bytes_read bytes from temp to buf
            bytes_read += n - bytes_read;
            return bytes_read;
        }else if(block_read(b, buf + i * BLOCK_SIZE) == -1){ //read block b into buf
            return -1;
        }
        bytes_read += BLOCK_SIZE;
        b = FAT[b];
        i++;
    }
    return bytes_read;
}
//write file, starting an empty block, into buf, for n bytes, return initial block on success, -1 on failure
//use block_write to write block, follow FAT to find next block, update FAT
int write_file(char* buf, size_t n){
    //will not follow the fat table
    int start_block = next_free_block();
    if(start_block == -1){
        return -1; // No free block available
    }

    int current_block = start_block;
    int prev_block = -1; // To store the previous block number
    int i = 0;
    size_t bytes_written = 0;

    while(bytes_written < n){
        size_t bytes_to_write = (n - bytes_written < BLOCK_SIZE) ? (n - bytes_written) : BLOCK_SIZE;
        char temp_buf[BLOCK_SIZE];
        memcpy(temp_buf, buf + i * BLOCK_SIZE, bytes_to_write);

        if(block_write(current_block, temp_buf) == -1){
            // Rollback if write fails
            while(start_block != END){
                int next_block = FAT[start_block];
                FAT[start_block] = FREE; // Mark block as free
                start_block = next_block;
            }
            return -1; // Write failed
        }

        bytes_written += bytes_to_write;
        i++;

        if(bytes_written < n){
            prev_block = current_block;
            current_block = next_free_block();

            if(current_block == -1){
                FAT[prev_block] = END;
                return bytes_written; //project constraint, stop writing at full
                //=====UNREACHABLE=====
                // Rollback if no more free blocks
                while(start_block != END){
                    int next_block = FAT[start_block];
                    FAT[start_block] = FREE; // Mark block as free
                    start_block = next_block;
                }
                return -2;// No more free blocks
            }

            if(prev_block != -1){
                FAT[prev_block] = current_block;
            }
        }
    }

    FAT[current_block] = END; // Mark the end of file in FAT
    return bytes_written;
}

int next_free_block(){
    int i;
    for(i = 0; i < DISK_BLOCKS; i++){
        if(FAT[i] == FREE){
            return i;
        }
    }
    return -1;
}

int make_fs(char *disk_name) {
    if (mounted == 1) {
        return -1;
    }
    if (make_disk(disk_name) == -1) {
        return -1;
    }
    if (open_disk(disk_name) == -1) {
        return -1;
    }
  
    mounted = 1;
    // Initialize FAT to have DISK_BLOCKS entries
    FAT = (short *)calloc(DISK_BLOCKS, sizeof(short));
    if (FAT == NULL) {
        return -1;
    }
    memset(FAT, FREE, DISK_BLOCKS * sizeof(short));

    // Initialize directory
    DIR = (struct dir_entry*)calloc(MAX_FILES, sizeof(struct dir_entry));
    if (DIR == NULL)
    {
        return -1;
    }
    

    // Initialize super block
    fs.fat_idx = 1; // Skip block 0 for the super block
    fs.fat_len = (DISK_BLOCKS * sizeof(short) + BLOCK_SIZE - 1) / BLOCK_SIZE; // Calculate number of blocks for FAT
    fs.dir_idx = fs.fat_idx + fs.fat_len;
    fs.dir_len = (MAX_FILES * sizeof(struct dir_entry) + BLOCK_SIZE - 1) / BLOCK_SIZE; // Calculate number of blocks for directory
    fs.id = IDENTIFIER;

    //reserve blocks for FAT and DIR and super block
    int i;
    for (i = 0; i < fs.dir_len + fs.fat_len + 1; i++) {
        FAT[i] = (i == 0 || i == fs.dir_idx+fs.dir_len-1 || i == fs.fat_idx+fs.fat_len-1) ? END : i + 1;
    }
    // Write FAT to the disk using a buffer
    char temp_buf[BLOCK_SIZE];
    for (i = 0; i < fs.fat_len; i++) {
        memcpy(temp_buf, FAT + i * BLOCK_SIZE, BLOCK_SIZE);
        if (block_write(fs.fat_idx + i, temp_buf) == -1) {
            return -1;
        }
    }
    // Write directory to the disk using a buffer
    for (i = 0; i < fs.dir_len; i++) {
        memcpy(temp_buf, DIR + i * BLOCK_SIZE, BLOCK_SIZE);
        if (block_write(fs.dir_idx + i, temp_buf) == -1) {
            return -1;
        }
    }
    // Write super block to block 0 using a buffer
    memcpy(temp_buf, &fs, sizeof(struct super_block));
    if (block_write(0, temp_buf) == -1) {
        return -1;
    }

    // Cleanup and unmount
    if (close_disk() == -1) {
       
        return -1;
    }
    mounted = 0;

    // free(FAT);
    // free(DIR);
    // FAT = NULL;
    // DIR = NULL;
    
    return 0;
}

int mount_fs(char *disk_name) {
    if (mounted == 1) {
        return -1;
    }

    // Open the disk
    if (open_disk(disk_name) == -1) {
        return -1;
    }
    fs.id = 0;
    // Read the super block from block 0
    if (block_read(0, (char*)&fs) == -1) {
        return -1;
    }

    // Check if the file system is valid
    if (fs.id != IDENTIFIER) {
        return -1;
    }
    //printf("DISK_BLOCKS: %d, sizeof(short): %zu\n", DISK_BLOCKS, sizeof(short));

    // Allocate memory and read FAT into memory
    //FAT = (short *) calloc(DISK_BLOCKS, sizeof(short));
    FAT = (short *) realloc(FAT, DISK_BLOCKS * sizeof(short));
    if (FAT == NULL) {
        return -1;
    }
    char *fat_block = (char*)FAT;
    int i;
    for (i = 0; i < fs.fat_len; i++) {
        if (block_read(fs.fat_idx + i, fat_block + i * BLOCK_SIZE) == -1) {
            return -1;
        }
    }

    // Allocate memory and read DIR into memory
    //DIR = (struct dir_entry *)calloc(MAX_FILES, sizeof(struct dir_entry));
    DIR = (struct dir_entry *)realloc(DIR, MAX_FILES*sizeof(struct dir_entry));
    if (DIR == NULL) {

        return -1;
    }
    char *dir_block = (char*)DIR;
    for (i = 0; i < fs.dir_len; i++) {
        if (block_read(fs.dir_idx + i, dir_block + i * BLOCK_SIZE) == -1) {
         
            return -1;
        }
    }

    // Clear file descriptors
    memset(fildes, 0, MAX_FILDES * sizeof(struct file_descriptor));

    // Mark as mounted
    mounted = 1;
    return 0;
}


int umount_fs(char *disk_name) {
    if(mounted == 0){
        return -1;
    }
    
    // Write FAT to the disk using a buffer
    char temp_buf[BLOCK_SIZE];
    int i;
    for (i = 0; i < fs.fat_len; i++) {
        memcpy(temp_buf, FAT + i * BLOCK_SIZE, BLOCK_SIZE);
        if (block_write(fs.fat_idx + i, temp_buf) == -1) {
            return -1;
        }
    }
    // Write directory to the disk using a buffer
    for (i = 0; i < fs.dir_len; i++) {
        memcpy(temp_buf, DIR + i * BLOCK_SIZE, BLOCK_SIZE);
        if (block_write(fs.dir_idx + i, temp_buf) == -1) {
            return -1;
        }
    }
    // Write super block to block 0 using a buffer
    memcpy(temp_buf, &fs, sizeof(struct super_block));
    if (block_write(0, temp_buf) == -1) {
        return -1;
    }

    //close disk
    if (close_disk() == -1) {
        return -1;
    }

    // free(FAT);
    // FAT = NULL;
    // free(DIR);
    // DIR = NULL;
    mounted = 0;
    return 0;
}

int fs_create(char *name) {
    if(mounted == 0){
        return -1;
    }
    //check if name is valid
    if (strlen(name) > MAX_F_NAME) {
        return -1;
    }
    //check if name already exists
    int i;
    for (i = 0; i < MAX_FILES; i++) {

        if (strcmp(DIR[i].name, name) == 0) {
            return -1;
        }
    }
    //find a free entry in DIR
    for (i = 0; i < MAX_FILES; i++) {
        if (DIR[i].used == 0) {
            break;
        }
    }
    //check if DIR is full
    if (i == MAX_FILES) {
        return -1;
    }
    //initialize DIR entry
    DIR[i].used = 1;
    DIR[i].size = 0;
    DIR[i].head = LAZY;
    DIR[i].ref_cnt = 0;
    strcpy(DIR[i].name, name);
    
    
    return 0;
}

int fs_open(char* name) {
    //check if name is valid
    if(mounted == 0){
        return -1;
    }
    if (strlen(name) > MAX_F_NAME) {
        return -1;
    }
    //find the file in DIR
    int i;
    for (i = 0; i < MAX_FILES; i++) {
        if (strcmp(DIR[i].name, name) == 0) {
            break;
        }
    }
    //check if file exists
    if (i == MAX_FILES) {
        return -1;
    }
    //check if file is open
    
    //find a free entry in fildes
    for (i = 0; i < MAX_FILDES; i++) {
        if (fildes[i].used == 0) {
            break;
        }
    }
    //check if fildes is full
    if (i == MAX_FILDES) {
        return -1;
    }
    //initialize fildes entry
    fildes[i].used = 1;
    fildes[i].file = i;
    fildes[i].offset = 0;
    if (DIR[i].ref_cnt > 0) {
        DIR[i].ref_cnt++;
    }else{
        DIR[i].ref_cnt = 1;
    }
    return i;
}
//====
int fs_write(int fd, void *buf, size_t nbyte) {
    // Validation checks
    if (fd < 0 || fd >= MAX_FILDES || fildes[fd].used == 0 || buf == NULL || nbyte <= 0 || mounted == 0 || fildes[fd].offset < 0 || fildes[fd].offset > DIR[fildes[fd].file].size) {
        return -1;
    }

    //int start_pos = fildes[fd].offset;  // The initial offset in the file
    int bytes_written = 0;
    //handle lazy creation, the first write to a file
    //file_write can fully handle lazy creation
    if (DIR[fildes[fd].file].head == LAZY) {
        DIR[fildes[fd].file].head = next_free_block();
        bytes_written = write_file((char *)buf, nbyte);
        if (bytes_written == -1) {
            // Handle error in write_file
            return -1;
        }
        // Update offset
        fildes[fd].offset += bytes_written;
        // Update size
        DIR[fildes[fd].file].size = fildes[fd].offset;
        
        return bytes_written;
    }
    //TODO: append to existing blocks

    // Write to existing blocks
    //find the block to start writing
    int b = DIR[fildes[fd].file].head;
    int i;
    for (i = 0; i < fildes[fd].offset/BLOCK_SIZE; i++) {
        b = FAT[b];
    }
    //partial write or full write on existing block(s)
    if (fildes[fd].offset % BLOCK_SIZE != 0) {
        char temp[BLOCK_SIZE];
        if (block_read(b, temp) == -1) {
            return -1;
        }
        //copy the partial block
        //take the lesser value of partial_write_size and nbyte
        int partial_write_size = BLOCK_SIZE - fildes[fd].offset % BLOCK_SIZE;
        if (partial_write_size > nbyte) {
            partial_write_size = nbyte;
        }
        memcpy(temp + fildes[fd].offset % BLOCK_SIZE, buf, partial_write_size);
        if (block_write(b, temp) == -1) {
            return -1;
        }
        bytes_written += partial_write_size;
    }
    //if partial write on first and not reached next block
    if (bytes_written == nbyte) {
        fildes[fd].offset += bytes_written;
        //update size
        if (fildes[fd].offset > DIR[fildes[fd].file].size) {
            DIR[fildes[fd].file].size = fildes[fd].offset;
        }
        return bytes_written;
    }
    //full write on subsequent existing blocks or no partial in first block
    //not handled by write_file since write_file will not follow FAT
    //write block by block and stop before the last block or when nbyte is reached
    b = FAT[b];
    while (FAT[b] != END && bytes_written + BLOCK_SIZE <= nbyte) {
        if (block_write(b, buf + bytes_written) == -1) {
            return -1;
        }
        bytes_written += BLOCK_SIZE;
        b = FAT[b];
    }

    //return if nbyte is reached
    if (bytes_written == nbyte) {
        fildes[fd].offset += bytes_written;
        //update size
        if (fildes[fd].offset > DIR[fildes[fd].file].size) {
            DIR[fildes[fd].file].size = fildes[fd].offset;
        }
        return bytes_written;
    }
    //FAT[b] == END
    

    //write or partial write on last block

    if (bytes_written < nbyte) {
        char temp[BLOCK_SIZE];
        if (block_read(b, temp) == -1) {
            return -1;
        }
        memcpy(temp, buf + bytes_written, nbyte - bytes_written);
        if (block_write(b, temp) == -1) {
            return -1;
        }
        bytes_written += nbyte - bytes_written;
    }
    
    //if write is not complete, write to new blocks using write_file
    if (bytes_written < nbyte) {
        //connect the last block to the new block
        FAT[b] = next_free_block();
        int temp = write_file(buf + bytes_written, nbyte - bytes_written);
        if (temp == -1) {
            return -1;
        }
        bytes_written += temp;
    }
    //update offset and size
    fildes[fd].offset += bytes_written;
    if (fildes[fd].offset > DIR[fildes[fd].file].size) {
        DIR[fildes[fd].file].size = fildes[fd].offset;
    }

    fildes[fd].offset += bytes_written;
    //update size
    if (fildes[fd].offset > DIR[fildes[fd].file].size) {
        DIR[fildes[fd].file].size = fildes[fd].offset;
    }
    return bytes_written;
}


int fs_close(int fd){
    if(mounted == 0){
        return -1;
    }
    //check if fd is valid
    if (fd < 0 || fd >= MAX_FILDES) {
        return -1;
    }
    //check if fildes is in use
    if (fildes[fd].used == 0) {
        return -1;
    }
    //decrement ref_cnt
    DIR[fildes[fd].file].ref_cnt--;
    //check if ref_cnt is 0
    if (DIR[fildes[fd].file].ref_cnt == 0) {
        //free fildes entry
        fildes[fd].used = 0;

    }
    return 0;
}
int fs_delete(char *name){
    if(mounted == 0){
        return -1;
    }
    //check if name is valid
    if (strlen(name) > MAX_F_NAME) {
        return -1;
    }
    //find the file in DIR
    int i;
    for (i = 0; i < MAX_FILES; i++) {
        if (strcmp(DIR[i].name, name) == 0) {
            break;
        }
    }
    //check if file exists
    if (i == MAX_FILES) {
        return -1;
    }
    //check if file is open
    if (DIR[i].ref_cnt > 0) {
        return -1;
    }
    //free DIR entry
    DIR[i].used = 0;
    memset(DIR[i].name, 0, MAX_F_NAME + 1);
    //free FAT blocks
    int b = DIR[i].head;
    //if lazy creation, head is LAZY
    if (b == LAZY) {
        //clear the name in DIR
        
        return 0;
        //no blocks to free
    }
    while (b != END) {
        int temp = FAT[b];
        FAT[b] = FREE;
        b = temp;
    }
    FAT[b] = FREE;
    return 0;
}

int fs_read(int fd, void *buf, size_t nbyte){
    if(mounted == 0){
        return -1;
    }
    //check if fd is valid
    if (fd < 0 || fd >= MAX_FILDES) {
        return -1;
    }
    //check if fildes is in use
    if (fildes[fd].used == 0) {
        return -1;
    }
    //check if buf is valid
    if (buf == NULL) {
        return -1;
    }
    //check if nbyte is valid
    if (nbyte < 0) {
        return -1;
    }
    //check if offset is valid
    if (fildes[fd].offset < 0 || fildes[fd].offset > DIR[fildes[fd].file].size) {
        return -1;
    }
    if (fildes[fd].offset + nbyte > DIR[fildes[fd].file].size) {
        nbyte = DIR[fildes[fd].file].size - fildes[fd].offset;
    }

    if (nbyte == 0) {
        return 0;  // Nothing to read
    }
    //find actual number of bytes to read
    int bytes_to_read = min(nbyte, DIR[fildes[fd].file].size - fildes[fd].offset);
    //read all blocks covered
    int start_block = fildes[fd].offset / BLOCK_SIZE;
    int end_block = (fildes[fd].offset + bytes_to_read - 1) / BLOCK_SIZE;
    int total_blocks = end_block - start_block + 1;
    //create a buffer large enough to hold all blocks
    char *temp_buf = calloc(total_blocks, BLOCK_SIZE);
    int b = DIR[fildes[fd].file].head;
    int i;
    for (i = 0; i < start_block; i++) {
        b = FAT[b];
    }
    //copy all blocks into temp_buf
    for(i = 0; i < total_blocks; i++){
        if(block_read(b, temp_buf + i * BLOCK_SIZE) == -1){
            return -1;
        }
        b = FAT[b];
    }
    //copy bytes_to_read bytes from temp_buf to buf
    memcpy(buf, temp_buf + fildes[fd].offset % BLOCK_SIZE, bytes_to_read);
    free(temp_buf);
    //update offset
    fildes[fd].offset += bytes_to_read;
    return bytes_to_read;
    

}

int fs_get_filesize(int fd){
    if(mounted == 0){
        return -1;
    }
    //check if fd is valid
    if (fd < 0 || fd >= MAX_FILDES) {
        return -1;
    }
    //check if fildes is in use
    if (fildes[fd].used == 0) {
        return -1;
    }
    return DIR[fildes[fd].file].size;
}

int fs_listfiles(char ***files){
    //check if files is valid
    if (files == NULL) {
        return -1;
    }
    //allocate memory for files
    *files = malloc(MAX_FILES * sizeof(char*));
    if (*files == NULL) {
        return -1;
    }
    //initialize files
    int i;
    for (i = 0; i < MAX_FILES; i++) {
        (*files)[i] = NULL;
    }
    //copy file names into files
    for (i = 0; i < MAX_FILES; i++) {
        if (DIR[i].used == 1) {
            (*files)[i] = malloc(MAX_F_NAME + 1);
            if ((*files)[i] == NULL) {
                return -1;
            }
            strcpy((*files)[i], DIR[i].name);
        }
    }
    return 0;
}
int fs_lseek(int fd, off_t offset){
    if(mounted == 0){
        return -1;
    }
    //check if fd is valid
    if (fd < 0 || fd >= MAX_FILDES) {
        return -1;
    }
    //check if fildes is in use
    if (fildes[fd].used == 0) {
        return -1;
    }
    //check if offset is valid
    if (offset < 0 || offset > DIR[fildes[fd].file].size) {
        return -1;
    }

    //update offset
    fildes[fd].offset = offset;
    return 0;
}
int fs_truncate(int fd, off_t length){
    if(mounted == 0){
        return -1;
    }
    //check if fd is valid
    if (fd < 0 || fd >= MAX_FILDES) {
        return -1;
    }
    //check if fildes is in use
    if (fildes[fd].used == 0) {
        return -1;
    }
    //check if length is valid
    if (length < 0 || length > DIR[fildes[fd].file].size) {
        return -1;
    }
    //free blocks
    int b = DIR[fildes[fd].file].head;
    int i;
    //block to keep
    //ciling of length/BLOCK_SIZE
    for (i = 0; i < (int)(length+BLOCK_SIZE-1)/BLOCK_SIZE; i++) {
        b = FAT[b];
    }
    while(b != END){
        int temp = FAT[b];
        FAT[b] = FREE;
        b = temp;
    }
    FAT[b] = FREE;

    //update DIR entry
    DIR[fildes[fd].file].size = length;
    return 0;
}
