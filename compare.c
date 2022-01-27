#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

//this part is unbound synchronized queue

struct Node{
  char* name;
  struct Node* next;
};

typedef struct{
  struct Node *front, *rear;
  int count;
  int active_threads;
  
  pthread_mutex_t lock;
  pthread_cond_t read_ready;
} unboundqueue_t;

//unbound

#define QSIZE 8

typedef struct {
  char* data[QSIZE];
  unsigned count;
  unsigned head;
  pthread_mutex_t lock;
  pthread_cond_t read_ready;
  pthread_cond_t write_ready;
} boundqueue_t;



int unbound_init(unboundqueue_t *Q){
  Q->front = NULL;
  Q->rear = NULL;
  Q->count = 0;
  pthread_mutex_init(&Q->lock, NULL);
  pthread_cond_init(&Q->read_ready, NULL);
  return 0;
}


int unbound_enqueue(unboundqueue_t *Q, char* name){
  //lock
  pthread_mutex_lock(&Q->lock);

  //create new node
  struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
  newNode->name = malloc(strlen(name)+1);
  strncpy(newNode->name, name, strlen(name) + 1);
  newNode->next = NULL;

  //if queue is empty, then new node is front and rear both
  if(Q->rear == NULL){
    Q->front = Q->rear = newNode;
  }

  //Add the new node at the end of queue and change rear
  else{
    Q->rear->next = newNode;
    Q->rear = newNode;
  }

  ++Q->count;
  
  pthread_cond_signal(&Q->read_ready);
  pthread_mutex_unlock(&Q->lock);
  
  return 0;
}


char* unbound_dequeue(unboundqueue_t *Q, boundqueue_t *BQ){
  pthread_mutex_lock(&Q->lock);
  //printf("queue count: %d\n",Q->count);
  //printf("active count: %d\n", Q->active_threads);
  
  if(Q->count == 0){
    Q->active_threads--;
    //printf("threade decreased to : %d\n", Q->active_threads);
    
    if(Q->active_threads == 0){
      pthread_mutex_unlock(&Q->lock);
      pthread_cond_broadcast(&Q->read_ready);
      //pthread_cond_broadcast(&BQ->read_ready);
      //printf("HIT FINAL THREAD\n");
      return NULL;
    }

    while(Q->count == 0 && Q->active_threads > 0){
      //printf("TID: %ld WAITING HERE\n", pthread_self());
      pthread_cond_wait(&Q->read_ready, &Q->lock);
    }

    if(Q->count == 0){
      pthread_mutex_unlock(&Q->lock);
      return NULL;
    }

    Q->active_threads++;
    //printf("thread increased to: %d\n", Q->active_threads);
  }

  //temporary storage
  struct Node *temp = Q->front;
  char *name;
  name = temp->name;
  --Q->count;
  Q->front = Q->front->next;

  //if front is NULL, then change rear also as NULL
  if(Q->front == NULL){
    Q->rear = NULL;
  }

  free(temp);

  pthread_mutex_unlock(&Q->lock);
  return name;
}


int unbound_isEmpty(unboundqueue_t *Q){
  pthread_mutex_lock(&Q->lock);
  if(Q->front == NULL){
    pthread_mutex_unlock(&Q->lock);
    return 1;
  }
  pthread_mutex_unlock(&Q->lock);
  return 0;
}


int unbound_destroy(unboundqueue_t *Q)
{
  pthread_mutex_destroy(&Q->lock);
  pthread_cond_destroy(&Q->read_ready);
  
  return 0;
}

//BOUNDED QUEUE



int bound_init(boundqueue_t *Q)
{
  Q->count = 0;
  Q->head = 0;
  pthread_mutex_init(&Q->lock, NULL);
  pthread_cond_init(&Q->read_ready, NULL);
  pthread_cond_init(&Q->write_ready, NULL);
  
  return 0;
}


int bound_destroy(boundqueue_t *Q)
{
  pthread_mutex_destroy(&Q->lock);
  pthread_cond_destroy(&Q->read_ready);
  pthread_cond_destroy(&Q->write_ready);
  
  return 0;
}


