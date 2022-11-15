/* l1cache@bu.edu*/

#include <dirent.h>
#include <stdio.h>
#include <unistd.h> // chdir and getlogin_r
#include <pthread.h>
#include <stdlib.h> // malloc and free


struct threadstate {
  int done : 1;
  int spawned : 1;
  unsigned int depth : 6;  // want it byte aligned, folder depth can be up to 64
  void* random_ptr;
};

struct list_node {
  struct list_node* prev;
  struct list_node* next;
  struct threadstate* obj;
  pthread_t me;
};

unsigned int threadcount; //for debug/curiosity
unsigned int max_depth; // max depth of folders to go into

char filename[256];

unsigned int len(char* string){
  if (string == 0x0) {
    return 0;
  }
  unsigned int i = 0;
  while (string[++i]);
  return i;

}

void pathconcat(char* full, char* begin, char* end){  // forgot string.h existed
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
}

int compare(char* a, char*b){
  unsigned int i = 0;
  while ((a[i] != NULL) && (i < 0xff)) {
    if (b[i] == NULL) {
      return 0;
    } else {
      if (a[i] != (b[i] | 0x20)) { // makes lowercase
        return 0;
      }
      i++;  // b shorter than a? 
    } 
  }
  //printf("%s was a match \n", b);
  return 1;
}

int triecompare(char* a, char* b){
  unsigned int length, max, matchlen;
  length = len(a);
  max = len(b) - length;
  if (max > 0xff){ // a longer than b
    return 0;
  }
  matchlen = 0;
  for (unsigned int i = 0; i < max; i++) {
    matchlen = ((a[matchlen] == (b[i] | 0x20))) ? ++matchlen : 0;
    if (matchlen == len(a)){
      return 1;
    }
  }
  return (matchlen == length) ? 1 : 0;
}

int looknfind(struct threadstate* state) {

  DIR *d;
  char* path;  // useful ptr instead of index->obj->random_ptr
  struct dirent *dentry;  // directory entry
  char fullpath[256];  // as long as natively supported

  struct threadstate child_state;
  unsigned int attempts;  // opendir may fail from too many fids or lack of privileges so we wait to try and open if it fails
  struct list_node fam; //head
  struct list_node* index;

  path = (char*)state->random_ptr;
  if (state->depth > max_depth) {
    //puts("Depth exceeded");
    state->done = 1;
    pthread_exit(NULL);
  }
  

  index = &fam;
  attempts = 0;
  //d = opendir(path);
  for (attempts = 0; attempts < 3; ++attempts){
    d = opendir(path);
    if (d == NULL) {  // error opening, can be run out of file descriptors
      //printf("Out of files for : %s \n" , path);
      sched_yield(); // wait for other threads to free up file descriptors, try again
    } else {
      break;
    }
  }

  if (d) {
    while ((dentry = readdir(d)) != NULL) {  // openable directory
      if ((dentry->d_type != DT_UNKNOWN) && (dentry->d_name[0] != '.')) {  // good file and not hidden

        if (dentry->d_type == DT_DIR) {  // is a directory
          if (compare(filename, dentry->d_name)) {
            printf("MATCH: %s/%s (Directory)\n", path, dentry->d_name);
          }
          
          index->next = (struct list_node*)malloc(sizeof(fam));  // new node
          index->next->prev = index;
          index = index->next;
          
          
          index->obj = (struct threadstate*)malloc(sizeof(child_state));  // new child
          index->obj->depth = state->depth + 1;  
          index->obj->random_ptr = (char*)malloc(256);  // allocate space for path
          pathconcat(index->obj->random_ptr, path, dentry->d_name);  // give child the path it should open
          state->spawned = 1;
        }

    if ((dentry->d_type == DT_REG) && (compare(filename, dentry->d_name))) {  // normal file
      printf("MATCH: %s/%s T %u\n", path, dentry->d_name, threadcount);
        }
      }
    }
    closedir(d);
  } else {
    //printf("Could not open %s \n", path);
    state->done = 1;
    pthread_exit(NULL);
  }

  if (state->spawned) {
    while(index->prev != NULL) {  // go backwards on the LL

      if (!(pthread_create(&(index->me), NULL, &looknfind, (index->obj)))) {
      //made thread
        threadcount++;
        while(pthread_join(index->me, NULL)) {  // wait for it to finish before making new ones
          sched_yield();
        }
      } else {
        puts("Could not create thread");
        index->obj->done = 1;  // mark as ready for cleanup
      }
      if (index->obj->done) {
        free(index->obj->random_ptr);
        free(index->obj);
        index = index->prev;
        free(index->next);  // erase from list
        threadcount--;  // just for debugs
      }
    }
    
  }
  state->done = 1;
  pthread_exit(NULL);
  
  return 0;
}

int main(int argc, char** argv){
  pthread_t worker;  // worker thread
  struct threadstate state;  // control bits and a string ptr
  int filename_len;
  char* username;
  switch (argc){
  case 1:
      puts("Please specify a drive");
      return 0;
      break;
  case 2:
      puts("Please specify a file");
      return 0;
      break;
    case 3:
      puts("Default max depth of 5 will be used");
      max_depth = 5;
      break;
    case 4: 
      if (len(argv[3]) > 2){
        max_depth = (((unsigned int)argv[3][0] - 48)*100) + (((unsigned int)argv[3][1] - 48)*10) + ((unsigned int)argv[3][2] - 48);
      } else {
        max_depth = (argv[3][1]) ? (((unsigned int)argv[3][0] - 48)*10) + ((unsigned int)argv[3][1] - 48) : ((unsigned int)argv[3][0] - 48);
      }
      printf("Depth of %u will be used\n", max_depth);
      break;
  }

  if (argc < 2){
    return 0;
  }

  filename_len = 0;


  char paf[64];
  pathconcat(paf, "/mnt", argv[1]);
  if (chdir(paf)) {
    perror("Could not change directory");
    return 0;
  }
  while (argv[2][filename_len] != NULL){
    filename[filename_len] = argv[2][filename_len];
    filename_len++;
  }
  state.random_ptr = paf;
  state.depth = 0;
  threadcount++;
  pthread_create(&worker, NULL, &looknfind, &state);
  while (!state.done) {
    sched_yield();
  }
  if (pthread_join(worker, NULL)){
    perror("thread error");
  }
  puts("Done");
  return 0;
}
