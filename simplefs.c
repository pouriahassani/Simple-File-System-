#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"
#include <string.h>

int vdisk_fd; // global virtual disk file descriptor
              // will be assigned with the sfs_mount call
              // any function in this file can use this.

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

//set the pointer to open file table to NULL 
//it will be initialized later
struct OFT_Entry *OFT = NULL;

int num_open_files;
// This function is simply used to a create a virtual disk
// (a simple Linux file including all zeros) of the specified size.
// You can call this function from an app to create a virtual disk.
// There are other ways of creating a virtual disk (a Linux file)
// of certain size. 
// size = 2^m Bytes
int create_vdisk (char *vdiskname, int m)
{
    char command[BLOCKSIZE]; 
    int num = 1;
    int count; 
    size  = num<<m;
	
	if(size> (1<<27) )
		return (-1);
	
    count = size / BLOCKSIZE;
    printf ("%d %d", m, size);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
	     vdiskname, BLOCKSIZE, count);
    printf ("executing command = %s\n", command); 
    system (command); 
    return (0); 
}



// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk. 
int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("read error\n");
	return -1;
    }
    return (0); 
}

// write block k into the virtual disk. 
int write_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("write error\n");
	return (-1);
    }
	//printf("\nwrite is done");
    return 0; 
}


/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/

int sfs_format (char *vdiskname)
{
	sfs_mount(vdiskname);
	int* superblock_data = malloc(BLOCKSIZE);
	int return_value =0;
	
	if(superblock_data == NULL)
	{
		printf("\nsfs_format error: Error in allocating superblock to initialize it");
		return -1;
	}
	//set second 4 bytes of superblocks as the number of the allocated files
	superblock_data[1] =0;
	
	//set the first 4 bytes of the file as the number of data blocks
	superblock_data[0] = size /BLOCKSIZE- 1032;
	
	return_value = write_block((void*)superblock_data,0);

	if(return_value !=0 )
		return -1;
	
	return_value = sfs_umount(vdiskname);
		
	if(return_value != 0)
		return -1;
	

	//initialization of the open file table
	if(OFT == NULL)
	{
		//create 10 entry for open file	
		OFT = malloc(sizeof(struct OFT_Entry)*10);
		num_open_files =0;
		for(int i=0;i<10;i++)
		{
			(OFT+i)->valid=0;
			(OFT+i)->fd = i;
		}
	}
	
	free(superblock_data);
	
	return (0); 
}

int sfs_mount (char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it. 
    vdisk_fd = open(vdiskname, O_RDWR); 
	
	if(vdisk_fd<0)
	{
		printf("\nsfs_mount error: Error in mounting disk");
		return -1;
	}
	
	else
		return(0);
}

int sfs_umount ()
{
	int return_value;
    return_value = fsync (vdisk_fd); 
	if (return_value !=0)
		printf("\nsfs_umount: Error while unmounting disk");
	
    close (vdisk_fd);
    return (return_value); 
}


