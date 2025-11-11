// gcc shm_file_demo.c -o shm_file_demo -pthread
// (nếu hệ thống yêu cầu thêm -lrt thì thêm vào cuối dòng lệnh)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>

#define SHM_NAME "/shm_file_demo"
#define CAP 4
#define MSG_MAX 128
#define END_TOKEN "END"

typedef struct {
    sem_t empty, full, mutex;
    size_t in, out;
    char buf[CAP][MSG_MAX];
} Shared;

static volatile sig_atomic_t stop_flag = 0;
void on_sigint(int){ stop_flag = 1; }

static void cleanup(int shmfd, Shared* shm) {
    if (shm) munmap(shm, sizeof(*shm));
    if (shmfd >= 0) close(shmfd);
    shm_unlink(SHM_NAME);
}

int main() {
    signal(SIGINT, on_sigint);

    // Tạo đối tượng shared memory
    int shmfd = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0666);
    if (shmfd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(shmfd, sizeof(Shared)) == -1) { perror("ftruncate"); return 1; }

    Shared* shm = mmap(NULL, sizeof(Shared), PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }

    memset(shm, 0, sizeof(*shm));
    sem_init(&shm->empty, 1, CAP);
    sem_init(&shm->full,  1, 0);
    sem_init(&shm->mutex, 1, 1);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); cleanup(shmfd, shm); return 1; }

    if (pid == 0) {
        // ===================== CHILD = CONSUMER =====================
        FILE* fout = fopen("output.txt", "w");
        if (!fout) { perror("open output.txt"); return 1; }

        while (!stop_flag) {
            sem_wait(&shm->full);
            sem_wait(&shm->mutex);

            char msg[MSG_MAX];
            strncpy(msg, shm->buf[shm->out], MSG_MAX);
            shm->out = (shm->out + 1) % CAP;

            sem_post(&shm->mutex);
            sem_post(&shm->empty);

            if (strncmp(msg, END_TOKEN, MSG_MAX) == 0) break;

            fprintf(fout, "%s\n", msg);
            fflush(fout);
            printf("[Consumer] Wrote: %s\n", msg);
        }

        fclose(fout);
        munmap(shm, sizeof(*shm));
        close(shmfd);
        return 0;

    } else {
        // ===================== PARENT = PRODUCER =====================
        FILE* fin = fopen("input.txt", "r");
        if (!fin) { perror("open input.txt"); cleanup(shmfd, shm); return 1; }

        char line[MSG_MAX];
        while (!stop_flag && fgets(line, sizeof(line), fin)) {
            line[strcspn(line, "\r\n")] = '\0'; // bỏ newline
            sem_wait(&shm->empty);
            sem_wait(&shm->mutex);

            strncpy(shm->buf[shm->in], line, MSG_MAX);
            shm->in = (shm->in + 1) % CAP;

            sem_post(&shm->mutex);
            sem_post(&shm->full);
        }

        // Gửi END token
        sem_wait(&shm->empty);
        sem_wait(&shm->mutex);
        strncpy(shm->buf[shm->in], END_TOKEN, MSG_MAX);
        shm->in = (shm->in + 1) % CAP;
        sem_post(&shm->mutex);
        sem_post(&shm->full);

        fclose(fin);
        waitpid(pid, NULL, 0);

        sem_destroy(&shm->empty);
        sem_destroy(&shm->full);
        sem_destroy(&shm->mutex);
        cleanup(shmfd, shm);

        printf(" Done. Check output.txt\n");
        return 0;
    }
}
