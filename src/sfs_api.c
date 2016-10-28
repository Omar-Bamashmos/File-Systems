//********************************************
//Operating Systems
//ECSE 427
//Name: Omar Ba mashmos
// student ID#: 260617216
//*********************************************

#include "sfs_api.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/time.h>
#include "disk_emu.h"
#include <string.h>
#include <fuse.h>
#include <unistd.h>
//#include "disk_emu.c"




// root inode number
#define rootInode 0
//used 0, free 1							
#define Used 0								
#define Free 1 
// total size of disk									
	
#define maxNumber_openFiles 120
// to be assined to int if not used yet (default of 0 is risky) 
#define ND 9999	
#define blockSize 1024

#define dataBlocks 220
#define metaBlocks 32		//takes care of non user data. for example i-bitmap, super block, inodes

#define maxBlocks	dataBlocks + metaBlocks 

// memory inode table pointer (array of structs)
//ND FOR MEMBDERS NOT YET DEFINED
char *inode_table_ptr;				
int inode_table_blocks_num = ND; 	
int inode_table_disk_loc = ND;				
int d_bitmap_blocks_num = ND;
int d_bitmap_disk_loc = ND;
int i_bitmap_blocks_num = ND;			
int i_bitmap_disk_loc = ND;	
int directory_blocks_num = ND;
int db_start  = ND;					

fileDescriptor fdt[maxNumber_openFiles];
iNode inode_table[dataBlocks];
directoryEntry directory[dataBlocks];			
superBlock super_blockPointer;
int directoryLocation =0;

int i_bitmap[dataBlocks];
int d_bitmap[dataBlocks];

int set_ind_ptr();
int get_free_data_block();
int free_used_data_block(int block);
int maxOf(int a, int b);

void super_block_initialization() {
		// Initialize members of thy super block
		super_blockPointer.magic_num = 0xACBD0005;
		super_blockPointer.blockNumber = maxBlocks;
		super_blockPointer.dataBlockNum = dataBlocks;
		super_blockPointer.block_size = blockSize;
		super_blockPointer.inode_tableLocation = i_bitmap_blocks_num + d_bitmap_blocks_num+1;
		super_blockPointer.inode_table_sizeBlocks = inode_table_blocks_num;
		super_blockPointer.node_root_directory = rootInode;
}


void mksfs(int fresh)
{
	// new disk
	//add 1 to elimnate rounding down error
	inode_table_blocks_num = (sizeof(inode_table)/blockSize) + 1;

	
	i_bitmap_blocks_num = (sizeof(i_bitmap)/blockSize) + 1; 
	
	d_bitmap_blocks_num = (sizeof(d_bitmap)/blockSize) + 1; 
	directory_blocks_num = (sizeof(directory)/blockSize) + 1;

	db_start = 1 + inode_table_blocks_num + i_bitmap_blocks_num + d_bitmap_blocks_num +9; 
	
	if(inode_table_blocks_num == ND || d_bitmap_blocks_num == ND||i_bitmap_blocks_num == ND||directory_blocks_num == ND || db_start  == ND)
	{
		printf("Please define all the initiators!\n");
	}

	int i;

	// file descriptor table reset all free
	for (i=0;i<maxNumber_openFiles;i++)
	{
		fdt[i].status = Free;
		fdt[i].inode_num = ND;
	}

	

	// fresh start
	if (fresh) {
		init_fresh_disk("myDisk", blockSize, maxBlocks);
		//printf("new Disk is openning\n");
		
		// memory elements
		// initialize bitmaps to 0 AND directory map
		for (i = 0; i < dataBlocks; i++ ) {
			i_bitmap[i] = Free;
			d_bitmap[i] = Free;
			directory[i].inode_num = ND;
		}

		// inode_table
		for (i=0; i< dataBlocks; i++){
			inode_table[i].iNum = i;
			inode_table[i].byte_size = 0;
			inode_table[i].directory = -1;	
			int j;
			for (j = 0; j < 12; j++)
			{
				inode_table[i].direct_p[j]=ND;
			}
			inode_table[i].ind_p=ND;
			
		}
		
		// super block initialization
		super_block_initialization();
		//printf("initialization super block\n");
	    // memory elements

		fdt[0].status = Used;
		fdt[0].inode_num = rootInode;
		fdt[0].rw_ptr = 0;
		fdt[0].inode = &inode_table[0];
		fdt[0].inode->directory = 1;
		fdt[0].inode->byte_size = sizeof(directory);
		if (directory_blocks_num<=12){
			for(i=0;i<directory_blocks_num;i++){
				// reserve first x blocks for directory (to store map)
				fdt[0].inode->direct_p[i]=db_start+i;				
				// mark blocks as used
				d_bitmap[i] = Used;												
			}
		}
		else{
			set_ind_ptr(fdt[0].inode);
		}
		// store memory map in directory datablock
		// new inode for rootdirectory 
		i_bitmap[0] = Used;

		write_blocks(0, 1,  (void*) &super_blockPointer);
		write_blocks(1, i_bitmap_blocks_num, i_bitmap);
		write_blocks(1 + i_bitmap_blocks_num, d_bitmap_blocks_num, d_bitmap);
		write_blocks(1 + i_bitmap_blocks_num + d_bitmap_blocks_num, inode_table_blocks_num, inode_table);
		write_blocks(db_start, directory_blocks_num, directory);

	}
	else {
		init_disk("myDisk", blockSize, maxBlocks);
	
 

	}
}

