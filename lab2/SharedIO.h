#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include "config.h"

#define read_bytes SharedIO_read_bytes
#define write_bytes SharedIO_write_bytes
#define DEBUG 0

typedef struct {
    int id;
    size_t size;
} shm_t;

typedef struct {
    int sender;
    bool closed;
    shm_t *shm;
    void *shm_data;
} SharedIO;

// Определения функций связанных с shm_t
shm_t *shm_new(size_t size) {
    shm_t *shm = (shm_t *)malloc(sizeof(shm_t));
    shm->size = size;

    if ((shm->id = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) < 0) {
        perror("shmget");
        free(shm);
        return NULL;
    }

    return shm;
}

void shm_write(SharedIO *shared_io, char *data, int offset, int size) {
    /*void *shm_data;
    if ((shm_data = shmat(shm->id, NULL, 0)) == (void *) -1) {
        perror("write");
        return;
    }*/
    memcpy((char *)shared_io->shm_data + offset, data, size);
    //shmdt(shm_data);
}

void shm_read(char *data, SharedIO *shared_io, int offset, int size) {
    /*void *shm_data;
    if ((shm_data = shmat(shm->id, NULL, 0)) == (void *) -1) {
        perror("read");
        return;
    }*/
    memcpy(data, (char *)shared_io->shm_data + offset, size);
    //shmdt(shm_data);
}

void shm_del(shm_t *shm) {
    //shmctl(shm->id, IPC_RMID, 0);
    free(shm);
}

// Определения функций связанных с SharedIO
void SharedIO_init(SharedIO *shared_io, shm_t *shm, int sender) {
    shared_io->sender = sender;
    shared_io->shm = shm;
    shared_io->closed = false;

    if ((shared_io->shm_data = shmat(shm->id, NULL, 0)) == (void *) -1) {
        perror("error shmat");
        return;
    }
}

void SharedIO_close(SharedIO *shared_io) {
    shared_io->closed = true;
    int temp = -1;
    shm_write(shared_io, (char *) &temp, sizeof(int), sizeof(int));
}

void SharedIO_write_bytes(SharedIO *shared_io, const uint8_t *bytes, int len) {
    if (shared_io->closed) {
        return;
    }
    int size = 0;
    uint64_t startTime2 = getCurTime();
    do {
        shm_read((char *) &size, shared_io, sizeof(int), sizeof(int));
        if (size == -1) {
            SharedIO_close(shared_io);
            return;
        }
        //printf("size %d\n", size);
    } while (size != 0);
    uint64_t endTime2 = getCurTime();
    if (DEBUG) printf("WAITING WRITE TIME: %f\n", (double)(endTime2 - startTime2)/1000000.0);
    //printf("Write process %d\n", shared_io->sender);

    uint64_t startTime3 = getCurTime();
    shm_write(shared_io, (char *) bytes, sizeof(int) * 2, len);
    int temp = len;
    shm_write(shared_io, (char *) &temp, sizeof(int), sizeof(int));
    shm_write(shared_io, (char *) &shared_io->sender, 0, sizeof(int));

    uint64_t endTime3 = getCurTime();
    if (DEBUG) printf("        WRITE TIME: %f\n", (double)(endTime3 - startTime3)/1000000.0);
}

int SharedIO_read_bytes(SharedIO *shared_io, uint8_t *out_data, int max_size) {
    if (shared_io->closed) {
        return -1;
    }
    int size, other;
    uint64_t startTime2 = getCurTime();
    do {
        shm_read((char *) &other, shared_io, 0, sizeof(int));
        shm_read((char *) &size, shared_io, sizeof(int), sizeof(int));
        //printf("other %d\n", other);
    } while (!size || other == shared_io->sender);
    uint64_t endTime2 = getCurTime();
    if (DEBUG) printf("WAITING  READ TIME: %f\n", (double)(endTime2 - startTime2)/1000000.0);
    //printf("Read process %d\n", shared_io->sender);
    if (size == -1) {
        SharedIO_close(shared_io);
        return -1;
    }
    uint64_t startTime3 = getCurTime();
    shm_read((char *) out_data, shared_io, sizeof(int) * 2, size);

    int temp = 0;
    shm_write(shared_io, (char *) &temp, sizeof(int), sizeof(int));
    shm_write(shared_io, (char *) &shared_io->sender, 0, sizeof(int));

    uint64_t endTime3 = getCurTime();
    if (DEBUG) printf("        READ TIME: %f\n", (double)(endTime3 - startTime3)/1000000.0);

    return size;
}

