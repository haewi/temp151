#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/file.h>
#include <dirent.h>

// --------------------------- Functions ----------------------------
void *workerFunc(void *ar);
void managerFunc();

// --------------------------- Queue ----------------------------

// node
typedef struct __node_t {
	char* filepath;
	struct __node_t *next;
} node_t;

// list structure
typedef struct __list_t {
	node_t *head, *tail;
	pthread_mutex_t head_l, tail_l;
} list_t;

	// ----------------- Global Variables ------------------

char ** keywords = 0x0;
list_t queue;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_cond_t done = PTHREAD_COND_INITIALIZER;
int idle=0, workerNum=2;

void queue_init(list_t *L){
	node_t *tmp = (node_t*) malloc(sizeof(node_t));
	tmp->next = 0x0;
	L->head = tmp;
	L->tail = tmp;
	pthread_mutex_init(&L->head_l, NULL);
	pthread_mutex_init(&L->tail_l, NULL);
}

int enqueue(list_t *L, char* key){ // success: 1, fail: -1
//	printf("enqueue start\n");
	node_t *new = (node_t*) malloc(sizeof(node_t));
	if(new == NULL){
		perror("malloc - new node");
		return -1;
	}
//	printf("new node malloc done\n");

	new->filepath = (char*) malloc(sizeof(char)*strlen(key));
	if(new->filepath == NULL){
		perror("malloc - filepath");
		return -1;
	}
//	printf("new->filepath malloc done\n");

	strcpy(new->filepath, key);
	new->next = 0x0;
//	printf("filepath: %s\n", new->filepath);

	pthread_mutex_lock(&L->tail_l);
	L->tail->next = new;
	L->tail = new;
	pthread_mutex_unlock(&L->tail_l);

	return 1;
}

node_t* dequeue(list_t *L){
	// check if empty
	pthread_mutex_lock(&L->head_l);
	node_t *tmp = L->head;
	node_t *new_head = tmp->next;
	idle++;
	while(new_head == NULL){ // sleep until it queue contains something
		pthread_cond_wait(&fill, &L->head_l);
	}
	idle--;
	node_t *temp = new_head;
	L->head = new_head;
	pthread_mutex_unlock(&L->head_l);
	free(tmp);
	return temp;
}

int isEmpty(list_t *L){ //1: empty, 0: not empty
	pthread_mutex_lock(&L->head_l);
	if(L->head->next == NULL){
		return 1; // it is empty
	}
	return 0;
}

void printqueue(list_t * L){
	pthread_mutex_lock(&L->head_l);
	node_t *tmp = L->head->next;
	printf("----------------All files in queue\n");
	while(tmp != NULL){
		printf("%s\n", tmp->filepath);
		tmp = tmp->next;
	}
	pthread_mutex_unlock(&L->head_l);
}

// --------------------------- Main ----------------------------

int main(int argc, char * argv[]){
	int c;

	// Parse Argument
	while((c = getopt(argc, argv, "t:")) != -1) {
		switch(c) {
			case 't':
				workerNum = atoi(optarg);
				break;
			case '?':
				if(optopt == 't'){
					fprintf(stderr, "[ERROR] Option -%c requires an integer argument.\n", optopt);
				}
				else {
					fprintf(stderr, "[ERROR] Unknown option character '\\x%x'.\n", optopt);
				}
				return 1;
			default:
				abort();
		}
	}
	if(workerNum <= 0){
		printf("[ERROR] No worker number\n");
		abort();
	}
	if(workerNum > 16){
		printf("[ERROR] worker more than 16\n");
		abort();
	}
	printf("worker number: %d\n", workerNum);
	idle = workerNum;

	if(argc <= optind){
		printf("[ERROR] No directory Name\n");
		abort();
	}
	char* directory = argv[optind];
	printf("directory: %s\n", directory);

	int keyNum = argc - optind - 1;
	if(keyNum <= 0){
		printf("[ERROR] No keywords\n");
		abort();
	}
	if(keyNum > 8){
		printf("[ERROR] keyword more than 8\n");
		abort();
	}
	keywords = (char**) malloc(keyNum * sizeof(char*));
	for(int index = optind+1; index < argc; index++){
		keywords[index-optind-1] = (char*) malloc(sizeof(char)*strlen(argv[index]));
		keywords[index-optind-1] = argv[index];
	}

	printf("Keywords\n");
	for(int i=0; i<keyNum; i++){
		printf("%s\n", keywords[i]);
	}

	// initialize queue
	queue_init(&queue);

	// add first search directory to queue
	if(enqueue(&queue, directory) == -1){
		perror("enqueue error");
		return -1;
	}

//	printqueue(&queue);

//	node_t* tmp = dequeue(&queue);
//	printf("tmp: %s\n", tmp->filepath);
//	printqueue(&queue);

	pthread_t * workers = (pthread_t*) malloc(sizeof(pthread_t)*workerNum);
	for(int i=0; i<workerNum; i++){
		int rc = pthread_create(&workers[i], NULL, workerFunc, (void *) 100);
		if(rc !=0){
			printf("[ERROR] pthread_create error\n");
			abort();
		}
	}

	managerFunc();




	// free allocated memory
//	for(int i=0; i<keyNum; i++){
//		printf("free\n");
//		free(keywords[i]);
//	}
//	free(keywords);
}



void *workerFunc(void *ar){
	printf("I am worker\n");
	do{
		printf("I am working\n");
		node_t* file = dequeue(&queue);
		printf("idle: %d\n", idle);
		struct dirent *pDirent;
		DIR * pDir;

		if((pDir = opendir(file->filepath)) == 0x0) {
			printf("[ERROR] opening directory error\n");
			return 0x0;
		}

		char* retDir = (char*) malloc(sizeof(int));
		char test_dir[100];
	
		while((pDirent = readdir(pDir)) != NULL){
			if(strcmp(".", pDirent->d_name) == 0 || strcmp("..", pDirent->d_name) == 0){
				continue;
			}

			if(pDirent->d_type == DT_DIR){
				char* path = (char*) malloc(sizeof(file->filepath) + sizeof(pDirent->d_name) + 1);
				strcpy(path, file->filepath);
				strcat(path, "/");
				strcat(path, pDirent->d_name);
//				printf("found directory: %s\n", path);
				enqueue(&queue, path);
				printqueue(&queue);
				pthread_cond_signal(&fill);
			}
			if(pDirent->d_type == DT_REG){
				printf("file? %s\n", pDirent->d_name);
			}
		}
	} while(idle < workerNum);

	printf("worker: I am done\n");
}


void managerFunc(){
//	pthread_mutex_lock(&queue.head_l);
//	while(idle < workerNum && isEmpty(&queue)){
//		pthread_cond_wait(&done, &queue.head_l);
//	}
	printf("manager done\n");
//	pthread_mutex_unlock(&queue.head_l);
}

















































