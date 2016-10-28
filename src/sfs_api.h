#define MAXFILENAME 20


typedef struct {	

	int magic_num;
	int blockNumber;						
	int dataBlockNum;				
	int block_size;					
	int inode_tableLocation;
	int inode_table_sizeBlocks;		
	int node_root_directory;	

} superBlock;			

typedef struct {

	int iNum; 								
	int byte_size;
	int directory;						
	int direct_p[12];						
	int ind_p;								
} iNode;	

typedef struct {
	int status;						
	int inode_num;							
	iNode* inode;
	int rw_ptr;
} fileDescriptor;

typedef struct {				
	int inode_num;
	char fileName[MAXFILENAME+1];
} directoryEntry;


void mksfs(int fresh);
int sfs_get_next_filename(char *fname);
int sfs_GetFileSize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);