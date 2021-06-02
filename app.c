

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

#include "sbmem.h"

int main()
{
    int i, ret; 
    char *p;  	

    sbmem_init(32768); 
    printf ("memory segment is created and initialized \n");


    ret = sbmem_open(); 
    if (ret == -1)
        exit (1); 
    
    p = sbmem_alloc (256); // allocate space to hold 1024 characters
    for (i = 0; i < 256; ++i)
        p[i] = 'a'; // init all chars to ‘a’
    sbmem_free (p);

    sbmem_close(); 


    sbmem_remove(); 

    printf ("memory segment is removed from the system. System is clean now.\n");
    printf ("you can no longer run processes to allocate memory using the library\n"); 
    
    return (0); 
}
