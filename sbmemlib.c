#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include "sbmem.h"
#include <math.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

// Shared memory variable(s)
#define PID_SIZE 8
#define SEGMENTSIZE_OFFSET 4
#define LEVEL_SIZE 4
#define IS_ALLOC_SIZE 4
#define NEXT_PTR_SIZE 8
#define MAX_SHM_SIZE_OFFSET 19 // (including 0) (256 KB = 18 bit + 1 bit (for 0))
#define SEM_NAME "/semaphore"

#define MAX_PROCESS_COUNT 10
#define MAX_LEVEL 18
#define MIN_REQ_SIZE 128
#define MAX_REQ_SIZE 4096


const int MAX_SHM_SIZE = (1 << 10) * 256; // 256 KB
const int MIN_SHM_SIZE = (1 << 10) * 32; // 32 KB
const int BLOCK_OVERHEAD_SIZE = LEVEL_SIZE + IS_ALLOC_SIZE + NEXT_PTR_SIZE;
const int SHM_OFFSET = SEGMENTSIZE_OFFSET + (MAX_PROCESS_COUNT * PID_SIZE) + (MAX_SHM_SIZE_OFFSET * NEXT_PTR_SIZE); // to access address of the user space


// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
#define sharedMemoryName "/sbmem"
// Define semaphore(s)
sem_t *semaphore;

// Define your stuctures and variables.
struct managementInfo{
    int shmSize;
    pid_t pids[10]; //max process count
    //heads of the linked lists for free spaces that will be used in buddy algorithm
    int* freeSpaces[19];
};

void* self; //starting address(offset) of the shared memory segment in virtual address space of the process using the library at the moment
int shm_fd;


// utility functions 
int is_pow_of_2(int num)
{
	return !(num & (num - 1));
}

int next_pow_of_2(int num)
{
	if (is_pow_of_2(num))
		return num;
	for (int i = 1; i <= 16; i *= 2)
	{
		num |= num >> i;
	}
	return num + 1;
}

int size_to_level(int size){
    return (int)(log2(size));
}


int get_mem_info_pointer(int level){
    return SEGMENTSIZE_OFFSET + (MAX_PROCESS_COUNT * PID_SIZE) + (level * NEXT_PTR_SIZE);
}



// project specific methods

