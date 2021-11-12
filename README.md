# Simple-File-System-
This project a simple file system. The file system will be
implemented as a library (libsimplefs.a) and will store files in a virtual disk. The
virtual disk will be a simple regular Linux file of some certain size. An application
that would like to create and use files will be linked with the library. We will assume
that the virtual disk will be used by one process at a time. When the process using the
virtual disk terminates, then another process that will use the virtual disk can be
started.

The functions of the file system are implemented in `simplefs.c` file. The description of the functions are explained in the `simplefs.h file.
The sfs file system will have just a single directory, i.e., root directory. The block size is 1 KB. Block 0 (first block) will contain superblock information. 
The next 7 blocks, ie., blocks 1, 2, 3, 4, 5, 6, 7, will contain the root directory. Fixed
sized directory entries will be used. Directory entry size is 128 bytes.
FAT (file allocation table) method is used to keep track of blocks allocated to files
and free blocks. FAT entry size is 8 bytes. FAT will be stored after the root directory
in the disk.

`app.c` file is a sample code to test different functions in the code.