double compute_latency_SharedIO(SharedIO *file_io, uint64_t number_of_experiments) {
    double total_latency = 0;
    uint8_t data[128];
    uint8_t response[128];

    for (uint64_t k = 0; k < number_of_experiments; k++) {
        uint64_t startTime = getCurTime();
        for (uint64_t i = 0; i < sizeof(data); i++) {
            data[i] = i;
        }
        uint64_t startTime2 = getCurTime();
        write_bytes(file_io, data, sizeof(data));
        uint64_t endTime2 = getCurTime();
        if (DEBUG) printf("WRITE TIME: %f\n", (double)(endTime2 - startTime2)/1000000.0);
        uint64_t startTime3 = getCurTime();
        int response_size = read_bytes(file_io, response, sizeof(response));
        uint64_t endTime3 = getCurTime();
        if (DEBUG) printf("READ  TIME: %f\n", (double)(endTime3 - startTime3)/1000000.0);
        for (uint64_t i = 0; i < sizeof(data); i++) {
            assert(data[i] == response[i]);
        }
        uint64_t endTime = getCurTime();
        total_latency += ((double)(endTime - startTime)/1000000.0) / 2;
    }
    printf("Latency: %f s\n", total_latency / (double)number_of_experiments);
    return total_latency / (double)number_of_experiments;
}

double compute_throughput_SharedIO(SharedIO *file_io, uint64_t number_of_experiments) {
    double throughput = 0;
    for(uint64_t n = 0; n < number_of_experiments; n++) {
        uint64_t startTime = getCurTime();
        uint8_t data[PACKET_SIZE];
        uint8_t response[PACKET_SIZE];
        uint64_t mega_bytes = 128;
        for (uint64_t i = 0; i < sizeof(data); i++) {
            data[i] = i;
        }
        for (uint64_t k = 0; k < mega_bytes * 1024 * 1024 / PACKET_SIZE; k++) {
            write_bytes(file_io, data, sizeof(data));
            int response_size = read_bytes(file_io, response, sizeof(response));
            for (uint64_t i = 0; i < sizeof(data); i++) {
                assert(data[i] == response[i]);
            }
        }
        uint64_t endTime = getCurTime();
        throughput += (double)mega_bytes / ((double)(endTime - startTime) / 1000000.0) * 2;
    }
    printf("Throughput: %f MB/s\n", throughput/(double)number_of_experiments);
    return throughput/(double)number_of_experiments;
}

double compute_capacity_SharedIO(SharedIO *file_io, uint64_t number_of_experiments) {
    double total_max_throughput = 0;
    for(uint64_t n = 0; n < number_of_experiments; n++){
        double max_throughput = 0;
        uint8_t data[PACKET_SIZE];
        uint8_t response[PACKET_SIZE];
        uint64_t mega_bytes = 16;

        for (int k = 0; k < 16; k++) {
            uint64_t startTime = getCurTime();
            for (uint64_t i = 0; i < sizeof(data); i++) {
                data[i] = i;
            }
            for (uint64_t k = 0; k < mega_bytes * 1024 * 1024 / PACKET_SIZE; k++) {
                write_bytes(file_io, data, sizeof(data));
                int response_size = read_bytes(file_io, response, sizeof(response));
                for (uint64_t i = 0; i < sizeof(data); i++) {
                    assert(data[i] == response[i]);
                }
            }
            uint64_t endTime = getCurTime();
            double throughput = (double)mega_bytes / ((double)(endTime - startTime)/1000000.0) * 2;
            max_throughput = (max_throughput < throughput) ? throughput : max_throughput;
        }
        total_max_throughput += max_throughput;
    }
    printf("Capacity: %f MB/s\n", total_max_throughput/(double)number_of_experiments);
    return total_max_throughput/(double)number_of_experiments;
}


double* run_benchmark_SharedIO(const char *name, SharedIO *io_first, SharedIO *io_second) {
    double *result = (double *)malloc(sizeof(double));
    printf("Starting benchmark for method: %s\n", name);
    int p = fork();
    if (p == 0) {
        // child
        uint8_t data[PACKET_SIZE];
        int data_size;
        do {
            data_size = read_bytes(io_second, data, sizeof(data));
            if (data_size > 0) {
                write_bytes(io_second, data, data_size);
            }
        } while (data_size > 0);
        exit(0);
    } else {
        // parent
        result[0] = compute_latency_SharedIO(io_first, NUMBER_OF_EXPERUMENTS*10000);
        result[1] = compute_throughput_SharedIO(io_first, NUMBER_OF_EXPERUMENTS);
        result[2] = compute_capacity_SharedIO(io_first, NUMBER_OF_EXPERUMENTS);
        SharedIO_close(io_first);
    }
    return result;
}