int sfs_get_next_filename(char *fname) {
	if (directoryLocation == dataBlocks || directory[directoryLocation].inode_num==ND || directory[directoryLocation].fileName==NULL){
		printf("OOPS! No next file!\n");
		return 1;	
	}
	printf("i: %s\n", directory[directoryLocation].fileName);
	printf("%d\n", directoryLocation);
	//strncpy(fname, directory[directoryLocation].fileName, MAXFILENAME + 1); //copy filename over and increment
	directoryLocation++;
	printf("current directory location: %d \n", directoryLocation);
	return 0;
	
}


int sfs_GetFileSize(const char* path) {

	int i;
	for(i=0;i<dataBlocks;i++){
		// take into accoutn the /0 terminator
		if (strncmp(directory[i].fileName , path , MAXFILENAME)==0){
			int in = directory[i].inode_num;
			int size = inode_table[in].byte_size;
			return size;
		}
	}
	
	return 0;
}


int sfs_fopen(char *name) {

	// check to see if filename is of correct size
	if(strlen(name) > MAXFILENAME){
		printf("File name is too long\n");
		return -1;
	}

	// check if file exists => directory map search
	int i;
	for(i=0;i<dataBlocks;i++){	
		if (strncmp(directory[i].fileName , name , MAXFILENAME)==0){
			// file name exists
			int in_num = directory[i].inode_num;
			// check if its already open
			int j;

			for (j=0;j<maxNumber_openFiles;j++){
				if(fdt[j].inode_num == in_num){
					printf("The file is already open\n");
					return j;
				}
			}

		    
			for(j=0;j<maxNumber_openFiles;j++){
				// get a free entry
				if (fdt[j].status == Free) {
					fdt[j].status = Used;
					fdt[j].inode_num = in_num;
					fdt[j].inode = &inode_table[in_num];
					fdt[j].rw_ptr = inode_table[in_num].byte_size;
					return j;
				}
			}
			// did not find place for file otherwise would already return
			printf("File Descritor table full! Please close files\n");
			return -1;
		}
	}
	//File does not exist, create a new file
	int k;
	int in_num = ND;
	
	for(k=0;k<dataBlocks;k++){
		// is there an empty block
		if(i_bitmap[k] == Free){
			inode_table[k].directory =0;
			inode_table[k].byte_size = 0;
			i_bitmap[k] =Used;
			in_num = k;
			break;
		}
	}
	if (in_num == ND){
		printf("The inode table full!\n");
		return -1;
	}
	
	// add entry to directory
	for(k=0;k<dataBlocks;k++){
		if(directory[k].inode_num == ND) {
			directory[k].inode_num =in_num;
			strncpy(directory[k].fileName, name, MAXFILENAME+1);
			break;
		}
	}
	
	// create entry in location inf the file descriptor table
	for(k=0;k<maxNumber_openFiles;k++)
	{
		// find first free entry
		if (fdt[k].status == Free) 				
		{
			fdt[k].status = Used;
			fdt[k].inode_num = in_num;
			fdt[k].inode = &inode_table[in_num];
			fdt[k].rw_ptr = inode_table[in_num].byte_size;

			// rewrite all modified data
			write_blocks(1, i_bitmap_blocks_num, i_bitmap);
			
			write_blocks(1+i_bitmap_blocks_num+d_bitmap_blocks_num , inode_table_blocks_num , inode_table);
			write_blocks(db_start, directory_blocks_num, directory);
			return k;			
		}
	}

	printf("File Descritor table full, empty spot and try again\n");
	return -1;
}


