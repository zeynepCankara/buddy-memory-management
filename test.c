#include "sbmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>




int
test1()
{
    int i, ret; 
    char *p;  	

    printf("--- TEST: sbmem_init() ---\n");
    sbmem_init(32768);
    printf("library init \n");

    printf("--- TEST: sbmem_open() ---\n");
    ret = sbmem_open(); 
    if (ret == -1)
	    exit (1); 
    int * ptr = NULL;
    printf("--- TEST: sbmem_alloc() ---\n");
    ptr = sbmem_alloc(128);
    printf("level before free:   %d\n", *(ptr-4));
    printf("address after alloc:   %p\n", ptr);
    printf("--- TEST: sbmem_free() ---\n");
    sbmem_free(ptr);
    printf("level after free:   %d\n", *(ptr-4));
    printf("address after free:   %p\n", ptr);
    sbmem_close();
    printf("test code finished \n");
    sbmem_remove();

	return (0);
}



int generateRandomNum(unsigned int* seed)
{
	int random;
    random = (rand_r(seed) % 3969) + 128; //generating number between 128 and 4096
	printf("random number: %d\n", random);
    return random;
}


void test2(){

    int ret;
	char *p;
	// create_memory_sb.c
	int res = sbmem_init(32768);
	if (res == -1)
	{
		printf("ERROR: sbmem_init()");
		exit(1);
	}
 
	int notAvailable = 0;
	char* ptr[10] = {NULL};
	res = sbmem_open();
	if(res != -1){
		for(int i = 0; i < 50; i++){
            unsigned int seed = (unsigned int)i;
            int size = generateRandomNum(&seed);
            int index = size % 10;

            if(ptr[index] == NULL){
                ptr[index] = sbmem_alloc(size);
                if(ptr[index] == NULL){
                    notAvailable++;
                }
            }
            else{
                sbmem_free(ptr[index]);
                ptr[index] = NULL;
            }
		}
		printf("NUMBER OF FAIL COUNT: %d\n", notAvailable);
	}
	else{
		printf("ERROR: unsuccessful sbmem_open()");
		exit(1);
	}
	sbmem_close();

    //sbmem_remove();
}

void test3(){

    int ret;
	void *ptr = NULL;
	// create_memory_sb.c
    const int size = (1 << 10) * 128; // 256 KB
	int res = sbmem_init(262144);
	if (res == -1)
	{
		printf("ERROR: sbmem_init()");
		exit(1);
	}
 
	int notAvailable = 0;
	res = sbmem_open();
	if(res != -1){
		for(int i = 0; i < 100000000; i++){
            ptr = sbmem_alloc(128);
            printf("nof request %d\n", notAvailable);
            notAvailable++;
      
		}
		printf("NUMBER OF FAIL COUNT: %d\n", notAvailable);
	}
	else{
		printf("ERROR: unsuccessful sbmem_open()");
		exit(1);
	}
	sbmem_close();

    sbmem_remove();
}


int main()
{

    test1();
    //test2();
    //test3();
   
	return (0);
}