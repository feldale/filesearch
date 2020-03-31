

#include <dirent.h>
#include <stdio.h>
#include <unistd.h> // chdir and getlogin_r
#include <pthread.h>
#include <stdlib.h> // malloc and free


struct threadstate {
  int matched : 1;
  int hold : 1;
  int done : 1;
  int spawned : 1;
  unsigned int coolnum : 4;
  void* random_ptr;
};

struct list_node {
  struct list_node* prev;
  struct list_node* next;
  struct threadstate* obj;
  pthread_t me;
};

unsigned int threadcount;
unsigned int max_depth;

char filename[256];

void pathconcat(char* full, char* begin, char* end){
  unsigned int ctr1, ctr2 = 0;
  ctr1 = 0;
  while((begin[ctr1] != NULL) && (ctr1 < 0xff)) {
    full[ctr1] = begin[ctr1];
    ctr1++;
  }
  full[ctr1] = '/';
  ctr1++;
  while((end[ctr2] != NULL) && ((ctr1+ctr2) < 0xff)) {
    full[ctr1 + ctr2] = end[ctr2];
    ctr2++;
  }
  //return ((ctr1 + ctr2) < 0xff) ? 0 : 1;
}

int compare(char* a, char* b){
  unsigned int len = 0;
  while ((a[len] != NULL) && (len < 0xff)) {
    if (b[len] == NULL) {
      return 0;
    } else {
      if (a[len] != (b[len] | 0x20)) { // makes lowercase
        return 0;
      }
      len++;  // b shorter than a? 
    } 
  }
  //printf("%s was a match \n", b);
  return 1;
}

int looknfind(struct threadstate* state) {

	DIR *d;
  char* path;
	struct dirent *dentry;
  char fullpath[256];
  pthread_t deeper = 0;
  struct threadstate child_state;
  unsigned int attempts;
  path = (char*)state->random_ptr;
  if (state->coolnum > max_depth) {
    //puts("TOO DEEP");
    state->done = 1;
    pthread_exit(NULL);
  }
  while (state->hold) {
    //puts("Yielding");
    sched_yield();

  }
  
  struct list_node fam; //head
  struct list_node* index = &fam;
  attempts = 0;
  //d = opendir(path);
  for (attempts = 0; attempts < 3; ++attempts){
    d = opendir(path);
    if (d == NULL) {
      //printf("Out of files for : %s \n" , path);
      sched_yield();
    } else {
      break;
    }
  }

  //printf("Opened: %s \n", path);
  if (d) {
    while ((dentry = readdir(d)) != NULL) {
    	if ((dentry->d_type != DT_UNKNOWN) && (dentry->d_name[0] != '.')) {  // good file

    		if (dentry->d_type == DT_DIR) {
          if (compare(filename, dentry->d_name)) {
    		    printf("MATCH: %s/%s (Directory)\n", path, dentry->d_name);
            state->matched = 1;
    		  }
          //pathconcat(fullpath, path, dentry->d_name);
          //printf("%s \n", fullpath);
          
          index->next = (struct list_node*)malloc(sizeof(fam));  // new node
          index->next->prev = index;
          index = index->next;
          
          
          index->obj = (struct threadstate*)malloc(sizeof(child_state));  // new child
          index->obj->hold = 1;
          index->obj->coolnum = state->coolnum + 1;
          //printf("New thread at depth: %d \n", index->obj->coolnum);
          index->obj->random_ptr = (char*)malloc(256);
          pathconcat(index->obj->random_ptr, path, dentry->d_name);
          //printf("Queued: %s \n", index->obj->random_ptr);
          state->spawned = 1;
          //state->spawned = (pthread_create(&deeper, NULL, &looknfind, &child_state)) ? 0 : 1;
          //printf("%d \n", deeper);
          // spawn child threads at the end after accumulating folder names
          // use a linked list
        }

  			if ((dentry->d_type == DT_REG) && (compare(filename, dentry->d_name))) {
  				printf("MATCH: %s/%s \n", path, dentry->d_name);
          state->matched = 1;	
        }
      }
    }
    closedir(d);
  } else {
    printf("Could not open %s \n", path);
    state->done = 1;
    pthread_exit(NULL);
  }

  if (state->spawned) {
    while(index->prev != NULL) {

      index->obj->hold = 0;
      if (!(pthread_create(&(index->me), NULL, &looknfind, (index->obj)))) {
      //made thread
        threadcount++;
        //printf("Threads %d \n", threadcount);
        while(pthread_join(index->me, NULL)) {
          sched_yield();
        }
        //puts("Joined");
      } else {
        puts("Could not create thread");
      }
      if (index->obj->done) {
        free(index->obj);
        index = index->prev;
        free(index->next);
        threadcount--;
        //printf("Freed. %d remain \n", threadcount);
      }
    }
    //while(pthread_join(deeper, &child_state)){
    
  }
  //puts("Thread exited");
  state->done = 1;
  pthread_exit(NULL);
  
  return 0;
}

int main(int argc, char** argv){
  pthread_t worker;  // worker thread
  struct threadstate state;
  int filename_len = 0;
  char* username;
	switch (argc){
    case 1:
      puts("Please specify a file");
      return 0;
      break;
    case 2:
      puts("Default max depth of 2 will be used");
      max_depth = 2;
      break;
    case 3: 
      max_depth = (unsigned int)argv[2][0] - 48;
      printf("Depth of %u will be used\n", max_depth);
      break;
  }
  if (argc < 2){
		return 0;
	}
  username = getlogin();


  char paf[64];
  pathconcat(paf, "/mnt/c/Users", username);
  if (chdir(paf)) {
    perror("Could not change directory");
    return 0;
  }
  while (argv[1][filename_len] != NULL){
    filename[filename_len] = argv[1][filename_len];
    filename_len++;
  }
  state.random_ptr = paf;
  state.coolnum = 0;
  threadcount++;
  pthread_create(&worker, NULL, &looknfind, &state);
  while (!state.done) {
    sched_yield();
  }
  if (pthread_join(worker, NULL)){
    perror("thread error");
  }
	//looknfind(paf, argv[1]);
  puts("Done");
	return 0;
}