int sfs_create(char *filename)
{
	
	int num_alloc_files = num_alloc_files_in_dir();
	int file_valid = 1;
	int size = 0;
	int *block = malloc(BLOCKSIZE);
	int *superblock = malloc(BLOCKSIZE);
	char *ptr;
	char* name;
	int name_len = strlen(filename);
	int found=0;
	int return_value_super;
	int return_value_block;
	
	int found_same =0;
	int count;
	int block_number = 0;
	
	if(block == NULL || superblock ==NULL)
	{
		printf("\nsfs_create: Error in creating new file");
		return -1;
	}
	
	//checks to find whether there is a file with the same name or not
	while(!found_same && block_number<7)
	{
		block_number++;
		read_block(block,block_number);
		
		for(int i=0;i<BLOCKSIZE/128;i++)
		{
			count=0;
			ptr =(char*) block + i*128;
			if( *((int*)(ptr+32+sizeof(int))) )
			{
				if(strlen(filename) ==*((int*)(ptr+32)))
				{
					for(int j=0;j<strlen(filename);j++)
					{
						if(*(ptr+j) == filename[j])
							count++;
					}
					if(count==strlen(filename))
					{
						found_same=1;
						break;
					}
				}
			}
		}
	}
	
	if(found_same)
	{
		printf("\nsfs_create: There is another file with the same name");
		return -1;
	}
	
	if(num_alloc_files<56)
	{
		read_block((void*)superblock,0);
		
		//checks the FAT to find free space to create new file
		for(int i=1;i<8;i++)
		{
			read_block((void*)block,i);

			if(found)
				break;
			
			for(int j=0;j<BLOCKSIZE/128;j++)
			{
				ptr =  (char*) ( (int*)block + 128*j/sizeof(int));
				name = ptr;
				
				//initialization of FAT
				if(*( (int*)(ptr+32+sizeof(int))) != 1)
				{
					*(int*)(ptr+32+2*sizeof(int)) = size;
					*(int*)(ptr+32+sizeof(int)) = file_valid;
					*(int*)(ptr+32) = name_len;
					strcpy(name,filename);
					superblock[1] = num_alloc_files+1;
					
					return_value_block = write_block((void*)block,i);
					return_value_super = write_block((void*)superblock,0);
					if(return_value_block !=0 || return_value_super !=0)
					{
						printf("\nsfs_create: Error in creating new file");
						return -1;
					}
					found=1;
					break;
				}
			}
		}
	}
	else
		printf("\nsfs_create: root directory is full");

	free(block);
	free(superblock);
	
    return (0);
}


int sfs_open(char *file, int mode)
{
	void* block = malloc(BLOCKSIZE);
	int found =0;
	char*ptr;
	int count;
	int block_number = 0;
	int fd;
	int found_file_num;
	int str_len;

	if(block == NULL)
	{
		printf("\nsfs_open: Error in opening file");
		return -1;
	}
	
	while(!found && block_number<7)
	{
		block_number++;
		read_block(block,block_number);
		
		//checks to find an allocated file with match name
		for(int i=0;i<BLOCKSIZE/128;i++)
		{
			count=0;
			ptr =(char*) block + i*128;
			if( *((int*)(ptr+32+sizeof(int))) )
			{
				if(strlen(file) ==*((int*)(ptr+32)))
				{
					for(int j=0;j<strlen(file);j++)
					{
						if(*(ptr+j) == file[j])
							count++;
					}
					if(count==strlen(file))
					{
						found=1;
						found_file_num=i;
						break;
					}
				}
			}
		}
	}

	if(found)
	{
		for(int i=0;i<10;i++)
		{
			count = 0;
			if((OFT+i)->valid)
			{
				str_len =strlen((OFT+i)->name);
				if(str_len == strlen(file))
				{
					for(int j=0;j<str_len;j++)
					{
						if(*( (OFT+i)->name+j) == file[j])
							count++;
					}
					if(count == strlen(file))
					{
						if((OFT+i)->mode == mode)
						{
							printf("\nThe file is already open");
							return (OFT+i)->fd;
						}
						else
						{
							printf("\nsfs_open: You can't open a previously opened file in another mode");
							return -1;
						}
					}
				}
			}
		}
		
		//checks the open file table to find a free space to insert information of the new open file
		for(int i=0;i<56;i++)
		{
			//initialization of new open file in open file table
			if((OFT+i)->valid == 0)
			{
				(OFT+i)->name = file;
				(OFT+i)->size = *((int*)(ptr +32 + 2*sizeof(int))); 
				(OFT+i)->start_block_num = *((int*)(ptr +32 + 3*sizeof(int))); 
				(OFT+i)->last_block_num = *((int*)(ptr +32 + 4*sizeof(int))); 
				(OFT+i)->mode = mode;
				fd = (OFT+i)->fd;
				(OFT+i)->valid =1;
				(OFT+i)->file_num = (block_number-1)*(BLOCKSIZE/128) +found_file_num;
				(OFT+i)->fpp =0;
				break;
			}
		}
	}
	
	else
	{
		printf("\nsfs_open: There is no such file");
		free(block);
		return -1;
	}
	
	free(block);
    return fd; 
}

