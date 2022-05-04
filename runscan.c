#include <stdio.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <string.h>
#include <dirent.h>
#include <errno.h>
/*
* For a directory entry (that in fact is a jpg file) and two strings makes two paths: file-inode.jpg and name-file.jpg
*
*/
void jpg( struct ext2_dir_entry* dir_tmp, char *pathFile, char * path)
{

	strncat(pathFile, "/file-", 7);
	char b[10];
	snprintf(b, 10, "%u", dir_tmp->inode);
	strcat(pathFile, b);
	strncat(pathFile, ".jpg", 5);
	//printf("PATHFILE __%s\n", pathFile);
	int fd2 = open(pathFile, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
	close(fd2);
	strncat(path, "/", 2);
	strncat(path, dir_tmp->name, dir_tmp->name_len);
int     fd1 = open(path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
//	printf("PATHFILE __%d\n", fd1);
	//fd1=0;
	//if (fd1) printf("ha");
	close(fd1);

}
//code from specs, checks if this is an image
int is_pic(char * buffer_tmp)
{
	if (buffer_tmp[0] == (char)0xff &&
			buffer_tmp[1] == (char)0xd8 &&
			buffer_tmp[2] == (char)0xff &&
			(buffer_tmp[3] == (char)0xe0 ||
			 buffer_tmp[3] == (char)0xe1 ||
			 buffer_tmp[3] == (char)0xe8)) {
		return 1;
	}
	return 0;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("expected usage: ./runscan inputfile outputfile\n");
		exit(0);
	}
	if(!opendir(argv[2])) mkdir(argv[2], S_IRWXU); //if the output directory doesn't exist, create one. 
	int fd;

	fd = open(argv[1], O_RDONLY);    /* open disk image */

	ext2_read_init(fd);

	struct ext2_super_block super;
	struct ext2_group_desc group;

	// example read first the super-block and group-descriptor
	read_super_block(fd, 0, &super);
	read_group_desc(fd, 0, &group);

	printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
	//iterate the first inode block
	off_t start_inode_table = locate_inode_table(0, &group);
	for (unsigned int i =0 ; i < inodes_per_block; i++) {
		printf("inode %u: \n", i);
		struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
		read_inode(fd, 0, start_inode_table, i, inode);
		if(!S_ISDIR(inode->i_mode) && !S_ISREG(inode->i_mode))
		{
			printf("   Skipping this block %d since it is not a normal file\n", i);
			continue;
		}
		/* the maximum index of the i_block array should be computed from i_blocks / ((1024<<s_log_block_size)/512)
		 * or once simplified, i_blocks/(2<<s_log_block_size)
		 * https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
		 */
		unsigned int i_blocks = inode->i_blocks/(2<<super.s_log_block_size);
		printf("number of blocks %u\n", i_blocks);
		printf("Is directory? %s \n Is Regular file? %s\n",
				S_ISDIR(inode->i_mode) ? "true" : "false",
				S_ISREG(inode->i_mode) ? "true" : "false");
		//put the content of the first inode's block into a buffer
		char buffer[1024];
		lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);   
		read(fd,&buffer,1024);
		//if that's a directory (apparently this is when we enter the root directory idk)
		if(S_ISDIR(inode->i_mode)){
			//cast our buffer to an entry struct
			//struct ext2_dir_entry* dir = (struct ext2_dir_entry*) buffer;
			//when we iterate through the directory we go by offset, check the diagram for a directory in specs
			int offset_total =24;// (int) dir->rec_len;
			
			while(offset_total<1024)//bc the block is 1024 long
			{
				//traverse through the directory and see what is a jpg. 
				//we're doing the same thing we just did outside the cycle but inside the dir
				struct ext2_dir_entry* dir_tmp = (struct ext2_dir_entry*) (buffer+offset_total);
				char buffer_tmp[1024];
				struct ext2_inode *inode_tmp =malloc(sizeof(struct ext2_inode));
				read_inode(fd, 0, start_inode_table, dir_tmp->inode, inode_tmp);
				lseek(fd, BLOCK_OFFSET(inode_tmp->i_block[0]), SEEK_SET);
				read(fd,&buffer_tmp,1024);
				if (dir_tmp->inode ==0) break; //bc it's empty 
				printf("inside loop inode %u, offset = %d\n", dir_tmp->inode, offset_total);                                

				if(S_ISREG(inode_tmp->i_mode) && is_pic(buffer_tmp)) //we found a picture!
				{ 
					printf("jpg file inside the root dir, inode %u\n", dir_tmp->inode);
					//set paths so that they start with the output directory name
					char* pathFile= malloc(260);
					strcpy(pathFile, argv[2]);
					char* path = malloc(260);
					strncpy(path, argv[2], strlen(argv[2])+1);					
					jpg(dir_tmp, pathFile, path);
					//COPYING
					printf("size of the file: %d\n", inode_tmp->i_size);	
					int target = open(path,O_WRONLY | O_APPEND);//open file to write and append so write in the end
					int target2 = open(pathFile,O_WRONLY | O_APPEND);
					write(target, buffer_tmp, (int) inode_tmp->i_size);
					 write(target2, buffer_tmp, (int) inode_tmp->i_size); 
					perror("the error is\n");//in case write fails
					free(pathFile);
					free(path);
				}/*
				else if(S_ISDIR(inode_tmp->i_mode)){//this is the subdir case and it should be the same as before but this doesn't work fully
					struct ext2_dir_entry* dir_sub = (struct ext2_dir_entry*) (buffer_tmp);
					int sub_offset=dir_sub->rec_len;
					printf("jpg file inside the subdir, inode %u\n", dir_sub->inode);
					char buffer_sub[1024];
					struct ext2_inode *inode_sub =malloc(sizeof(struct ext2_inode));;
					read_inode(fd, 0, start_inode_table, dir_sub->inode, inode_sub);
					lseek(fd, BLOCK_OFFSET(inode_sub->i_block[0]), SEEK_SET);
					read(fd,&buffer_sub,1024);
					if (dir_sub->inode ==0) break;
					if(is_pic(buffer_sub))
						printf("found a pic in subdir %u\n", dir_sub->inode);
					int add_sub = 8;
					if(dir_sub->name_len%4) add_sub+= dir_sub->name_len + (4-dir_sub->name_len%4);
					sub_offset +=  add_sub;
				}*/
				//we need to catch deleted files, check piazza post @1209
				int add = 8; 
				if(dir_tmp->name_len%4) add+= dir_tmp->name_len + (4-dir_tmp->name_len%4);
				offset_total +=  add;
				
			}
			//printf("----inside dir address %p\n", dir);

		} 



		// print i_block numberss
		for(unsigned int i=0; i<EXT2_N_BLOCKS; i++)
		{       if(i==0 && inode->i_block[i]==0)
			{

				printf("   Skipping this block %d since it starts with 0\n", i);
				break;
			}

			if (i < EXT2_NDIR_BLOCKS)                                 /* direct blocks */
				printf("Block %2u : %u\n", i, inode->i_block[i]);
			else if (i == EXT2_IND_BLOCK)                             /* single indirect block */
				printf("Single   : %u\n", inode->i_block[i]);
			else if (i == EXT2_DIND_BLOCK)                            /* double indirect block */
				printf("Double   : %u\n", inode->i_block[i]);
			else if (i == EXT2_TIND_BLOCK)                            /* triple indirect block */
				printf("Triple   : %u\n", inode->i_block[i]);

		};

		free(inode);
//might need to free buffers. using malloc ruins everything so need to do it in a loop?

	}


	close(fd);
}
