#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include "list.h"
#include "rbtree.h"
#include "libhashfile.h"
/************************ RED-BLACK TREE INDEXING *****************************/

#define RBTREE_INDEXING_BITS		8
#define RBTREE_INDEXING_TABLE_SIZE	1 << RBTREE_INDEXING_BITS
#define USERS_NUM 100


//int dedup_table[USERS_NUM][USERS_NUM];
//int total_dedup[USERS_NUM];
unsigned char finger_buffer[100][6];
int buffer_count=0;
FILE* file_out[USERS_NUM];



struct rbtree_hash_element {
	unsigned char *key;
	int keysz; /* in bytes */
	char *value;
	struct rb_node node;
	int duplicate_count;
	int distribute[USERS_NUM];
};

struct rb_root *rbtree_hash_table;

static int rbtree_init_key_value_store()
{
	int i;

	rbtree_hash_table = malloc(sizeof(struct rb_root)
					 * RBTREE_INDEXING_TABLE_SIZE);
	if (!rbtree_hash_table)
		return -1;

	for (i = 0; i < RBTREE_INDEXING_TABLE_SIZE; i++)
		rbtree_hash_table[i] = RB_ROOT;


	return 0;
}


static struct  rbtree_hash_element* search_rbtree(struct rb_root *root, unsigned char *key,int debug)
{
	int result;
	struct rb_node *node = root->rb_node;

	while (node) {
		struct rbtree_hash_element *ele =
			 container_of(node, struct rbtree_hash_element, node);
		
		result = memcmp((char *)key, (char *)ele->key, ele->keysz);
		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else {
			ele->duplicate_count++;
			return ele;
		}
	}
	return NULL;
}

static int rbtree_insert(struct rb_root *root, struct rbtree_hash_element *ele)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	while (*new) {
		struct rbtree_hash_element *this =
			 container_of(*new, struct rbtree_hash_element, node);
		int result = memcmp((char *)ele->key, (char *)this->key,
								 this->keysz);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return 1;
	}

	rb_link_node(&ele->node, parent, new);
	rb_insert_color(&ele->node, root);

	return 0;
}

static int rbtree_store_key_value(unsigned char *key, int keysz, char *value, int id)
{
	int  hash;
	struct rbtree_hash_element *newel;

	hash = (unsigned int)key[0];

	newel = search_rbtree(&rbtree_hash_table[hash], key,0);
	if (newel){
		//if(newel->distribute[id]==0)
		//	newel->duplicate_count++;
		//newel->distribute[id]++;
		return 1;
	}
	/* No duplicates found => allocate entry */
	newel = malloc(sizeof(struct rbtree_hash_element));
	if (!newel)
		return -1;

	newel->key = malloc(keysz);
	if (!newel->key) {
		free(newel);
		return -1;
	}

	memcpy((char *)newel->key, (char *)key, keysz);
	newel->keysz = keysz;
	newel->value = value;
	newel->duplicate_count = 0;
	//memset(newel->distribute,0,sizeof(newel->distribute));
	//newel->distribute[id]++;
	memcpy(finger_buffer[buffer_count],(char*)key,keysz);
	buffer_count++;
	rbtree_insert(&rbtree_hash_table[hash], newel);

	return 0;
}


int Process_SourceFile(char* fileName,int currentID){
	int ret;
	struct hashfile_handle *handle;
	const struct chunk_info* chunk;
	//int hashfile_chunks,hashfile_chunk_count;

	printf("Processing %s ...\n",fileName);


	handle = hashfile_open(fileName);
	if (!handle) {
		printf("Failed to open hash file\n");
		return -1;
	}
	
	//hashfile_chunks = hashfile_numchunks(handle);
	//hashfile_chunk_count = 0;

	while(1){
		ret=hashfile_next_file(handle);
		if(ret==0){
		  break;
		}
		if(ret<0){
			printf("Error processing hash file\n");
			goto out;
		}
		assert(buffer_count==0);
	
	
		while(1){
			chunk=hashfile_next_chunk(handle);
			if(!chunk){
				break;
			}

			ret=rbtree_store_key_value(chunk->hash,handle->header.hash_size/8,NULL,currentID);
			if(ret<0){
				printf("Fail to insert key into index!\n");
				goto out;
			}
			if(buffer_count==100){
				int i,j;
				for(i=0;i<100;i++){
					for(j=0;j<6;j++)
						fprintf(file_out[currentID],"%02x",finger_buffer[i][j]);
					fprintf(file_out[currentID],"\n");
				}
				buffer_count=0;
			}
		}
		if(buffer_count>0){
			int i,j;
			for(i=0;i<buffer_count;i++){
			  for(j=0;j<6;j++)
				fprintf(file_out[currentID],"%02x",finger_buffer[i][j]);
			  fprintf(file_out[currentID],"\n");
			}
			buffer_count=0;
		}
	}
	ret=0;

out:
	hashfile_close(handle);
	return ret;
}