int sfs_close(int fd){
	(OFT+fd)->valid = 0;
    return (0); 
}

int sfs_getsize (int  fd)
{
	if((OFT+fd)->valid)
		return ((OFT+fd)->size); 
	else
	{
		printf("\nThere is no file with this file descriptor");
		return -1;
	}
}

int sfs_read(int fd, void *buf, int n){

	//checks whether n is bigger than he size of the unread part of the file
	if(n>(OFT+fd)->size-(OFT+fd)->fpp)
		n=(OFT+fd)->size-(OFT+fd)->fpp;
	//if the file has no extra byte to read return number of the  read bytes which is 0
	if(n==0)
		return 0;

	int last_readed_byte= (OFT+fd)->fpp -1;
	int num_needed_block = (((OFT+fd)->fpp-1)%BLOCKSIZE  + n)/BLOCKSIZE +1;
	int first_read_block = last_readed_byte/BLOCKSIZE + (OFT+fd)->start_block_num;
	int *block=malloc(BLOCKSIZE);
	int num_readed_byte=last_readed_byte+1;
	int byte_count;
	int count=0;
	int count1;
	int count2;
	int k;
	
	//checks the mode  of the opend file
	if( (OFT+fd)->mode != MODE_READ)
	{
		printf("\nsfs_read: File open mode is not in read mode");
		return -1;
	}
		
	for (int i=first_read_block;i<num_needed_block+first_read_block;i++)
	{
		read_block(block,i+1032);
		
		//if the block to be read is the first one starts from the last byte read 
		if(i==first_read_block)
		{			
			if(num_readed_byte%BLOCKSIZE == 0 && num_readed_byte!=0 )
				continue;
			
			if(n<BLOCKSIZE-num_readed_byte%BLOCKSIZE)
				k=n;
			else
				k=BLOCKSIZE-num_readed_byte%BLOCKSIZE;
			
			for(int j=0; j<k ;j++)
			{
				byte_count = j + num_readed_byte%BLOCKSIZE;			
				*((char*)buf +j) = *( (char*)block +  byte_count);
				count++;
			}
		}
		else 
		{
			//if the block to be read is not the first one or the last one read the  whole block
			if(i<num_needed_block+first_read_block-1)
			{
				{
					count1 = count;
					for(int j=0;j<BLOCKSIZE;j++)
					{	
						*((char*)buf +j+count1) = *( (char*)block + j);
						count++;
					}
				}
			}
			else
			{
				count2 =count;
				for(int j=0; j<( (n-1+num_readed_byte)%1024+1) ;j++)
				{		
					*((char*)buf +j+count2) = *( (char*)block + j);
					count++;
				}
			}
		}		
	}
	//increase the number of read bytes in open file table 
	(OFT+fd)->fpp +=n;
	free(block);
	
	// return the number of read bytes
    return (count); 
}