// add item to end of queue
// if the queue is full, block until space becomes available
int bound_enqueue(boundqueue_t *Q, char* item)
{
  pthread_mutex_lock(&Q->lock);
  
  while (Q->count == QSIZE){
    pthread_cond_wait(&Q->write_ready, &Q->lock);
  }

  

  unsigned i = Q->head + Q->count;
  //wrap around
  if (i >= QSIZE) i -= QSIZE;
  
  char *newItem = malloc(sizeof(char)*strlen(item) + 1);
  strncpy(newItem, item, strlen(item) + 1);
  Q->data[i] = newItem;
  ++Q->count;
  
  pthread_cond_signal(&Q->read_ready);
  
  pthread_mutex_unlock(&Q->lock);
  
  return 0;
}


char* bound_dequeue(boundqueue_t *Q, int *active_count)
{
  pthread_mutex_lock(&Q->lock);
  
  while (Q->count == 0){// && Q->open) {
    if(*active_count == 0){
        pthread_mutex_unlock(&Q->lock);
        return NULL;
    }
    //printf("Active count: %d, dirQ->Count: %d\n", *active_count, Q->count);
    // printf("waititng for the active_count\n");
    pthread_cond_wait(&Q->read_ready, &Q->lock);
    //printf("done waiting\n");
  }
  if (Q->count == 0) {
    pthread_mutex_unlock(&Q->lock);
    return NULL;
  }
  
  char *item = Q->data[Q->head];

  //printf("Item: %s\n", item);
  --Q->count;
  ++Q->head;
  //wrap around
  if (Q->head == QSIZE) Q->head = 0;
  
  pthread_cond_signal(&Q->write_ready);
  pthread_mutex_unlock(&Q->lock);
  
  return item;
}

int bound_isEmpty(boundqueue_t *Q){
  if (Q->count == 0){
    return 1;
  }
  return 0;

}



/////////////////////////////////////////////////////////////////////////

//Arraylists

//////////////////////////////////////////////////////////////////////////

typedef struct{
  size_t length;
  size_t used;
  char *data;
} arraylist_t;


int al_init(arraylist_t *L, size_t length)
{
  L->data = malloc(sizeof(char) * length);
  if (!L->data) return 1;
  
  L->length = length;
  L->used   = 0;
  
  return 0;
}


void al_destroy(arraylist_t *L)
{
  free(L->data);
}


int al_append(arraylist_t *L, char item)
{
  if (L->used == L->length) {
      size_t size = L->length * 2;
      char *p = realloc(L->data, sizeof(char) * size);
      if (!p) return 1;
      
      L->data = p;
      L->length = size;
  }

  L->data[L->used] = item;
  ++L->used;
  
  return 0;
}


///////////////////////////////////////

//MISCELLANEOUS functions

///////////////////////////////////////


//error : -1, directory: 1, file: 2, anything else: 0
int isdir(const char* name){
  struct stat data;
  int err = stat(name, &data);
  if (err){
    perror(name);
    return -1;
  }

  //for dir
  if (S_ISDIR(data.st_mode)){
    return 1;
  }

  else if (S_ISREG(data.st_mode)){
    return 2;
  }
  return 0;
}


//////////////////////////////////////////////////////////////

//Thread Structs

///////////////////////////////////////////////////////////////

//for each words
struct wordNode{
  char* word;
  int count;
  double frequency;
  struct wordNode* next;
  
};

//for each file linked lists
struct fileData{
  char* filename;
  struct wordNode *file_words;
  struct fileData* next;
};

struct fileArgs{
  unboundqueue_t *dirQ;
  boundqueue_t *fileQ;
  struct fileData *headFile;
  int fileCount;
};



struct dirArgs{
  unboundqueue_t *dirQ;
  boundqueue_t *fileQ;
  char *fileSuffix;
};

struct JSD{
  struct fileData *file1;
  struct fileData *file2;
  double JSD_result;

};

struct anaArgs{
  struct JSD **arrayJSD;
  int thread_number;
  pthread_mutex_t lock;
  int anaThreads;
  int numComparisons;
};



////////////////////////////////////////////////////////////////

//Directory Functions

////////////////////////////////////////////////////////////////


