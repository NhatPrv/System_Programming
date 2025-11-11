// reader.c
// gcc reader.c -o reader -pthread
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "shared.h"

static void usage(const char* prog){
    fprintf(stderr,
        "Usage: %s [-o output.txt] [-n /shm_name] [-w seconds]\n"
        "  -o  đường dẫn file output (mặc định: output.txt)\n"
        "  -n  tên POSIX shm (mặc định: %s)\n"
        "  -w  thời gian tối đa chờ writer tạo SHM (mặc định: 5 giây)\n",
        prog, SHM_NAME);
}

int main(int argc, char** argv){
    const char* out_path = "output.txt";
    const char* shm_name = SHM_NAME;
    int wait_secs = 30;

    int opt;
    while ((opt = getopt(argc, argv, "o:n:w:h")) != -1){
        if (opt == 'o') out_path = optarg;
        else if (opt == 'n') shm_name = optarg;
        else if (opt == 'w') wait_secs = atoi(optarg);
        else { usage(argv[0]); return 1; }
    }

    // 1) Chờ SHM xuất hiện (nếu chưa có)
    int shmfd = -1;
    for (int i = 0; i <= wait_secs * 10; ++i) { // mỗi 100ms
        shmfd = shm_open(shm_name, O_RDWR, 0666);
        if (shmfd >= 0) break;
        if (errno != ENOENT) { perror("shm_open"); return 1; }
        usleep(100 * 1000);
    }
    if (shmfd < 0) {
        fprintf(stderr, "Timed out waiting for SHM '%s'\n", shm_name);
        return 1;
    }

    struct stat st;
    if (fstat(shmfd, &st) == -1) { perror("fstat"); return 1; }
    if ((size_t)st.st_size < sizeof(Shared)) {
        fprintf(stderr, "SHM size too small.\n");
        return 1;
    }

    Shared* shm = mmap(NULL, sizeof(Shared), PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }

    FILE* fout = fopen(out_path, "w");
    if (!fout) { perror("open output"); return 1; }

    // 2) Vòng lặp tiêu thụ
    for (;;) {
        if (sem_wait(&shm->full) == -1) { perror("sem_wait full"); break; }
        if (sem_wait(&shm->mutex) == -1) { perror("sem_wait mutex"); break; }

        char msg[MSG_MAX];
        strncpy(msg, shm->buf[shm->out], MSG_MAX);
        msg[MSG_MAX-1] = '\0';
        shm->out = (shm->out + 1) % CAP;

        sem_post(&shm->mutex);
        sem_post(&shm->empty);

        if (strncmp(msg, END_TOKEN, MSG_MAX) == 0) {
            fprintf(stderr, "[reader] got END, exit.\n");
            break;
        }

        fprintf(fout, "%s\n", msg);
        fflush(fout);
        printf("[reader] wrote: %s\n", msg);
    }

    fclose(fout);
    munmap(shm, sizeof(*shm));
    close(shmfd);
    return 0;
}