int sfs_append(int fd, void *buf, int n)
{
	int free_block_count = 0;
	int* ptr;
	int j=0;
	int i=0;
	int found=0;
	int write_data_block;
	int buf_num=0;
	char* copy_block_index;
	int full =0;
	
	int* superblock = malloc(BLOCKSIZE);
	int *block = malloc(BLOCKSIZE);
	int *block_dir = malloc(BLOCKSIZE);
	int *block_data = malloc(BLOCKSIZE);
	int first_free_block;
	int last_free_block;
	int FAT_block_num;
	int *block_FAT = malloc(BLOCKSIZE);
	read_block(superblock,0);
	int h;
	int not_found =0;
	int end_copy_index;
	int remain_block_space;
	
	int num_need_blocks = (n-1)/BLOCKSIZE+1;
	int num_dir_block = (superblock[0]-1)/(BLOCKSIZE/8)+1;
	int dir_per_block = BLOCKSIZE/8;
	int num_ava_blocks = (size/BLOCKSIZE - 1032);

	if(block == NULL && block_dir ==NULL && block_data == NULL &&superblock == NULL)
	{
		printf("\nsfs_append: Error while appending new data");
		return -1;
	}

	
	if( (OFT+fd)->mode != MODE_APPEND)
	{
		printf("\nsfs_append: File open mode is not in append mode");
		return -1;
	}
	
	if((OFT+fd)->valid == 0)
	{
		printf("sfs_append: file is not valid");
		return -1;
	}

	else	
	{
			//if the file size is zero searchs whole data blocks to find a big enough free space to append new data
			if((OFT+fd)->size == 0)
			{
				for(int i_=0;i_<num_dir_block;i_++)
				{
					i = i_;
					if(full)
						break;
					
					read_block(block,i_+8);
					for(int j_=0;j_<dir_per_block;j_++)
					{	
				
						if( (i_*(BLOCKSIZE/8)+j_) >= num_ava_blocks)
						{
							full=1;
							break;
						}
						
						j=j_;
						ptr = block+j_*2 ;
						if(*ptr == 0)
						{
							if (free_block_count == 0)
								first_free_block = i*(BLOCKSIZE/8) + j;
							
							free_block_count++;
							if(free_block_count >= num_need_blocks)
							{
								last_free_block = i*(BLOCKSIZE/8) + j;
								found=1;
								break;
							}
						}
						else
							free_block_count =0;
					}
					if(found==1)
						break;
				}
					//if find free space allocate data and update directory blocks and FAT blocks
					if(found==1)
					{
						for (int k=0;k<num_dir_block;k++)
						{
							read_block(block_dir,k+8);
							for(int p=0;p<dir_per_block;p++)
							{
								
								if(k*(BLOCKSIZE/8)+p >= first_free_block && (k*(BLOCKSIZE/8)+p <= last_free_block))
								{									
									if((k*(BLOCKSIZE/8)+p) == last_free_block)
									{
										*(block_dir+2*p+1) = n%BLOCKSIZE;
										end_copy_index = n%BLOCKSIZE;
									}
									else
									{
										*(block_dir+2*p+1) = BLOCKSIZE;
										end_copy_index = BLOCKSIZE;
									}
									
									*(block_dir+2*p) = 1;
									
									
									//copy buffered data into to allocated block and write it into data blocks
									copy_block_index = (char*)buf + buf_num*BLOCKSIZE ;
									write_data_block=first_free_block +buf_num+1032;
									
									copy_block(block_data, (int*)copy_block_index,0,end_copy_index);
									write_block(block_data,write_data_block);
									buf_num++;
								}
							}
							write_block(block_dir,k+8);
						}
						//Update FOT
						(OFT+fd)->start_block_num = first_free_block;
						(OFT+fd)->last_block_num = last_free_block;
						(OFT+fd)->size = n;
						FAT_block_num = (OFT+fd)->file_num/(BLOCKSIZE/128) + 1;
						read_block(block_FAT,FAT_block_num);
						ptr =block_FAT + ((OFT+fd)->file_num%(BLOCKSIZE/128)) * 128 / sizeof(int);
						*((int*)((char*)ptr+32+2*sizeof(int))) = n;
						*((int*)((char*)ptr+32+3*sizeof(int))) = first_free_block;
						*((int*)((char*)ptr +32 + 4*sizeof(int))) = last_free_block; 
						write_block(block_FAT,FAT_block_num);
					}
					else
					{
						printf("\nsfs_append: There is no enough space to append new data");
						return -1;
					}
			}
			
			//if the file size is not zero find the next free blocks to allocate new data
	 		else
			 {
				 remain_block_space =BLOCKSIZE - ( (OFT + fd)->size-1)%BLOCKSIZE -1;
				num_need_blocks = ( n - ( BLOCKSIZE - (OFT + fd)->size%BLOCKSIZE ) )  / BLOCKSIZE +1;
				
				//if the size of new data is bigger than the free space of the last allocated data block for the file
				//searchs later blocks to check whether it is big enough or not
				if(n > remain_block_space)
				{								
					for(int i_=( (OFT + fd)->last_block_num+1)/(BLOCKSIZE/8);i_<num_dir_block;i_++)
					{
						if(full)
							break;
						
						i = i_;
						read_block(block , i_+ 8);
						
						if(i_ == ( (OFT + fd)->last_block_num+1)/(BLOCKSIZE/8))						
							h = ( ( (OFT + fd)->last_block_num +1))% (BLOCKSIZE/8);
						
						else 
							h =0;
						for(int j_ = h ; j_<dir_per_block;j_++)
						{	
						
						if( (i_*(BLOCKSIZE/8)+j_) >= num_ava_blocks)
						{
							full=1;
							break;
						}
						
							j=j_;
							ptr = block+j_*2 ;
							if(*ptr == 0)
							{				
								if (free_block_count == 0)
									first_free_block = i*(BLOCKSIZE/8) + j;
								
								free_block_count++;
								if(free_block_count >= num_need_blocks)
								{
									last_free_block = i*(BLOCKSIZE/8) + j;
									found=1;
									break;
								}
							}
							else
							{
							
							not_found=1;						
							break;	
							}
							
						}
						if(not_found || found )
							break;
					}
					
					if(not_found)
					{
						printf("\nsfs_append: There is no enough space to append new data");
						return -1;
					}
					else
					{
						//if there is enough space allocate new data
						if(found==1)
						{

							for (int k=0;k<num_dir_block;k++)
							{
								for(int p=0;p<dir_per_block;p++)
								{
									//allocate the first data block
									if(k*(BLOCKSIZE/8)+p >= first_free_block -1 && (k*(BLOCKSIZE/8)+p <= last_free_block))
									{
										read_block(block_dir,k+8);	
										if(k*(BLOCKSIZE/8)+p == first_free_block -1)
										{
											*(block_dir+2*p+1) = BLOCKSIZE;	
											copy_block_index = (char*)buf ;
											write_data_block=first_free_block -1 + 1032;
											
											read_block(block_data,write_data_block);
											copy_block(block_data, (int*)copy_block_index, (OFT + fd)->size%BLOCKSIZE, remain_block_space);
											write_block(block_data,write_data_block);
										}
										
										//allocate later data blocks
										else
										{
											if(k*(BLOCKSIZE/8)+p == last_free_block)
											{
												*(block_dir+2*p) = 1;
												*(block_dir+2*p+1) =(n - ( BLOCKSIZE - (OFT + fd)->size%BLOCKSIZE )) % BLOCKSIZE;
												
												copy_block_index = (char*)buf +remain_block_space + buf_num*BLOCKSIZE ;
												end_copy_index = (n - 1 - remain_block_space)%BLOCKSIZE +1;
												write_data_block=last_free_block + 1032;

												copy_block(block_data, (int*)copy_block_index, 0 , end_copy_index);
												write_block(block_data,write_data_block);
											}

											else
											{
												*(block_dir+2*p+1) = BLOCKSIZE;
												*(block_dir+2*p) = 1;
													
													copy_block_index = (char*)buf +remain_block_space + buf_num*BLOCKSIZE ;
													write_data_block=first_free_block +buf_num+1032;
													read_block(block_data,write_data_block);
													
													
													read_block(block_data,write_data_block);
													copy_block(block_data, (int*)copy_block_index, 0, BLOCKSIZE);
													write_block(block_data,write_data_block);
													buf_num++;
												}
										}
										write_block(block_dir,k+8);
									}
								}
							}
					//	Update FOT
						(OFT+fd)->last_block_num = last_free_block;
						(OFT+fd)->size =(OFT+fd)->size+ n;
						FAT_block_num = (OFT+fd)->file_num/(BLOCKSIZE/128) + 1;
						read_block(block_FAT,FAT_block_num);
						ptr =block_FAT + ((OFT+fd)->file_num%(BLOCKSIZE/128)) * 128 / sizeof(int);
						*(int*)((char*)ptr+32+2*sizeof(int)) = (OFT+fd)->size;
						*(int*)((char*)ptr+32+4*sizeof(int)) = last_free_block;
						write_block(block_FAT,FAT_block_num);
						}
					}
				}
				else
				{
					for (int k=0;k<num_dir_block;k++)
					{
						for(int p=0;p<dir_per_block;p++)
						{									
							if(k*(BLOCKSIZE/8)+p == (OFT+fd)->last_block_num)
							{
								read_block(block_dir,k+8);	
								*(block_dir+2*p+1) =n + (OFT+fd)->size%BLOCKSIZE;
								write_block(block_dir,k+8);
								
								copy_block_index = (char*)buf ;
								write_data_block = (OFT+fd)->last_block_num + 1032;
								
								read_block(block_data,write_data_block);
								copy_block(block_data, (int*)copy_block_index, (OFT + fd)->size%BLOCKSIZE,n);
								write_block(block_data,write_data_block);
							}
						}
					}
						(OFT+fd)->size =(OFT+fd)->size+ n;
						FAT_block_num = (OFT+fd)->file_num/(BLOCKSIZE/128) + 1;
						read_block(block_FAT,FAT_block_num);
						ptr =block_FAT + ((OFT+fd)->file_num%(BLOCKSIZE/128)) * 128 / sizeof(int);
						*(int*)((char*)ptr+32+2*sizeof(int)) = (OFT+fd)->size;
						write_block(block_FAT,FAT_block_num);
				}
			} 
		
	}
	
	free(block);
	free(block_dir);
	free(block_data);
	free(superblock);
	
    return (n); 
}

