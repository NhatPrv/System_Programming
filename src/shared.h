#pragma once 
#include <semaphore.h>
#include <stddef.h>

#define SHM_NAME "/shm_file_demo"
#define CAP 4
#define MSG_MAX 128
#define END_TOKEN "END"

typedef struct {
    sem_t empty; // số ô trống 
    sem_t full;  // số ô đã có dữ liệu
    sem_t mutex; // khóa vùng tới hạn
    size_t in, out; // chỉ số vòng đệm
    char buf[CAP][MSG_MAX]; // vòng đệm
} Shared;