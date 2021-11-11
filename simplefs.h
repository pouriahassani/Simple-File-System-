#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MODE_APPEND 1
#define MODE_READ 0
#define BLOCKSIZE  1024
int vdisk_fd; /* global virtual disk file descriptor
               will be assigned with the sfs_mount call
               any function in this file can use this.*/

//Open file data structure
int size;
struct OFT_Entry{
	int size;
	int start_block_num;
	int last_block_num;
	int fd;
	int mode;
	int valid;
	char* name;
	int file_num;
	int fpp;
};

//Global variable keeps track of the number of open files
int num_open_files;

// This function is simply used to a create a virtual disk
int create_vdisk (char *, int);

// read block k from disk (virtual disk) into buffer block.
int read_block (void *, int);

// write block k into the virtual disk. 
int write_block (void *, int);

//This function will be used to initialize/create an sfs file system on the virtual disk
int sfs_format (char *);

 // open the Linux file vdiskname 
int sfs_mount (char *);

//This function will be used to mount the file system, i.e., to prepare the file system be used. 
int sfs_umount();

//With this, an applicaton will create a new file with name filename.
int sfs_create(char *);

//With this an application will open a file.
int sfs_open(char *, int);

//With this an application will close a file
int sfs_close(int);

//With this an application learns the size of a file
int sfs_getsize (int);

//With this, an application can read data from a file
int sfs_read(int, void *, int);

//With this, an application can append new data to the file
int sfs_append(int, void *, int);

//With this, an application can delete a file
int sfs_delete(char *);

//returns number of allocated files
int num_alloc_files_in_dir();

//copy data in the buffer to the allocated memoey space
void copy_block( int* block, int* source_block,int data_start,int buf_size);

//prints all files information 
void print_file();


//print all allocated data
void print_data();

//print FOT content
void print_FOT();