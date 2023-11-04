#include "FileIO.h"
#include "MmapIO.h"
#include "SharedIO.h"

double* RunExperiment_FileIO(char* filename){
    FileIO file1, file2;
    FileIO_open(&file1, filename, 1);
    FileIO_open(&file2, filename, 2);
    return run_benchmark_fileIO("file_io", &file1, &file2);
}

double* RunExperiment_MmapIO(){
    const char *shm_name = "/my_shared_memory";
    size_t shm_size = 1024 * 1024;
    // Створюємо об'єкт спільної пам'яті
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(shm_fd, shm_size); // змінюємо розмір
    uint8_t *shm_ptr = (uint8_t *)mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); //відображення спільної пам'яті

    MmapIO io1, io2;
    MmapIO_init(&io1, shm_ptr, 1);
    MmapIO_init(&io2, shm_ptr, 2);

    double* result = run_benchmark_MmapIO("mmap_io", &io1, &io2);

    munmap(shm_ptr, shm_size);
    shm_unlink(shm_name);

    return result;
}

double* RunExperiment_SharedIO(){
    shm_t *ptr = shm_new((PACKET_SIZE + 8) * sizeof(uint8_t));
    SharedIO io1, io2;
    SharedIO_init(&io1, ptr, 0);
    SharedIO_init(&io2, ptr, 1);
    double* result = run_benchmark_SharedIO("shares_io", &io1, &io2);
    shm_del(ptr);
    return result;
}

void print_table_of_experiments(double *FileIO, double *MmapIO, double *SharedIO, int number_of_experiments) {
    printf("Number of experiments: %d\n", NUMBER_OF_EXPERUMENTS);
    printf("+----------+-------------+-------------------+-----------------+\n");
    printf("| IPC Type | Latency (s) | Throughput (MB/s) | Capacity (MB/s) |\n");
    printf("+----------+-------------+-------------------+-----------------+\n");
    printf("|  %s  |  %lf   |    %lf     |   %lf    |\n", "FileIO", FileIO[0], FileIO[1], FileIO[2]);
    printf("+----------+-------------+-------------------+-----------------+\n");
    printf("|  %s  |  %lf   |    %lf     |   %lf    |\n", "MmapIO", MmapIO[0], MmapIO[1], MmapIO[2]);
    printf("+----------+-------------+-------------------+-----------------+\n");
    printf("| %s |  %lf   |    %lf     |   %lf    |\n", "SharedIO", SharedIO[0], SharedIO[1], SharedIO[2]);
    printf("+----------+-------------+-------------------+-----------------+\n");
}

int main() {
    double *FileIO = (double *)malloc(sizeof(double));
    double *MmapIO = (double *)malloc(sizeof(double));
    double *SharedIO = (double *)malloc(sizeof(double));
    FileIO = RunExperiment_FileIO("file.txt");
    MmapIO = RunExperiment_MmapIO();
    SharedIO = RunExperiment_SharedIO();

    print_table_of_experiments(FileIO, MmapIO, SharedIO, 1);

    return 0;
}