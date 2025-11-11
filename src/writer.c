// writer.c
// gcc writer.c -o writer -pthread
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
        "Usage: %s [-i input.txt] [-n /shm_name]\n"
        "  -i  đường dẫn file input (mặc định: input.txt)\n"
        "  -n  tên POSIX shm (mặc định: %s)\n",
        prog, SHM_NAME);
}

int main(int argc, char** argv){
    const char* in_path = "input.txt";
    const char* shm_name = SHM_NAME;

    int opt;
    while ((opt = getopt(argc, argv, "i:n:h")) != -1){
        if (opt == 'i') in_path = optarg;
        else if (opt == 'n') shm_name = optarg;
        else { usage(argv[0]); return 1; }
    }

    // 1) Mở/khởi tạo shared memory (tạo mới nếu chưa có)
    int creator = 0;
    int shmfd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shmfd >= 0) {
        creator = 1;
    } else if (errno == EEXIST) {
        shmfd = shm_open(shm_name, O_RDWR, 0666);
        if (shmfd < 0) { perror("shm_open existing"); return 1; }
    } else {
        perror("shm_open");
        return 1;
    }

    if (creator) {
        if (ftruncate(shmfd, sizeof(Shared)) == -1) { perror("ftruncate"); return 1; }
    } else {
        // nếu không phải creator, đảm bảo đối tượng đã có size đúng
        struct stat st; 
        if (fstat(shmfd, &st) == -1) { perror("fstat"); return 1; }
        if ((size_t)st.st_size < sizeof(Shared)) {
            fprintf(stderr, "SHM size too small.\n");
            return 1;
        }
    }

    Shared* shm = mmap(NULL, sizeof(Shared), PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }

    if (creator) {
        // init cấu trúc dùng chung
        memset(shm, 0, sizeof(*shm));
        if (sem_init(&shm->empty, 1, CAP) == -1) { perror("sem_init empty"); return 1; }
        if (sem_init(&shm->full,  1, 0  ) == -1) { perror("sem_init full");  return 1; }
        if (sem_init(&shm->mutex, 1, 1  ) == -1) { perror("sem_init mutex"); return 1; }
        fprintf(stderr, "[writer] created and initialized SHM '%s'\n", shm_name);
    } else {
        fprintf(stderr, "[writer] attached to existing SHM '%s'\n", shm_name);
    }

    // 2) Đọc file input và đẩy vào vòng đệm
    FILE* fin = fopen(in_path, "r");
    if (!fin) { perror("open input"); return 1; }

    char line[MSG_MAX];
    while (fgets(line, sizeof(line), fin)) {
        line[strcspn(line, "\r\n")] = '\0';

        // Đợi có ô trống và vào critical section
        if (sem_wait(&shm->empty) == -1) { perror("sem_wait empty"); break; }
        if (sem_wait(&shm->mutex) == -1) { perror("sem_wait mutex"); break; }

        // Ghi message
        strncpy(shm->buf[shm->in], line, MSG_MAX-1);
        shm->buf[shm->in][MSG_MAX-1] = '\0';
        shm->in = (shm->in + 1) % CAP;

        // Thoát critical section và báo có dữ liệu
        sem_post(&shm->mutex);
        sem_post(&shm->full);
    }

    // 3) Gửi END_TOKEN để reader thoát
    if (sem_wait(&shm->empty) == -1) { perror("sem_wait empty(END)"); }
    if (sem_wait(&shm->mutex) == -1) { perror("sem_wait mutex(END)"); }
    strncpy(shm->buf[shm->in], END_TOKEN, MSG_MAX);
    shm->in = (shm->in + 1) % CAP;
    sem_post(&shm->mutex);
    sem_post(&shm->full);

    fclose(fin);

    // Không sem_destroy hay shm_unlink ở đây để reader còn chạy an toàn.
    // (Sau khi demo xong, chạy tool cleanup riêng hoặc unlink thủ công.)

    munmap(shm, sizeof(*shm));
    close(shmfd);

    fprintf(stderr, "[writer] done.\n");
    return 0;
}