int sfs_delete(char *file)
{
	void* block = malloc(BLOCKSIZE);
	int found =0;
	char*ptr;
	int count;
	int block_number = 0;
	int h;
	int dir_block;
	int finish=0;
	int dir_offset;
	int first_data_block;
	int last_data_block;
	int str_len;
	
	if(block==NULL)
	{
		printf("\nsfs_delete: Error while deleting file");
		return -1;
	}
	
	//search to find a file with match name
	while(!found && block_number<7)
	{
		block_number++;
		read_block(block,block_number);
		for(int i=0;i<BLOCKSIZE/128;i++)
		{
			count=0;
			ptr =(char*) block + i*128;
			if( *((int*)(ptr+32+sizeof(int))) )
			{
				if(strlen(file) ==*((int*)(ptr+32)))
				{
					for(int j=0;j<strlen(file);j++)
					{
						if(*(ptr+j) == file[j])
						{
							count++;
						}
					}
					if(count==strlen(file))
					{
						found=1;
						break;
					}
				}
			}
		}
	}

	if(found)
	{
		//checks whether the file is open or not
		//if open it won't delete the file
		for(int i=0;i<10;i++)
		{
			count =0;
			if((OFT+i)->valid)
			{
				str_len =strlen((OFT+i)->name);
				if(str_len == strlen(file))
				{
					for(int j=0;j<str_len;j++)
					{
						if(*( (OFT+i)->name+j) == file[j])
							count++;
					}
					if(count == strlen(file))
					{
						printf("\nsfs_delete: you can't delete an opened file");
						return -1;
					}
				}
			}
		}
			//if found the file sets corresponding FAT information and directory blocks as free for later uses
		*((int*)(ptr+32+sizeof(int))) =0;
		first_data_block =*((int*)(ptr +32 + 3*sizeof(int))); 
		last_data_block = *((int*)(ptr +32 + 4*sizeof(int))); 
		dir_block = first_data_block/(BLOCKSIZE/8) +8;
		dir_offset = first_data_block % (BLOCKSIZE/8);
		write_block(block,block_number);
		for (int i=dir_block;i<1032;i++)
		{
			read_block(block,i);
			if(i==dir_block)
				h=dir_offset;
			else
				h=0;
			for(int j=h; j<(BLOCKSIZE/8); j++)
			{
				if( (i-8)*(BLOCKSIZE/8)+j > last_data_block)
				{
					finish=1;
					break;
				}
				
				*((int*)block+j*(BLOCKSIZE/128)/sizeof(int)) = 0;
			}
			if(finish)
				break;
		}
	}
	
	else
	{
		printf("\nsfs_delete: There is no such file");
		return -1;
	}
	free(block);
	
	return (0); 
}