void *dirThreadFunc(void *A){
  struct dirArgs *dirArgs = (struct dirArgs *) A;
  unboundqueue_t *dirQ = dirArgs->dirQ;
  boundqueue_t *fileQ = dirArgs->fileQ;
  char *fileSuffix = dirArgs->fileSuffix;
  int suffixLength = strlen(fileSuffix);



  char* name;
  struct dirent *dir;
  DIR *d; 
  while(!unbound_isEmpty(dirQ) || dirQ->active_threads > 0){
    
    name = unbound_dequeue(dirQ, fileQ);

    if(name != NULL){ 
      //printf("TID: %ld Working on directory: %s\n", pthread_self(), name);

      // SEARCH THROUGH EACH FILE IN DIRECTORY
      // ADD FILES TO FILE QUEUE (w/ suffix) ADD DIRECTORIES TO DIRECTORY QUEUE

      d = opendir(name);

      if(d == NULL){
        perror("Opendir");
        exit(EXIT_FAILURE);
      }
      else{
        while((dir = readdir(d)) != NULL){
          
          //printf("%s\n", dir->d_name);

          // ADD /"name" to end of the directory name
          // EXAMPLE: file foo in directory bar would become
          // bar/foo

          if(strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0){
            continue;
          }
          char *concatName = malloc(strlen(dir->d_name) + strlen(name) + 2); 
          strncpy(concatName, name, strlen(name)+1);
          strcat(concatName, "/");
          strcat(concatName, dir->d_name);

          //printf("Found: %s\n", concatName);

          //IF READS FILE, ADD TO FILE QUEUE
          if(isdir(concatName) == 2){
            if(dir->d_name[0] != '.'){
              char *checkSuffix = malloc(suffixLength+1);
              strncpy(checkSuffix, &dir->d_name[strlen(dir->d_name)-suffixLength], suffixLength+1);
              //printf("Filename: %s, Suffix: %s\n", dir->d_name, checkSuffix);
              if(strcmp(checkSuffix, fileSuffix) == 0){
                //printf("Suffix Matches!\n");
                bound_enqueue(fileQ, concatName);
              }
            // DO STUFF HERE 
            

            free(checkSuffix);
            }
            

          }
          //IF READS DIRECTORY, ADD TO DIRECTORY QUEUE
          else if(isdir(concatName)==1 && dir->d_name[0] != '.'){
            unbound_enqueue(dirQ, concatName);
          }
          //OTHERWISE CONTINUE and free concatName
          
          free(concatName); // Frees concat name b/c we malloc the name inside enqueue
        }
        free(name); // After going through everything in a directory, we dont need that directory name anymore?
        // we might need it later so we might have to get rid of this name
      }


      closedir(d);
    }
    else{
      //printf("Woopsies! :P\n");
      free(name);
    }
    
  }

  return NULL;
}