int sfs_fwrite(int fileID, const char *buf, int length){

	// check parameters
	if (fdt[fileID].status == Free){
		printf("File is closed. can not write\n");
		return -1;
	}

 	// check if its a new file (no block assigned yet)
 	if (fdt[fileID].inode->byte_size ==0){
 		// assign free block
 		fdt[fileID].inode->direct_p[0] = get_free_data_block();
 	}

	// get block to read from
	int block_write_from = fdt[fileID].rw_ptr/blockSize; 
	int byte_write_from = fdt[fileID].rw_ptr%blockSize;
	int block_write_to = (fdt[fileID].rw_ptr + length)/blockSize;
	int byte_write_to = (fdt[fileID].rw_ptr + length)%blockSize;

	// get free blocks and store in array
	int i;
	iNode *in = fdt[fileID].inode;

	for (i=block_write_from;i<=block_write_to;i++){
		// direct pointer
		if (i<12){
			// check if inode contains a direct pointer for that block
			if(in->direct_p[i] == ND){
				in->direct_p[i]=get_free_data_block();
				if (in->direct_p[i] <0){
					printf("read block due to no free spaces\n");
					return -1;
				}
			}
			
		}

		// indirect pointer
		else{	
			//  indirect pointer  not defined
			if(in->ind_p == ND) {
				// returns index of block of pointers
				in->ind_p = set_ind_ptr(); 
				if ( in->ind_p<0){
					printf("No free space to read\n");
					return -1;
				}
			}
			// block defined and contains values
			if(in->ind_p != ND) {
				//get the block, rewrite/add values, write block
				int *indptr_buff = malloc(blockSize);
				read_blocks(db_start + in->ind_p, 1, indptr_buff);
				if (indptr_buff[i-12] ==ND) {
					indptr_buff[i-12] = get_free_data_block();
					if (indptr_buff[i-12] <0){
						printf("read block due to no free spaces\n");
						return -1;
					}
					write_blocks(db_start + in->ind_p, 1, indptr_buff);
				}
				// if block is defined, we dnt need to define one. the defined one will be overwritten
				free(indptr_buff);
			}
		}
	}

	int write_block_num;
	int start_index;
	int end_index;
	int target_ptr;
	target_ptr=0;

	//start writing
	for (i=block_write_from;i<=block_write_to;i++){
		// get index of block to write on
		if(i < 12){
			write_block_num = in->direct_p[i];
		}
		else{
			int *indptr_buff = malloc(blockSize);
			read_blocks(db_start + in->ind_p, 1, indptr_buff);
			write_block_num = indptr_buff[i - 12];
			free(indptr_buff);
		}


		if (write_block_num ==ND){
			printf("i-node definition problem!\n");
			return -1;
		}

		char *one_block = malloc(blockSize);
		read_blocks(db_start+write_block_num,1,one_block);
		// block content is in memory, Modify and re-write

		// determine where to start writing
		start_index=0;
		end_index=blockSize;
		if(i==block_write_from){
			start_index=byte_write_from;
		}
		if(i==block_write_to){
			end_index=byte_write_to;
		}
		// modify memory cache and rewrite to block
		memcpy(&one_block[start_index], &buf[target_ptr], end_index-start_index);
		write_blocks(db_start+write_block_num,1,one_block);
		target_ptr = target_ptr+ (end_index-start_index);
		free(one_block);	
	}
	//modify size in the inode
	in->byte_size = maxOf(in->byte_size , fdt[fileID].rw_ptr+length);
	fdt[fileID].rw_ptr = fdt[fileID].rw_ptr +length;


	// inode table modified
	write_blocks(1+i_bitmap_blocks_num+d_bitmap_blocks_num , inode_table_blocks_num , inode_table);
	return target_ptr;
}

int sfs_fseek(int fileID, int loc){
	if (fdt[fileID].status == Free){
		printf("FILE WAS FREED, CAN NOT SEEK\n");
		return -1;
	}
	if (fdt[fileID].inode->byte_size < loc){
		printf("Can not seek to this location, pointer was set to end of file\n");
		fdt[fileID].rw_ptr = fdt[fileID].inode->byte_size ;
		return 0;
	}
	fdt[fileID].rw_ptr = loc;
	return 0;
}