//returns number of allocated files
int num_alloc_files_in_dir()
{
	int *num_files = malloc(BLOCKSIZE);
	read_block(num_files,0);
	return num_files[1];
}

//copy data in the buffer to the allocated memoey space
void copy_block( int* block, int* source_block,int data_start,int buf_size)
{
	char *a = (char*)block+data_start;
	char *b = (char*)source_block;
	for (int i=0;i<buf_size;i++)
	{
		a[i] = b[i];
	}
}
//prints all files information 
void print_file()
{
	int *block = malloc(BLOCKSIZE);
	int ofsset_size;
	int ofsset_state;
	int ofsset_name_length;
	int ofsset_name;
	int ofsset_first_block;
	int ofsset_last_block;
	char* ptr = (char*)block;
	
	for (int i=1;i<8;i++)
	{
	read_block((void*)block,i);
		for (int j =0;j<BLOCKSIZE/128;j++)
		{
			
			ofsset_size = (128*j + 32 +2*sizeof(int));
			ofsset_state = (128*j +32 + sizeof(int));
			ofsset_first_block =  (128*j + 32 +3*sizeof(int));;
			ofsset_last_block =  (128*j + 32 +4*sizeof(int));
			ofsset_name_length = (128*j + 32);
			ofsset_name = (128*j );
			ptr = (char*)block;
			if(*(int*)(ptr + ofsset_state))
			{
				printf("\nsize = %d",*(int*)(ptr + ofsset_size));
				printf("  length = %d",*(int*)(ptr + ofsset_name_length));
				printf("  first = %d",*(int*)(ptr + ofsset_first_block));
				printf("  last = %d",*(int*)(ptr + ofsset_last_block));
				printf("  name is: ");
				for (int i=0;i<*(int*)(ptr + ofsset_name_length);i++)
					printf("%c",*(ptr+ofsset_name+i));
			}
		}
	}
}

//print all allocated data
void print_data()
{
	int* block_dir = malloc(BLOCKSIZE);
	char* block_data = malloc(BLOCKSIZE);
	static int count=0;
	for(int i=0;i < (size/BLOCKSIZE -1032) / (BLOCKSIZE/8); i++)
	{
		read_block(block_dir,i+8);
		for(int j=0;j<BLOCKSIZE/8;j++)
		{
			if( *(block_dir + j*2) )
			{
				count++;
				read_block(block_data,i*BLOCKSIZE/8 +j +1032);
				for(int k=0;k<*(block_dir + j*2+1);k++ )
				{
					printf("%c",*((char*)(block_data + k)));
				}
			}
 		}
	}
}

//print FOT content
void print_FOT()
{
	for(int i=0;i<56;i++)
	{
		if((OFT+i)->valid == 1)
		{
			printf("\nsize %d mode %d name %s fd %d\tfile num %d start block %d end block %d",(OFT+i)->size,(OFT+i)->mode,(OFT+i)->name,(OFT+i)->fd,(OFT+i)->file_num,(OFT+i)->start_block_num,(OFT+i)->last_block_num);
		}
	}
}