void *fileThreadFunc(void *A){

  //printf("File thread starts here\n");
  struct fileArgs *fileArgs = (struct fileArgs *)A;
  

  int bytes_read;
  char buf[10];
  
  arraylist_t arr;
  al_init(&arr, 10);

  
  while(bound_isEmpty(fileArgs->fileQ) != 1 || fileArgs->dirQ->active_threads > 0){

    //printf("active dirThreads %d\n", fileArgs->dirQ->active_threads);
    
    char* name = bound_dequeue(fileArgs->fileQ, &fileArgs->dirQ->active_threads);
    if(name == NULL){
      free(name);
      al_destroy(&arr);
      return NULL;
      
    }

    

    
    
    //printf("FileName: %s\n", name);

    int fd = open(name, O_RDONLY);
    if (fd == -1){
      perror("Error: ");
      free(name);
      abort();
      
    }
    fileArgs->fileCount++;
    
    struct fileData* fileData = malloc(sizeof(struct fileData));
    fileData->filename = name;
    fileData->file_words = NULL;
    fileData->next = NULL;
    

    //head of word
    struct wordNode* head = NULL;
    


    //counter for total words
    int total_words = 0;


    
    //do ww stuff here
    while((bytes_read = read(fd, &buf, 10)) > 0){
      for(int i = 0; i < sizeof(char) * bytes_read; i++){

	if(isspace(buf[i])){
	  //printf("it hit space ");


	  //to ignore any new line before any words are received
	  if(arr.used > 0){
	    char* new_word = malloc(arr.used+1);
	    //printf("arr length: %d\n", arr.length);
	    //printf("arr used: %d\n", arr.used);
	    
	    for(int i = 0; i < arr.used; i++){
	      new_word[i] =  arr.data[i];
	      //printf("added: %c\n", new_word[i]);
	      
	    }
	    new_word[arr.used] = '\0';
	    total_words++;
	    
	    
	    //printf("new word: %s\n", new_word);
	    
	    arr.used = 0;

	    struct wordNode *newNode = malloc(sizeof(struct wordNode));
	    newNode->word = new_word;
	    newNode->count = 1;
	    newNode->next = NULL;
	    

	    //first word
	    if(head == NULL){
	      newNode->next = head;
	      head = newNode;
	    }
	    
	    else{
	      //traverse through head 
	      struct wordNode* ptr = head;
	      
	      
	      //if word comes before head
	      if(strcmp(new_word, ptr->word) < 0){
		newNode->next = head;
		head = newNode;
	      }
	      
	      //word equals head
	      else if(strcmp(new_word, ptr->word) == 0){
		ptr->count++;
		free(new_word);
		free(newNode);
	      }
	      
	      //word comes after head
	      else{;
		while(ptr->next != NULL){
		  //word comes before next word
		  if(strcmp(new_word, ptr->next->word) < 0){
		    newNode->next = ptr->next;
		    ptr->next = newNode;
		    break;
		  }
		  
		  //check for same word
		  else if (strcmp(new_word, ptr->next->word) == 0){
		    ptr->next->count++;
		    //printf("already exists!  %s\n", new_word);
		    free(new_word);
		    free(newNode);
		    break;
		  }
		  
		  
		  ptr = ptr->next;
		}
		
		//word comes at the end of ptr
		if(ptr->next == NULL){
		  ptr->next = newNode;
		}
	      }
	    }	    
	    
	  }
	}

	//if not space
	else{
	  //check if char should be added, if not then ignore
	  if(isalpha(buf[i]) || isdigit(buf[i]) || buf[i] == '-'){
	    //printf("appending: %c\n", (buf[i]));
	    al_append(&arr, tolower(buf[i]));
	  }
	}
      }
    }


    
    fileData->file_words = head;

    //calculating frequency of the word
    struct wordNode *ptr = head;
    
    while(ptr != NULL){
      ptr->frequency = (double)ptr->count / total_words;
      ptr = ptr->next;
    }



    /*
    ptr = fileData->file_words;
    while(ptr != NULL){
      printf("word: %s\n", ptr->word);
      printf("count: %d\n", ptr->count);
      printf("Frequency: %lf\n\n", ptr->frequency);
      ptr = ptr->next;
      }*/


    fileData->next = fileArgs->headFile;
    fileArgs->headFile = fileData;

    /*
    struct fileData *fileptr = fileArgs->headFile;
    while(fileptr != NULL){
      printf("file %s\n", fileptr->filename);
      fileptr = fileptr->next;
      }*/
    
  }
  
  al_destroy(&arr);
  return NULL;
}

