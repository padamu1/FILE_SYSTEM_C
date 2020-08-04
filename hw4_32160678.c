#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <wait.h>
#include "fs.h"
struct partition new_part;
typedef struct thread{
	struct partition part;
	struct super_block super;
	struct inode *node;
	struct dentry *den;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int count;
	int full;
	
} so_t;

void *read_thread(void *arg){
	FILE *wfp;
	int ret;
	struct blocks buffer;
	struct blocks tempbuffer;
	so_t *so = arg;
	int this_thread = so->count; //file name make text -> text#
	char *thread_name = malloc(sizeof(char) * 10);
	so->count ++;
	int stack = 0;
	char close[30];
	char clof[30]="y";
	int pid;
	int file_open = 0;
	char *tok[100];
	char temp_name[100] = "temp";
	char temp_path[100] = "./temp";
	pthread_mutex_lock(&so->lock);
	while(so->full == 1){
		pthread_cond_wait(&so->cond,&so->lock);	
	}
	printf("inode : %d\n",(so->den)->inode);
	so->node = (struct inode *)&((so->part).inode_table[(so->den)->inode]);
	printf("size : %d ",(so->node)->size);
	
	buffer = (so->part).data_blocks[(so->node)->blocks[0x0]];
	printf("block : %s\n",buffer.d);
	// open temp file and write buffer's content
	sprintf(thread_name, "%d", this_thread);
	strcat(temp_name,thread_name);
	strcat(temp_path,thread_name);
	strcat(temp_name,".txt");
	strcat(temp_path,".txt");
	wfp = fopen(temp_name,"w");
	fwrite(&buffer.d,(so->node)->size,1,wfp);
	fclose(wfp);
	tok[0] = "vim";
	tok[1] = temp_path;
	// fork and start vim
      	pid = fork();
    	if(pid < 0) 
		{    
        		fprintf(stderr,"open file Failed");
        		exit(-1); 
    		} 
	else if (pid== 0) 
    	{
        	printf("vim start!!\n");
        	execv("/usr/bin/vim",tok);
    	} 
   	else 
    	{
        	wait(NULL); 
        	printf("vim Complete!\n");
		file_open = 1;
        
    	}
	if (file_open ==1){
		printf("if you want save and close typing 'y' else other\n");
		printf(": ");
		scanf("%s",close);
		if (!strcmp(clof,close)){
			//save file when user typing "y"
			wfp = fopen(temp_name,"rb");
			while(1){
				ret = fread(&tempbuffer, sizeof(tempbuffer),1,wfp);
				if (ret == 0) break;
			}
			printf("file saved \n");
			fseek(wfp,0,SEEK_END);
			//save size and data when size > 1024, program cut 1024 over data
			if(ftell(wfp) < new_part.s.block_size){
				new_part.inode_table[(so->den)->inode].size = ftell(wfp);
			}else
				new_part.inode_table[(so->den)->inode].size = new_part.s.block_size;
			printf("%s\n",tempbuffer.d);
			memcpy(new_part.data_blocks[(so->node)->blocks[0x0]].d,tempbuffer.d,new_part.inode_table[(so->den)->inode].size);
			fclose(wfp);		
		}
	}
	so->full = 0;
	pthread_cond_broadcast(&so->cond);
	pthread_mutex_unlock(&so->lock);
	pthread_exit(0);

}
void *find_thread(void *arg){
	so_t *so = arg;
	int stack = 0;
	char find[30];
	pthread_mutex_lock(&so->lock);
	//find file's metadata in block
	printf("please input want open file : "); 
	scanf("%s",find);	
	while(1){
		if (!strcmp((so->den)->name,find)){
			stack = 0;
			break;
		}		
		stack = stack+(so->den)->dir_length;
		so->den = (struct dentry *)((long long)(so->den)+((so->den)->dir_length));
		if(stack==((so->node)->size))break;
		if((so->den)->name[0] == 0)break;
	}
	so->full = 0;
	pthread_cond_broadcast(&so->cond);
	pthread_mutex_unlock(&so->lock);
	pthread_cond_wait(&so->cond,&so->lock);	
	pthread_exit(0);
}
void main(){
	//initial variable
	int cache_make = 0;
	struct cache_buffer cache;
	int count=0;
	int thread_num = 1;
	so_t *share = malloc(sizeof(so_t));
	pthread_t fthread;
	pthread_t rthread[2];
	struct partition part_init;
	FILE *fp;
	int *temp;
	int ret;
	int stack;
	int option;
	int firstcreate=1;
	int thread_create=0;
	//open disk.img
	fp = fopen("disk.img","rb");
	pthread_mutex_init(&share->lock, NULL);
    	pthread_cond_init(&share->cond,NULL);
	while(1){
		ret = fread(&part_init, sizeof(part_init),1,fp);
		if (ret == 0) break;
	}
	fclose(fp);	
	
	printf("root file list : \n");
	//saved first data to new_part(before used)
	new_part = part_init;
	while(1){
		part_init = new_part; // content update
		share->part = part_init;
		share->count = 0;
		share->full = 1;
		share->super = part_init.s;
		printf("free inode : %d free block : %d\n",(share->super).num_free_inodes,(share->super).num_free_blocks);
		share->node = (struct inode *)&part_init.inode_table[part_init.s.first_inode];
		share->den = (struct dentry *)&part_init.data_blocks[(share->node)->indirect_block];
		
		while(1){ /* list root file and make cache*/
			printf("root file name : %s\n",(share->den)->name);
			if(cache_make == 0){
				cache.buf[count].cache_inode = (share->den)->inode;
				cache.buf[count].cache_mode = part_init.inode_table[(share->den)->inode].mode;
				cache.buf[count].cache_locked = part_init.inode_table[(share->den)->inode].locked;
				cache.buf[count].cache_date = part_init.inode_table[(share->den)->inode].date;
				cache.buf[count].cache_size = part_init.inode_table[(share->den)->inode].size;
				cache.buf[count].cache_indirect_block = part_init.inode_table[(share->den)->inode].indirect_block;
				memcpy(cache.buf[count].cache_blocks,part_init.inode_table[(share->den)->inode].blocks,sizeof(part_init.inode_table[(share->den)->inode].blocks));
				count++;
			}
			
			stack = stack+(share->den)->dir_length;
			(share->den) = (struct dentry *)((long long)(share->den)+((share->den)->dir_length));
			if(stack==(share->node)->size)break;
			if((share->den)->name[0] == 0)break;
		}
		cache_make =1;
		share->den = (struct dentry *)&part_init.data_blocks[(share->node)->indirect_block];
		/* option select user want close program or open file */
		printf("select option 'program close' : 1, 'open file' : 2 \n:");
		scanf("%d",&option);
		if(option == 1){
			share->full = 1;
			break;		
		}
		else if(option == 2){
			thread_create = 1;
			for(int i = 0 ; i < thread_num ; i++)
				pthread_create(&rthread[i],NULL,read_thread,share);
			pthread_create(&fthread,NULL,find_thread,share);
			
		}
		if(thread_create == 1){
			for(int i = 0 ; i < thread_num ; i++)
				ret = pthread_join(rthread[i],(void **)&temp);
			ret = pthread_join(fthread,(void **)&temp);
			pthread_cond_destroy(&share->cond);
			pthread_mutex_destroy(&share->lock);
			thread_create=0;
		}		
		option = 0;
	}
	//new data saved to new_disk.img
	fp = fopen("new_disk.img","wb");
	fwrite(&part_init,sizeof(part_init),1,fp);
	fclose(fp);
	//make cache => quickly obtain files' metadata
	fp = fopen("cache.img","wb");
	fwrite(&cache,sizeof(cache),1,fp);
	fclose(fp);
	//test cache
	unsigned int found = 0x4;
	count = 0;
	while(1){
		if (cache.buf[count].cache_inode == found){
			printf("test cache \ninode : %d, size : %d\n",cache.buf[count].cache_inode,cache.buf[count].cache_size);
			break;
		}
		count++;
	}
	pthread_exit(NULL);
	exit(0);
}
