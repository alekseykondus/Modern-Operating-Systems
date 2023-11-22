#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "config.h"

#define read_bytes FileIO_read_bytes
#define write_bytes FileIO_write_bytes
#define DEBUG 0

static uint64_t getCurTime() {
    struct timespec tms;
    if (clock_gettime(CLOCK_REALTIME, &tms)) {
        return -1;
    }
    return tms.tv_sec * 1000000 + tms.tv_nsec / 1000;
}

typedef struct {
    FILE *file;
    bool closed;
    int sender;
} FileIO;

void FileIO_open(FileIO *file_io, const char *filename, int sender) {
    file_io->sender = sender;
    file_io->file = fopen(filename, "w+");
    file_io->closed = false;
    fwrite(&sender, sizeof(int), 1, file_io->file);
    int temp = 0;
    fwrite(&temp, sizeof(int), 1, file_io->file);
    fflush(file_io->file);
}

void FileIO_close(FileIO *file_io) {
    file_io->closed = true;
    fseek(file_io->file, sizeof(int), SEEK_SET);
    int temp = -1;
    fwrite(&temp, sizeof(int), 1, file_io->file);
    fflush(file_io->file);
}

void FileIO_write_bytes(FileIO *file_io, const uint8_t *bytes, int len) {
    if (file_io->closed) {
        return;
    }
    int other = 0;
    int prev = 0;
    uint64_t startTime2 = getCurTime();
    do {
        fseek(file_io->file, 0, SEEK_SET);
        fread(&other, sizeof(int), 1, file_io->file);
        fread(&prev, sizeof(int), 1, file_io->file);
        if (prev == -1) {
            file_io->closed = true;
            return;
        }
        //printf("prev %d\n", prev);
    } while (prev != 0); // (temp == 0 -> виходимо з циклу), перевіряємо, чи останній був read чи write

    //printf("Write process %d\n", file_io->sender);
    uint64_t endTime2 = getCurTime();
    if (DEBUG) printf("WAITING WRITE TIME: %f\n", (double)(endTime2 - startTime2)/1000000.0);


    uint64_t startTime3 = getCurTime();

    fseek(file_io->file, sizeof(int) * 2, SEEK_SET);
    fwrite(bytes, sizeof(uint8_t), len, file_io->file);
    fseek(file_io->file, 0, SEEK_SET);
    fwrite(&(file_io->sender), sizeof(int), 1, file_io->file);
    fwrite(&len, sizeof(int), 1, file_io->file);
    fflush(file_io->file); // щоб дані з буфера скинути на диск
    uint64_t endTime3 = getCurTime();
    if (DEBUG) printf("        WRITE TIME: %f\n", (double)(endTime3 - startTime3)/1000000.0);
}

int FileIO_read_bytes(FileIO *file_io, uint8_t *out_data, int max_size) {
    if (file_io->closed) {
        return -1;
    }

    int other = 0;
    int size = 0;
    int64_t startTime2 = getCurTime();
    while (!size || other == file_io->sender) {
        fflush(file_io->file);
        fseek(file_io->file, 0, SEEK_SET);
        fread(&other, sizeof(int), 1, file_io->file);
        fread(&size, sizeof(int), 1, file_io->file);
        //printf("other %d I am sender #%d\n", other, file_io->sender);
    }
    uint64_t endTime2 = getCurTime();
    if (DEBUG) printf("WAITING READ TIME: %f\n", (double)(endTime2 - startTime2)/1000000.0);
    //printf("Read process %d\n", file_io->sender);
    if (size == -1) {
        file_io->closed = true;
        return -1;
    }

    uint64_t startTime3 = getCurTime();
    fread(out_data, sizeof(uint8_t), size, file_io->file);

    int temp = 0;
    fseek(file_io->file, 0, SEEK_SET);
    fwrite(&(file_io->sender), sizeof(int), 1, file_io->file);
    fwrite(&temp, sizeof(int), 1, file_io->file);
    fflush(file_io->file);
    uint64_t endTime3 = getCurTime();
    if (DEBUG) printf("        READ TIME: %f\n", (double)(endTime3 - startTime3)/1000000.0);

    return size;
}



double compute_latency_FileIO(FileIO *file_io, uint64_t number_of_experiments) {
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

double compute_throughput_FileIO(FileIO *file_io, uint64_t number_of_experiments) {
    double throughput = 0;
    for(uint64_t n = 0; n < number_of_experiments; n++){
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
        throughput += (double)mega_bytes / ((double)(endTime - startTime)/1000000.0) * 2;
    }
    printf("Throughput: %f MB/s\n", throughput/(double)number_of_experiments);
    return throughput/(double)number_of_experiments;
}

double compute_capacity_FileIO(FileIO *file_io, uint64_t number_of_experiments) {
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

double* run_benchmark_fileIO(const char *name, FileIO *file1, FileIO *file2) {
    double *result = (double *)malloc(sizeof(double));
    printf("Starting benchmark for method: %s\n", name);
    int p = fork();
    if (p == 0) {
        // child
        uint8_t data[PACKET_SIZE];
        int data_size;
        do {
            data_size = read_bytes(file2, data, sizeof(data));
            if (data_size > 0) {
                write_bytes(file2, data, data_size);
            }
        } while (data_size > 0);
        exit(0);
    } else {
        // parent
        result[0] = compute_latency_FileIO(file1, NUMBER_OF_EXPERUMENTS*10000);
        result[1] = compute_throughput_FileIO(file1, NUMBER_OF_EXPERUMENTS);
        result[2] = compute_capacity_FileIO(file1, NUMBER_OF_EXPERUMENTS);
        FileIO_close(file1);
    }
    return result;
}
