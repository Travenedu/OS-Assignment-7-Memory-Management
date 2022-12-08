#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "list.h"
#include "util.h"

void TOUPPER(char * arr){
  
    for(int i=0;i<strlen(arr);i++){
        arr[i] = toupper(arr[i]);
    }
}

void get_input(char *args[], int input[][2], int *n, int *size, int *policy) 
{
  	FILE *input_file = fopen(args[1], "r");
	  if (!input_file) {
		    fprintf(stderr, "Error: Invalid filepath\n");
		    fflush(stdout);
		    exit(0);
	  }

    parse_file(input_file, input, n, size);
  
    fclose(input_file);
  
    TOUPPER(args[2]);
  
    if((strcmp(args[2],"-F") == 0) || (strcmp(args[2],"-FIFO") == 0)){
        *policy = 1;
    }
    else if((strcmp(args[2],"-B") == 0) || (strcmp(args[2],"-BESTFIT") == 0)){
        *policy = 2;
    }
    else if((strcmp(args[2],"-W") == 0) || (strcmp(args[2],"-WORSTFIT") == 0)){
        *policy = 3;
    }else {
       printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
       exit(1);
    }
        
}

void allocate_memory(list_t * freelist, list_t * alloclist, int pid, int blocksize, int policy) {
    /* if policy == 1 -> FIFO
    *              2 -> BESTFIT 
    *              3 -> WORSTFIT
    * 
    * blocksize - size of the block to allocate_memory
    * pid - process the block belongs to
    * alloclist - list of allocated memory blocksize
    * freelist - list of free memory blocks
    * 
    * 1. Check if a node is in the FREE_LIST with a blk(end - start) >= blocksize
    * 2. if so, remove it and go to #3, if not print ""Error: Memory Allocation <blocksize> blocks\n""
    * 3. set the blk.pid = pid
    * 4. set the blk.end = blk->start + blocksize - 1
    * 5. add the blk to the ALLOC_LIST in ascending order by address.
    * 6. Deal with the remaining left over memory (fragment).
    *     a. dynamically allocate a new block_t called fragment [use malloc]
    *     b. set the fragment->pid = 0 
    *     c. set the fragment->start = the blk.end + 1
    *     d. set the fragment->end = original blk.end before you changed it in #4
    *     e. add the fragment to the FREE_LIST based on policy
    */
  if (list_is_in_by_size(freelist, blocksize)){
      int index = list_get_index_of_by_Size(freelist, blocksize);
      block_t *block = list_remove_at_index(freelist, index);

      block->pid = pid;

      int old_end = block->end;
      block->end = block->start + blocksize - 1;

      list_add_ascending_by_address(alloclist, block);

      block_t *fragment = malloc(sizeof(block_t));
      fragment->pid = 0;
      fragment->start = block->end + 1;
      fragment->end = old_end;

      if(policy == 1){
          list_add_to_back(freelist, fragment);
      } 
      else if (policy == 2){
          list_add_ascending_by_blocksize(freelist, fragment);
      }
      else if (policy == 3){
          list_add_descending_by_blocksize(freelist, fragment);
      } else{
          printf("Error: Memory Allocation %d blocks\n", blocksize);
      }
  }

}

void deallocate_memory(list_t * alloclist, list_t * freelist, int pid, int policy) {   
    /* if policy == 1 -> FIFO
    *              2 -> BESTFIT 
    *              3 -> WORSTFIT
    * 
    * pid - process id of the block to deallocate 
    * alloclist - list of allocated memory blocksize
    * freelist - list of free memory blocks
    * 
    * 
    * 1. Check if a node is in the ALLOC_LIST with a blk.pid = pid
    * 2. if so, remove it and go to #3, if not print "Error: Can't locate Memory Used by PID: <pid>"
    * 3. set the blk.pid back to 0
    * 4. add the blk back to the FREE_LIST based on policy.
    */
    if (list_is_in_by_pid(alloclist, pid)){
        int index = list_get_index_of_by_Pid(alloclist, pid);
        block_t *block = list_remove_at_index(alloclist, index);

        block->pid = 0;
     

      if(policy == 1){
          list_add_to_back(freelist, block);
      } 
      else if (policy == 2){
          list_add_ascending_by_blocksize(freelist, block);
      }
      else if (policy == 3){
          list_add_descending_by_blocksize(freelist, block);
      } else{
          printf("Error: Can not locate Memory used by PID: %d", pid);
      }
  }
}

list_t* coalese_memory(list_t * list){
  list_t *temp_list = list_alloc();
  block_t *blk;
  
  while((blk = list_remove_from_front(list)) != NULL) {  // sort the list in ascending order by address
        list_add_ascending_by_address(temp_list, blk);
  }
  
  // try to combine physically adjacent blocks
  
  list_coalese_nodes(temp_list);
        
  return temp_list;
}

void print_list(list_t * list, char * message){
    node_t *current = list->head;
    block_t *blk;
    int i = 0;
  
    printf("%s:\n", message);
  
    while(current != NULL){
        blk = current->blk;
        printf("Block %d:\t START: %d\t END: %d", i, blk->start, blk->end);
      
        if(blk->pid != 0){
            printf("\t PID: %d\n", blk->pid);   
        }else{  
            printf("\n");
        }
        current = current->next;
        i += 1;
    }
}

/* DO NOT MODIFY */
int main(int argc, char *argv[]) 
{
   int PARTITION_SIZE, inputdata[200][2], N = 0, Memory_Mgt_Policy;
  
   list_t *FREE_LIST = list_alloc();   // list that holds all free blocks (PID is always zero)
   list_t *ALLOC_LIST = list_alloc();  // list that holds all allocated blocks
   int i;
  
   if(argc != 3) {
       printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
       exit(1);
   }
  
   get_input(argv, inputdata, &N, &PARTITION_SIZE, &Memory_Mgt_Policy);
  
   // Allocated the initial partition of size PARTITION_SIZE
   
   block_t * partition = malloc(sizeof(block_t));   // create the partition meta data
   partition->start = 0;
   partition->end = PARTITION_SIZE + partition->start - 1;
                                   
   list_add_to_front(FREE_LIST, partition);          // add partition to free list
                                   
   for(i = 0; i < N; i++) // loop through all the input data and simulate a memory management policy
   {
       printf("************************\n");
       if(inputdata[i][0] != -99999 && inputdata[i][0] > 0) {
             printf("ALLOCATE: %d FROM PID: %d\n", inputdata[i][1], inputdata[i][0]);
             allocate_memory(FREE_LIST, ALLOC_LIST, inputdata[i][0], inputdata[i][1], Memory_Mgt_Policy);
       }
       else if (inputdata[i][0] != -99999 && inputdata[i][0] < 0) {
             printf("DEALLOCATE MEM: PID %d\n", abs(inputdata[i][0]));
             deallocate_memory(ALLOC_LIST, FREE_LIST, abs(inputdata[i][0]), Memory_Mgt_Policy);
       }
       else {
             printf("COALESCE/COMPACT\n");
             FREE_LIST = coalese_memory(FREE_LIST);
       }   
     
       printf("************************\n");
       print_list(FREE_LIST, "Free Memory");
       print_list(ALLOC_LIST,"\nAllocated Memory");
       printf("\n\n");
   }
  
   list_free(FREE_LIST);
   list_free(ALLOC_LIST);
  
   return 0;
}
