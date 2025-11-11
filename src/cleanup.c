// cleanup.c
// gcc cleanup.c -o cleanup
#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shared.h"

int main(int argc, char** argv){
    const char* shm_name = (argc > 1) ? argv[1] : SHM_NAME;
    // chỉ cần unlink tên; kernel sẽ giải phóng khi không còn process nào giữ mmap/FD
    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink");
        return 1;
    }
    printf("Unlinked SHM '%s'\n", shm_name);
    return 0;
}