void *anaThreadFunc(void *A){

  struct anaArgs *anaArgs = (struct anaArgs *) A;
  pthread_mutex_lock(&anaArgs->lock);

  int thread_number = anaArgs->thread_number;
  anaArgs->thread_number++;

  pthread_mutex_unlock(&anaArgs->lock);   

  //anaThreads = # of analysis threads
  double KLD1 = 0;
  double KLD2 = 0;
  double JSD = 0;

  //printf("NumComparisons: %ld\n", ));

  double mean_frequency;
  for(int N = thread_number; N < anaArgs->numComparisons; N += anaArgs->anaThreads){

    struct wordNode *file1ptr = anaArgs->arrayJSD[N]->file1->file_words;
    struct wordNode *file2ptr = anaArgs->arrayJSD[N]->file2->file_words;


    //printf("\nfile1: %s    file2: %s\n", anaArgs->arrayJSD[N]->file1->filename, anaArgs->arrayJSD[N]->file2->filename);

    
    
    while(file1ptr != NULL || file2ptr != NULL){
      mean_frequency = 0;
      

      //words are equal
      if(file1ptr != NULL && file2ptr != NULL && strcmp(file1ptr->word, file2ptr->word) == 0){
        //printf("\nComparing 1: %s    2: %s\n", file1ptr->word,file2ptr->word);
        //printf("File 1 Frequency: %f\n", file1ptr->frequency); 
        //printf("File 2 Frequency: %f\n", file2ptr->frequency); 

        mean_frequency = (file1ptr->frequency + file2ptr->frequency)/2;
        KLD1 += file1ptr->frequency * log(file1ptr->frequency/mean_frequency)/log(2); 
        KLD2 += file2ptr->frequency * log(file2ptr->frequency/mean_frequency)/log(2); 

        file1ptr = file1ptr->next;
        file2ptr = file2ptr->next;


      }
      

      //word 1 is "less" than word 2
      //"less" meaning it comes first lexicographically
      else if(file2ptr == NULL || (file2ptr != NULL  && file1ptr != NULL && strcmp(file1ptr->word, file2ptr->word) < 0)){

        //printf("\nComparing 1: %s    2:DNE\n", file1ptr->word);
        //printf("File 1 Frequency: %f\n", file1ptr->frequency); 

        mean_frequency = file1ptr->frequency / 2;
        KLD1 += file1ptr->frequency * log(file1ptr->frequency/mean_frequency)/log(2);
        file1ptr = file1ptr->next;
      }
      
      //word 2 is "less" than word 1
      else if(file1ptr == NULL || (file2ptr != NULL && file1ptr != NULL && strcmp(file1ptr->word, file2ptr->word) > 0) ){

        //printf("\nComparing 1: DNE    2:%s\n", file2ptr->word);
        //printf("File 2 Frequency: %f\n", file2ptr->frequency); 
        mean_frequency = file2ptr->frequency / 2;
        KLD2 += file2ptr->frequency * log(file2ptr->frequency/mean_frequency)/log(2);
        file2ptr = file2ptr->next;
      }
      
      //printf("mean_frequency: %lf\n", mean_frequency);
      //printf("KLD1: %lf\n", KLD1);
      //printf("KLD2: %lf\n", KLD2);
    }
    
    
    JSD = sqrt((KLD1 + KLD2)/2);
    // printf("JSD: %lf\n", JSD);
    anaArgs->arrayJSD[N]->JSD_result = JSD;
    KLD1 = 0;
    KLD2 = 0;

  }

  //printf("thread_number: %d\n", thread_number);
  


  return NULL;
}

int compareJSD(const void *A, const void *B){
  
  struct JSD *f1 = *(struct JSD **) A;
  struct JSD *f2 = *(struct JSD **) B;
  

  if(f1->JSD_result < f2->JSD_result){
    return -1;
  }

  else if(f1->JSD_result > f2->JSD_result){
    return 1;
  }
  else{
    return 0;
  }

}



/////////////////////////////////////////////////////////////

//MAIN FUNCTION

/////////////////////////////////////////////////////////////

