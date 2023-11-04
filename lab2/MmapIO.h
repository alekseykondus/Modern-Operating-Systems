#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include "config.h"

#define read_bytes MmapIO_read_bytes
#define write_bytes MmapIO_write_bytes

typedef struct {
    int sender;
    int *other_ptr;
    int *size_ptr;
    uint8_t *data_ptr;
    bool closed;
} MmapIO;

void MmapIO_init(MmapIO *mmap_io, uint8_t *ptr, int sender) {
    mmap_io->sender = sender;
    mmap_io->other_ptr = (int *)ptr;
    mmap_io->size_ptr = ((int *)ptr) + 1;
    mmap_io->data_ptr = ptr + sizeof(int) * 2;
    mmap_io->closed = false;
}

void MmapIO_close(MmapIO *mmap_io) {
    mmap_io->closed = true;
    *mmap_io->size_ptr = -1;
}

void MmapIO_write_bytes(MmapIO *mmap_io, const uint8_t *bytes, int len) {
    if (mmap_io->closed) {
        return;
    }
    while (*mmap_io->size_ptr) {}
    if (*mmap_io->size_ptr == -1) {
        MmapIO_close(mmap_io);
        return;
    }
    memcpy(mmap_io->data_ptr, bytes, len);
    *mmap_io->size_ptr = len;
    *mmap_io->other_ptr = mmap_io->sender;
}

int MmapIO_read_bytes(MmapIO *mmap_io, uint8_t *out_data, int max_size) {
    if (mmap_io->closed) {
        return -1;
    }
    int size, other;
    do {
        other = *mmap_io->other_ptr;
        size = *mmap_io->size_ptr;
    } while (!size || other == mmap_io->sender);
    if (size == -1) {
        MmapIO_close(mmap_io);
        return -1;
    }
    memcpy(out_data, mmap_io->data_ptr, size);
    *mmap_io->size_ptr = 0;
    *mmap_io->other_ptr = mmap_io->sender;
    return size;
}


double compute_latency_MmapIO(MmapIO *file_io, uint64_t number_of_experiments) {
    double total_latency = 0;
    uint8_t data[128];
    uint8_t response[128];

    for (uint64_t k = 0; k < number_of_experiments; k++) {
        uint64_t startTime = getCurTime();
        for (uint64_t i = 0; i < sizeof(data); i++) {
            data[i] = i;
        }
        write_bytes(file_io, data, sizeof(data));
        int response_size = read_bytes(file_io, response, sizeof(response));

        for (uint64_t i = 0; i < sizeof(data); i++) {
            assert(data[i] == response[i]);
        }
        uint64_t endTime = getCurTime();
        total_latency += ((double)(endTime - startTime)/1000000.0) / 2;
    }
    printf("Latency: %f s\n", total_latency / (double)number_of_experiments);
    return total_latency / (double)number_of_experiments;
}

double compute_throughput_MmapIO(MmapIO *file_io, uint64_t number_of_experiments) {
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

double compute_capacity_MmapIO(MmapIO *file_io, uint64_t number_of_experiments) {
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


double* run_benchmark_MmapIO(const char *name, MmapIO *io_first, MmapIO *io_second) {
    double *result = (double *)malloc(sizeof(double));
    printf("Starting benchmark for method: %s\n", name);
    int p = fork();
    if (p == 0) {
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
        result[0] = compute_latency_MmapIO(io_first, NUMBER_OF_EXPERUMENTS*10000);
        result[1] = compute_throughput_MmapIO(io_first, NUMBER_OF_EXPERUMENTS);
        result[2] = compute_capacity_MmapIO(io_first, NUMBER_OF_EXPERUMENTS);
        MmapIO_close(io_first);
    }
    return result;
}
