#include <stdio.h>

#include "disk.h"

#include <stdio.h>
#include "fs.h" // Assuming your file system functions are in fs.h

int main() {
    //Create a new file system
    if (make_fs("myfs") != 0) {
        printf("Failed to create file system.\n");
        return -1;
    }
    printf("File system created successfully.\n");

    // Mount the file system
    if (mount_fs("myfs") != 0) {
        printf("Failed to mount file system.\n");
        return -1;
    }
    printf("File system mounted successfully.\n");

    // Create a new file
    if (fs_create("testfile") != 0) {
        printf("Failed to create file.\n");
        return -1;
    }
    printf("File created successfully.\n");
    // Open the file
    int fd = fs_open("testfile");
    if (fd < 0) {
        printf("Failed to open file.\n");
        return -1;
    }
    printf("File opened successfully.\n");

    // Write to the file
    char buf[10] = "Hello";
    if (fs_write(fd, buf, 5) != 5) {
        printf(buf);
        printf("Failed to write to file.\n");
        return -1;
    }
    printf("File written to successfully.\n");
    //reset seek pointer
    if (fs_lseek(fd, 1) != 0) {
        printf("Failed to reset seek pointer.\n");
        return -1;
    }
    printf("Seek pointer reset successfully.\n");
    //read from the file
    char buf2[10];

    if (fs_read(fd, buf2, 5) <= 0) {
        printf("Failed to read from file.\n");
        return -1;
    }
    printf("File read from successfully.\n");
    printf(buf2);
    //reading at edge of blocks
    //seek to just before the end of the block
    if (fs_lseek(fd, BLOCK_SIZE-3) != 0) {
        printf("Failed to reset seek pointer.\n");
        return -1;
    }
    printf("Seek pointer at edge.\n");
    //write to the file
    char buf3[10] = "World";
    if (fs_write(fd, buf3, 5) != 5) {
        
        printf("Failed to write to file.\n");
        return -1;
    }
    printf("File written to successfully to edge.\n");
    //reset seek pointer to edge
    if (fs_lseek(fd, BLOCK_SIZE-3) != 0) {
        printf("Failed to reset seek pointer.\n");
        return -1;
    }
    printf("Seek pointer reset successfully to edge.\n");
    //read from the file
    char buf4[10];
    if(fs_read(fd, buf4, 5) != 5) {
        printf("Failed to read from file.\n");
        return -1;
    }
    printf("File read from successfully from edge.\n");

    // Close the file
    if (fs_close(fd) != 0) {
        printf("Failed to close file.\n");
        return -1;
    }
    printf("File closed successfully.\n");
    // Unmount the file system
    if (umount_fs("myfs") != 0) {
        printf("Failed to unmount file system.\n");
        return -1;
    }
    printf("File system unmounted successfully.\n");
    printf("File system operations completed successfully.\n");
    return 0;
}