int main(int argc, char* argv[]){

  assert(!(argc==1));

  //directory queue
  unboundqueue_t *dirQ = malloc(sizeof(unboundqueue_t));
  unbound_init(dirQ);

  //regular file queue
  boundqueue_t *fileQ = malloc(sizeof(boundqueue_t));
  bound_init(fileQ);
  

  int success = 1;

  //argument variables
  int dirThreads = 1;
  int fileThreads = 1;
  int anaThreads = 1;
  char *fileSuffix = malloc(5);
  strcpy(fileSuffix,".txt");




  //looks through the argv
  for(int i = 1; i < argc; i++){
    if(argv[i][0] == '-' ){ // Checks for flags
      int argLen = strlen(argv[i]);
      if(argLen < 2 || (argLen < 3 && argv[i][1] != 's')){
        fprintf(stderr, "Missing Arguments in Optional Argument\n");
        exit(1);
      }
      char *flag = malloc(strlen(argv[i])-1);
      if(argv[i][1] == 'd'){ // dirThreads change
        strncpy(flag, &argv[i][2], strlen(argv[i])-1);
        dirThreads = atoi(flag);
        if(dirThreads == 0){
          fprintf(stderr, "Input Error: Directory Thread count was given: %d\n", dirThreads);
          exit(1);
        }
      }
      else if(argv[i][1] == 'f'){ // fileThreads change
        strncpy(flag, &argv[i][2], strlen(argv[i])-1);
        fileThreads = atoi(flag);
        if(fileThreads == 0){
          fprintf(stderr, "Input Error: File Thread count was given: %d\n", fileThreads);
          exit(1);
        }
      }
      else if(argv[i][1] == 'a'){ // analThreads change
        strncpy(flag, &argv[i][2], strlen(argv[i])-1);
        anaThreads = atoi(flag);
        if(anaThreads == 0){
          fprintf(stderr, "Input Error: Analysis Thread count was given: %d\n", anaThreads);
          exit(1);
        }
      }
      else if(argv[i][1] == 's'){ // fileSuffix change
        if(strlen(argv[i]) == 2){
          
          fileSuffix[0] = '\0';
        }
        else{
          free(fileSuffix);
          fileSuffix = malloc(strlen(argv[i]));
          strncpy(fileSuffix, &argv[i][2], strlen(argv[i])-1);
        } 
      }
      else{
        fprintf(stderr, "Invalid optional argument\n");
        exit(1);
      }
      free(flag);
    }
    //dir, file, errors
    else{ 
      int dirReg= isdir(argv[i]);
      if(dirReg == 1){
        unbound_enqueue(dirQ, argv[i]);
      }
      else if(dirReg == 2){
        bound_enqueue(fileQ, argv[i]);
      }
      else{
        success = -1;
      }
    }
  }

  dirQ->active_threads = dirThreads;

  //dirthread tid holder
  pthread_t *dirTIDs = malloc(dirThreads * sizeof(pthread_t));
  
  
  struct dirArgs *dirArgs = malloc(sizeof(struct dirArgs));
  dirArgs->dirQ = dirQ;
  dirArgs->fileQ = fileQ;
  dirArgs->fileSuffix = fileSuffix;
  

  
  int err;  
  for(int i = 0; i < dirThreads; i++){
    err = pthread_create(&dirTIDs[i], NULL, dirThreadFunc, dirArgs);
    if(err){
      perror("Dir Pthread Create");
      return -1;
    }
  }


  
  //filethread TID holder & structs
  pthread_t *fileTIDs = malloc(fileThreads * sizeof(pthread_t));
  struct fileArgs *fileArgs = malloc(sizeof(struct fileArgs));
  fileArgs->fileQ = fileQ;
  fileArgs->dirQ = dirQ;
  fileArgs->headFile = NULL;
  fileArgs->fileCount = 0;

  
  for(int i = 0; i < fileThreads; i++){
    err = pthread_create(&fileTIDs[i], NULL, fileThreadFunc, fileArgs);
    if(err){
      perror("File Pthread Create");
      return -1;
    }
  }



  // JOINING THREADS
  for(int i = 0; i < dirThreads; i++){
    err = pthread_join(dirTIDs[i], NULL);
    if(err){
      perror("Dir Pthread Join");
      return -1;
    }
  }
  free(dirTIDs);

  
  pthread_cond_broadcast(&fileQ->read_ready);

  
  for(int i = 0; i < fileThreads; i++){
    err = pthread_join(fileTIDs[i], NULL);
    if(err){
      perror("File Pthread Join");
      return -1;
    }
  }
  free(fileTIDs);


  //checking if file count was < 2
  if(fileArgs->fileCount < 2){
    fprintf(stderr, "less than two files, file Count: %d\n", fileArgs->fileCount);
    exit(EXIT_FAILURE);
  }
    


  

  //CREATE ANALYSIS THREADS

  int numComparisons = fileArgs->fileCount * ((fileArgs->fileCount)-1) / 2;
  // printf("File Count: %d, numComparisons: %d\n", fileArgs->fileCount, numComparisons);
  struct JSD **arrayJSD = malloc(numComparisons * sizeof(struct JSD*));  

  int i = 0;

  struct fileData *f1ptr = fileArgs->headFile;
  struct fileData *f2ptr;
  //printf("we have made fileptrs\n");
  while(f1ptr != NULL){
    
    f2ptr = f1ptr->next;
    

    while(f2ptr != NULL){
      //struct JSD *xd = (struct JSD *)malloc(sizeof(struct JSD));
      //arrayJSD[i] = *xd;
      
      struct JSD *newJSD = malloc(sizeof(struct JSD));
      arrayJSD[i] = newJSD;
      


      
      //printf("i: %d\n", i);
      arrayJSD[i]->file1 = f1ptr;
      //printf("Filename1: %s\n", arrayJSD[i]->file1->filename);
      //printf("Filename2: %s\n", f2ptr->filename);
      //printf("i: %d\n", i);
      arrayJSD[i]->file2 = f2ptr;
      
      arrayJSD[i]->JSD_result = 0;
      f2ptr = f2ptr->next;
      i++;
    }

    f1ptr = f1ptr->next;
  }


  

  pthread_t *anaTIDs = malloc(anaThreads * sizeof(pthread_t));
  struct anaArgs *anaArgs = malloc(sizeof(struct anaArgs));
  anaArgs->arrayJSD = arrayJSD; 
  anaArgs->thread_number = 0;
  anaArgs->numComparisons = numComparisons;
  anaArgs->anaThreads = anaThreads;



  for(int i = 0; i < anaThreads; i++){
    err = pthread_create(&anaTIDs[i], NULL, anaThreadFunc, anaArgs);
  
    if(err){
      perror("Ana Pthread Create");
      return -1;
    }
  }
  



  //JOIN ANALYSIS THREADS

  for(int i = 0; i < anaThreads; i++){
    err = pthread_join(anaTIDs[i], NULL);
    //printf("dirthread %d joined\n" ,i);
    if(err){
      perror("Ana Pthread Join");
      return -1;
    }
  }



  /*
  printf("Before Qsort-----------------------\n\n");
  for(int i = 0; i < numComparisons; i++){
   
    printf("%lf %s %s\n",arrayJSD[i]->JSD_result, arrayJSD[i]->file1->filename, arrayJSD[i]->file2->filename);
    }*/

  //QSORT arrayJSD
  qsort(arrayJSD, numComparisons, sizeof(struct JSD *), compareJSD);
  
  //printf("After Qsort-----------------------\n\n");
  for(int i = 0; i < numComparisons; i++){
    
    printf("%lf %s %s\n",arrayJSD[i]->JSD_result, arrayJSD[i]->file1->filename, arrayJSD[i]->file2->filename);
  }
  



  
  
  


  //freeing everything in fileArgs
  while(fileArgs->headFile != NULL){

    //printf("\nfilename %s\n", fileArgs->headFile->filename);
    
    while(fileArgs->headFile->file_words != NULL){

      //printf("\nword: %s\n", fileArgs->headFile->file_words->word);

      //freeing the word content of wordNode
      free(fileArgs->headFile->file_words->word);

      
      //printf("count: %d\n", fileArgs->headFile->file_words->count);
      //printf("frequency: %f\n", fileArgs->headFile->file_words->frequency);
      
      struct wordNode *temp_wordNode = fileArgs->headFile->file_words;
      fileArgs->headFile->file_words = fileArgs->headFile->file_words->next;

      //freeing wordNode
      free(temp_wordNode);
    }

    struct fileData *temp_fileData = fileArgs->headFile;
    fileArgs->headFile = fileArgs->headFile->next;

    //freeing fileName);
    free(temp_fileData->filename);

    //freeing fileData
    free(temp_fileData);
  }
  //FREE JSD ARRAY
  for(int i = 0; i < numComparisons; i++){
    free(arrayJSD[i]);
  }



  //freeing fileArg
  free(fileArgs);
  
  free(anaTIDs);

  free(arrayJSD);
  free(anaArgs);
  free(dirArgs);
  free(fileSuffix);
  free(fileQ);
  free(dirQ);
 
  if(success == -1){
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