int sfs_remove(char *file) {
	int i;
	char * emp="\0";
	for(i=0;i<dataBlocks;i++){
		if(strncmp(directory[i].fileName, file, MAXFILENAME) == 0 && directory[i].inode_num != ND) {
			// reset inode			
			iNode *in = &inode_table[directory[i].inode_num];
			in->byte_size=0;
			in->directory=-1;

			int j;
			for (j = 0; j < 12; j++){
				if (in->direct_p[j] != ND){
					free_used_data_block(in->direct_p[j]);
					in->direct_p[j]=ND;
				}
			}
			
			if (in->ind_p !=ND){
				free_used_data_block(in->ind_p);
				in->ind_p=ND;
			}

			// reset i_bitmap value
			i_bitmap[directory[i].inode_num] = Free;

			// remove file from directory
			strncpy(directory[i].fileName, emp, MAXFILENAME + 1); //copy filename over and increment
			directory[i].inode_num = ND;
		
			// write all modifications
			write_blocks(1, i_bitmap_blocks_num, i_bitmap);
			// write_blocks(1+i_bitmap_blocks_num , d_bitmap_blocks_num , d_bitmap);
			write_blocks(1+i_bitmap_blocks_num+d_bitmap_blocks_num , inode_table_blocks_num , inode_table);
			write_blocks(db_start, directory_blocks_num, directory);	
		}
	}	
	return 0;
}

int sfs_fread(int fileID, char *buf, int length){
	//check parameters
	if (fdt[fileID].status == Free)
	{
		printf(" File is closed. can not write\n");
		return -1;
	}

	if(fdt[fileID].rw_ptr + length > fdt[fileID].inode->byte_size)
	{
		//intf("Length is more than file size\n");
		length = fdt[fileID].inode->byte_size - fdt[fileID].rw_ptr;
	}

	//get block to read from
	int block_read_from = fdt[fileID].rw_ptr/blockSize;
	int byte_read_from = fdt[fileID].rw_ptr%blockSize;
	int block_read_to = (fdt[fileID].rw_ptr + length)/blockSize;
	int byte_read_to = (fdt[fileID].rw_ptr + length)%blockSize;

	int i;
	int read_block_num;
	int start_index;
	int end_index;

	iNode *in = fdt[fileID].inode;
	char *target = malloc(length);
	int target_ptr;
	target_ptr=0;
	
	for (i=block_read_from;i<=block_read_to;i++) {
		char *one_block = malloc(blockSize);
		// check if block is directly pointed
		if(i < 12){
			read_block_num = in->direct_p[i];
		}
		else{
			int *indptr_buff = malloc(blockSize);
			// assumes fsf_write istanciated an inderiect pointer block
			read_blocks(db_start + in->ind_p, 1, indptr_buff);		
			read_block_num = indptr_buff[i - 12];
			free(indptr_buff);
		}

		// read block from pointer
		read_blocks(db_start+read_block_num,1,one_block);


		// determine what part to copy from block buffer one_block
		start_index=0;
		end_index=blockSize;	
		if(i==block_read_from){
			start_index=byte_read_from;
		}
		if(i==block_read_to){
			end_index=byte_read_to;
		}

		// copy target memory to target from each block buffer
		memcpy(&target[target_ptr], &one_block[start_index], end_index-start_index);
		target_ptr = target_ptr+ (end_index-start_index);
		free(one_block);
	}
	// copy everything into buf
	memcpy(buf,target,length);
	// clean
	free(target);
	fdt[fileID].rw_ptr = fdt[fileID].rw_ptr +length;

	return target_ptr;
}



int sfs_fclose(int fileID){
	if (fdt[fileID].status == Free){
		printf("File is already closed\n");
		return -1;
	}
	fdt[fileID].status = Free;
	fdt[fileID].inode_num = ND;
	fdt[fileID].inode = NULL;
	fdt[fileID].rw_ptr=0;
	return 0;
}



//st the indirect point function
int set_ind_ptr(){
	int block = get_free_data_block();
	if (block <0){
		return -1;
	}
	int i;
	int *indptr_buff = malloc(blockSize);
	
	for (i = 0; i < (blockSize/sizeof(int)); i++){
		indptr_buff[i] = ND;
	}
	write_blocks(db_start + block, 1, indptr_buff);
	free(indptr_buff);
	return block;
}

int free_used_data_block(int block){
	d_bitmap[block] = Free;
	write_blocks(1+i_bitmap_blocks_num , d_bitmap_blocks_num , d_bitmap);
	return 0;
}

int maxOf(int a,int b){
	if (a>b){
		return a;
	}
	else{
		return b;
	}
}

int get_free_data_block(){
	int i;
	for (i=0;i<dataBlocks;i++){
		if (d_bitmap[i]==Free){
			d_bitmap[i] = Used;
			// save to disk
			write_blocks(1+i_bitmap_blocks_num , d_bitmap_blocks_num , d_bitmap);
			return i;
		}
	}
	printf("No more free data blocks! disk is full\n");
	return -1;
}




