#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "pidinfo.h"

/* Part I */
void run_current()
{
    int pid = 0, file;
    // Pointer and pointer to pointer, not sure of a better way to
    // do it.. for now.
    int *ptr = &pid, *pptr = &ptr;

    // Open the file for writing, write to it and close it.
    // Always check for errors.
    if((file = open("/sys/kernel/kernellab/kcurrent", O_WRONLY)) < 0)
    {
        printf("open error.\n");
        exit(1);
    }
    if(write(file, pptr, 11) < 0)
    {
        printf("write error.\n");
        exit(1);
    }

    if(close(file) < 0)
    {
        printf("close error.\n");
        exit(1);
    }
    
    printf("Current PID: %d\n", pid);
}

/* Part II */
void run_pid(int pid)
{
    int file;
    struct pid_info info;
    // Pointer and pointer to pointer, not sure of a better way to
    // do it.. for now.
    struct sysfs_message message, *ptr = &message, *pptr = &ptr;
 
    message.pid = pid;//getpid();
    message.address = &info;

    // Open the file for writing, write to it and close it.
    // Always check for errors.
    if((file = open("/sys/kernel/kernellab/pid", O_WRONLY)) < 0)
    {
        printf("open error.\n");
        exit(1);
    }
    if(write(file, pptr, 11) < 0)
    {
        printf("write error.\n");
        exit(1);
    }

    if(close(file) < 0)
    {
        printf("close error.\n");
        exit(1);
    }

    printf("PID: %d\n", info.pid);
    printf("COMM: %s\n", info.comm);
    printf("State: %ld\n", info.state);
}


int main(int argc, char **argv)
{
    int pid = -1;

    if(argc < 2)
    {
        printf("Usage: ./request-pid <pid>\n");
        exit(1);
    }

    if(atoi(argv[1]))
        sscanf(argv[1], "%d", &pid);

    run_current();
    run_pid(pid);
    return EXIT_SUCCESS;
}