/*void Process_TargetFile(char* fileName){
	//int ret;
	struct hashfile_handle *handle;
	const struct chunk_info* chunk;
	int hashfile_chunks,hashfile_chunk_count;

	printf("Processing %s ...\n",fileName);


	handle = hashfile_open(fileName);
	if (!handle) {
		perror("Failed to open hash file");
		
		return ;
	}
	
	hashfile_chunks = hashfile_numchunks(handle);
	hashfile_chunk_count = 0;
	while(1){
		int ret=hashfile_next_file(handle);
		if(ret==0)
		  break;
		if(ret<0){
			fprintf(stderr,"Error processing hash file");
			goto out;
		}
	
	
		while(1){
			chunk=hashfile_next_chunk(handle);
			if(!chunk)
			 break;
			
			unsigned char* key=(unsigned char*)(chunk->hash);
			int hash = ( int)key[0];

			struct rbtree_hash_element* ele;
			ele = search_rbtree(&rbtree_hash_table[hash], chunk->hash,1);
			if(ele){
				int j=0,count=0,pos=-1;
				for(j=0;j<100;j++){
					if(ele->distribute[j]>0){
						count++;
						pos=j;
					}
				}
				if(count==1)
				  answer[pos]+=1;
			}
		}
	}

out:
	hashfile_close(handle);
	return ;
}*/


/*void fill_table(int* dis,int table[][USERS_NUM])
{
	int i;
	for(i=0;i<USERS_NUM;i++){
		if(dis[i]>0){
			int j;
			for(j=i+1;j<USERS_NUM;j++){
				if(dis[j]>0){ 
					table[i][j]++;
				}
			}
		}
	}
	return ;
}

int statistics_dedupe(struct rb_node *root, int table[][USERS_NUM])
{
	if(root==NULL) return 0;

	//printf("%x \n",root);

	struct rbtree_hash_element *ele = container_of(root, struct rbtree_hash_element, node);
		
	//printf("here!\n");
	int i,count=0;;
	for(i=0;i<USERS_NUM;i++)
	  if(ele->distribute[i]>0) count++;
	if(count>1){
		int k;
		for(k=0;k<USERS_NUM;k++)
		  if(ele->distribute[k]>0)
			total_dedup[k]++;
	}
	fill_table(ele->distribute,table);

	//printf("here2!\n");
	statistics_dedupe(root->rb_left,table);
	statistics_dedupe(root->rb_right,table);
	return 0;
}*/

void Sort_File(struct dirent* list[],int size)
{
	int i,j;
	for(i=0;i<size;i++){
		struct dirent* min=list[i];
		int pos=i;
		for(j=i+1;j<size;j++)
		  if(strcmp(min->d_name,list[j]->d_name)>0){
			pos=j;
			min=list[j];
		  }
		list[pos]=list[i];
		list[i]=min;
	}
	return ;

}

void Output_Init(struct dirent* list[],int num,FILE* out[],char* output_dir)
{
	int i;
	char filename[100];
	for(i=0;i<num;i++){
		strcpy(filename,output_dir);
		strcat(filename,list[i]->d_name);
		out[i]=fopen(filename,"w");
		if(out[i]==NULL){
			printf("faile to create file:%s\n",filename);
			return ;
		}
	}

}


int main(int argc, char* argv[])
{
	int i;

	if(argc<2){
		printf("opt is not full!\n");
		return -1;
	}

	char* sourcePath=argv[1];
	//char* targetPath=argv[2];

	DIR* sourceDir;
	struct dirent* sourceDirNamelist[100];
	//struct dirent **targetDirNamelist;

	if((sourceDir=opendir(sourcePath))==NULL){
		perror("open dir error!\n");
		return -1;
	}

	for(i=0;(sourceDirNamelist[i]=readdir(sourceDir))!=NULL;i++){
		if(!strcmp(sourceDirNamelist[i]->d_name,".")||
					!strcmp(sourceDirNamelist[i]->d_name,".."))
		  i--;
	}
	int sourceFileNum=i;

	if(sourceFileNum==0){
		printf("no source file!\n");
		return -1;
	}

	char* dir="./output/";

	Sort_File(sourceDirNamelist,sourceFileNum);

	Output_Init(sourceDirNamelist,sourceFileNum,file_out,dir);

	rbtree_init_key_value_store();

	for(i=0;i<sourceFileNum;i++){
		char filepath[100];
		strcpy(filepath,sourcePath);
		strcat(filepath,sourceDirNamelist[i]->d_name);
		Process_SourceFile(filepath,i);
		fclose(file_out[i]);
		printf("%s finished\n",filepath);
	}

	//Process_TargetFile(targetPath);
	
	printf("processing down!\n");


	
	/*unsigned int ui;
	for(ui=0;ui<RBTREE_INDEXING_TABLE_SIZE;ui++)
		if(rbtree_hash_table[ui].rb_node!=NULL){
			statistics_dedupe(rbtree_hash_table[ui].rb_node,dedup_table);
		}
	int j;
	for(i=0;i<sourceFileNum;i++){
	  for(j=0;j<sourceFileNum;j++){
		  if(i>j) printf("%d\t",dedup_table[j][i]);
			else printf("%d\t",dedup_table[i][j]);
	  }
	 printf("total: %d\n",total_dedup[i]);
	}*/


	/*while((opt=getopt(argc,argv,"s:t:"))!=-1){
		switch(opt){
			case 's':

			break;

			case 't':

			break;
		
		}
	}*/
	return 0;
}