int sbmem_init(int segmentsize)
{
    if (segmentsize < MIN_SHM_SIZE || segmentsize > MAX_SHM_SIZE)
    {
        return -1;
    }
    shm_fd = shm_open(sharedMemoryName, O_CREAT | O_RDWR | O_EXCL, 0666);    
    //Test for allocating shared memory
    if(shm_fd < 0){
        if(errno != EEXIST){
            return -1;
        }
        sbmem_remove();
        shm_fd = shm_open(sharedMemoryName, O_CREAT | O_RDWR | O_EXCL, 0666);
    }
    if (shm_fd < 0)
    {
        return -1;
    }
    int manInfoSize = sizeof(struct managementInfo); //size of management info struct 
    //total size of the shared memory segment

    if(!is_pow_of_2(segmentsize)){
        segmentsize = next_pow_of_2(segmentsize);
    }
    int size = segmentsize + manInfoSize;
    ftruncate(shm_fd, size);
    

    //initializing memony management information
    void* ptr = mmap(NULL,manInfoSize, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    struct managementInfo*  manInfo = (struct managementInfo*)ptr;
    //initializing size of the shared memory(including management info size)
    manInfo->shmSize = size;
    //initializing pids
    for(int i = 0; i < 10; i++){
        manInfo->pids[i] = -1;
    }

    for(int i = 0; i < 19; i++){
        manInfo->freeSpaces[i] = 0;
    }
    int level = size_to_level(segmentsize);


    int * user_seg_address = (ptr + get_mem_info_pointer(level));
    *((int*) user_seg_address) = sizeof(struct managementInfo) + LEVEL_SIZE + IS_ALLOC_SIZE;


    
    semaphore = sem_open (SEM_NAME, O_CREAT | O_EXCL, 0644, 1); 
    sem_unlink (SEM_NAME);     
   

    return (0); 
}

int sbmem_remove()
{


    if (shm_unlink(sharedMemoryName) < 0)
    {
        sem_destroy(semaphore);
        return -1;
    }
    sem_destroy(semaphore);
    return 0; // success
}

int sbmem_open()
{
    
    // entry section
    sem_wait(semaphore);

    int shm_fd = shm_open(sharedMemoryName, O_RDWR, 0666);
    void* ptr = mmap(NULL, sizeof(struct managementInfo), PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    struct managementInfo* manInfo = (struct managementInfo*)ptr;
    int size = manInfo->shmSize; 
    munmap(ptr, sizeof(struct managementInfo));
    ftruncate(shm_fd, size);

    self = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    struct managementInfo* manageInfo = (struct managementInfo*)self;

    //Check the if the number of processes exceeds the max count. If not put its pids in the shared memory segment
    int isAvailable = -1;
    for(int i = 0; i < 10; i++){
        if(manageInfo->pids[i] == -1){
            isAvailable = i;
            break;
        }
    }

    if(isAvailable == -1){
        sem_post(semaphore);
        return -1;
    }

    manageInfo->pids[isAvailable] = getpid();

    sem_post(semaphore);

    return (0); 
}


void *sbmem_alloc (int size)
{
    // enter section
    sem_wait(semaphore);

    if (size < MIN_REQ_SIZE || size > MAX_REQ_SIZE)
    {
        sem_post(semaphore);
        return NULL;
    }

    // determine size to allocate
    int allocsize = (size + BLOCK_OVERHEAD_SIZE);
    if(is_pow_of_2(allocsize) == 0){
        allocsize = next_pow_of_2(allocsize);
    }

    

    // find position in the shared memory
    int alloclevel = size_to_level(allocsize);

    int man_pointer_offset = get_mem_info_pointer(alloclevel);


    int current_level = alloclevel;
    while( *(int*)(self + man_pointer_offset) == 0 && current_level != MAX_LEVEL){
        // iterate forward until you find available spot
        man_pointer_offset += NEXT_PTR_SIZE;
        current_level += 1;
    }

    // end of levels no block available for allocation
    if(current_level == MAX_LEVEL){
        sem_post(semaphore);
        return NULL;
    }

    // find the spot you can allocate in shared memory split if required
    while (current_level != alloclevel)
    {

        int offset = *(int*)(self + man_pointer_offset);
        int temp = offset;
        *(int*)(self + man_pointer_offset) = *(int*)(self + offset);


        man_pointer_offset -= NEXT_PTR_SIZE;
        *(int*)(self + man_pointer_offset) = temp;
        current_level -= 1;
        *(int*)(self + temp - IS_ALLOC_SIZE) = 0; // 1. block in shared memory segment
        *(int*)(self + temp - IS_ALLOC_SIZE - LEVEL_SIZE) = current_level;
        *(int*)(self + temp) = (temp + (int)(pow(2, current_level)));



        *(int*)(self + *(int*)(self + temp) - IS_ALLOC_SIZE) = 0; //2. block in shared memory segment
        *(int*)(self + *(int*)(self + temp) - IS_ALLOC_SIZE - LEVEL_SIZE) = current_level;
        *(int*)(self + *(int*)(self + temp) ) = 0;


    }
    int offset = *(int*)(self + man_pointer_offset);
    *(int*)(self + offset - IS_ALLOC_SIZE) = 1; // allocate the space before giving it to the user
    *(int*)(self + man_pointer_offset) = *(int*)(self + offset);
    *(int*)(self + offset ) = 0; // shouldn't have a next pointing link (added 21/04/2021)
    void * alloc_ptr =  self + offset + NEXT_PTR_SIZE;

    // exit section
    sem_post(semaphore);
    return alloc_ptr;
}

int find_buddy_offset(void * p, int level){
    int relative_offset = p - self - (sizeof(struct managementInfo) + NEXT_PTR_SIZE + LEVEL_SIZE + IS_ALLOC_SIZE );
    int mod_p = relative_offset % (int)pow(2, level+1);
    if(mod_p == 0) {
        relative_offset =  relative_offset + (int)pow(2, level);
    } else {
        relative_offset =  relative_offset - (int)pow(2, level);
    }
    relative_offset = relative_offset + sizeof(struct managementInfo) + LEVEL_SIZE + IS_ALLOC_SIZE; 
    return relative_offset;
}

void sbmem_free (void *p)
{
    // entry section
    sem_wait(semaphore);

    // check whether the position is allocated
    if(*(int*)(p - NEXT_PTR_SIZE - IS_ALLOC_SIZE) == 0){
        //sem_post(semaphore);
        return;
    }

    // check the unique buddy
    int segment_level = *(int*)(p - NEXT_PTR_SIZE - IS_ALLOC_SIZE - LEVEL_SIZE);
    int buddy_offset = find_buddy_offset(p, segment_level);
    int is_buddy_allocated = *(int*)(self + buddy_offset - IS_ALLOC_SIZE);
    int buddy_level = *(int*)(self + buddy_offset - IS_ALLOC_SIZE - LEVEL_SIZE);
    // buddy_offset needs self + to access address

    if(is_buddy_allocated == 1){

        *(int*)(p - NEXT_PTR_SIZE - IS_ALLOC_SIZE ) = 0; // deallocate
        // add to the head of the pointer corresponding
        int man_pointer_offset = get_mem_info_pointer(segment_level);
        *(int*)(p - NEXT_PTR_SIZE ) = *(int*)(self + man_pointer_offset );
        *(int*)(self + man_pointer_offset ) = (p - NEXT_PTR_SIZE - self);
    } 
    int max_level = size_to_level((*(int*)(self)  - sizeof(struct managementInfo)));
    while(is_buddy_allocated == 0 && buddy_level == segment_level && segment_level <  max_level ){
    
        int man_pointer_offset = get_mem_info_pointer(segment_level);
        int offset = *(int*)(self + man_pointer_offset);
        int prev = man_pointer_offset;
        int temp;

        while(offset != buddy_offset){
            temp = offset;
            offset = *(int*)(self + offset);
            prev = temp;
        }

        *(int*)(self + prev) = *(int*)(self + offset);

        segment_level += 1;
        *(int*)(p - NEXT_PTR_SIZE - IS_ALLOC_SIZE ) = 0; // deallocate
        *(int*)(p - NEXT_PTR_SIZE - IS_ALLOC_SIZE - LEVEL_SIZE) = segment_level; // increment the level since now it is a merged block
        // add to the head of the pointer corresponding
        man_pointer_offset = get_mem_info_pointer(segment_level);

        if((buddy_offset -  (p  -  self -  NEXT_PTR_SIZE)) > 0 ){
            // buddy right
            *(int*)(p - NEXT_PTR_SIZE ) = *(int*)(self + man_pointer_offset);
            *(int*)(self + man_pointer_offset ) = (p - NEXT_PTR_SIZE - self);
       
        } else {
            //  buddy left
            *(int*)(buddy_offset + self) = *(int*)(self + man_pointer_offset);
            *(int*)(self + man_pointer_offset ) = (buddy_offset);
      
        }
        p = self + *(int*)(self + man_pointer_offset) + NEXT_PTR_SIZE;
        buddy_offset = find_buddy_offset(p, segment_level);
        is_buddy_allocated = *(int*)(self + buddy_offset - IS_ALLOC_SIZE);
        buddy_level = *(int*)(self + buddy_offset - IS_ALLOC_SIZE - LEVEL_SIZE);

    }

    // exit section
    sem_post(semaphore);
}

int sbmem_close()
{
    // entry section
    sem_wait(semaphore);
    pid_t closingProcessId = getpid();
    int shmem_fd = shm_open(sharedMemoryName, O_RDWR, 0666);
    void* ptr = mmap(NULL, sizeof(struct managementInfo), PROT_WRITE | PROT_READ, MAP_SHARED, shmem_fd, 0);
    struct managementInfo* manInfo = (struct managementInfo*)ptr;
    int size = manInfo->shmSize; 
    int res;

    for(int i = 0; i < 10; i++){
        if(manInfo->pids[i] == closingProcessId){
            manInfo->pids[i] = -1;
            res = munmap(self, size);
            if(res < 0){
                sem_post(semaphore);
                return -1;
            }
        }
    }
    res = munmap(ptr, sizeof(struct managementInfo));
    if(res < 0){
        sem_post(semaphore);
        return -1;
    }
    
    // exit section
    sem_post(semaphore);
    return (0); 
}
