


#define MAX_FILDES 32
#define MAX_FILES 64
#define MAX_F_NAME 15 //15 characters
#define FREE -1
#define END -2
#define IDENTIFIER 5678

int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);
int fs_open(char *name);
int fs_close(int fd);
int fs_create(char *name);
int fs_delete(char *name);
int fs_read(int fd, void *buf, size_t nbyte);
int fs_write(int fd, void *buf, size_t nbyte);
int fs_get_filesize(int fd);
int fs_listfiles(char ***files);
int fs_lseek(int fd, off_t offset);
int fs_truncate(int fd, off_t length);

int read_file(int b, char *buf, size_t n);
int write_file(char* buf, size_t n);
int next_free_block();