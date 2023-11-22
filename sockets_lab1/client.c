#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

const char *SOCKET_FILE = "socket";
const int SOCKET_PORT = 1234;
const int PACKET_SIZE = 40960000;

#define DEBUG false

static uint64_t getCurTime() {
    struct timespec tms;
    if (clock_gettime(CLOCK_REALTIME, &tms)) {
        return -1;
    }
    return tms.tv_sec * 1000000 + tms.tv_nsec / 1000;
}

int create_socket(int socket_type) {
    int socket_file_descriptor = socket(socket_type, SOCK_STREAM, 0);
    if (socket_file_descriptor == EXIT_FAILURE) {
        printf("Error during creating socket\n");
        exit(EXIT_FAILURE);
    }
    return socket_file_descriptor;
}

void init_inet_address(struct sockaddr_in *serv_addr) {
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(SOCKET_PORT);
}

void init_unix_address(struct sockaddr_un *serv_addr) {
    serv_addr->sun_family = AF_UNIX;
    strcpy(serv_addr->sun_path, SOCKET_FILE);
    //unlink(SOCKET_FILE);
}

int open_inet() {
    const int socket_file_descriptor = create_socket(AF_INET);

    struct sockaddr_in serv_addr;
    init_inet_address(&serv_addr);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported \n");
        exit(1);
    }

    if (connect(socket_file_descriptor, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == EXIT_FAILURE) {
        printf("Error during connecting socket\n");
        exit(EXIT_FAILURE);
    }

    return socket_file_descriptor;
}

int open_unix() {
    const int socket_file_descriptor = create_socket(AF_UNIX);

    struct sockaddr_un serv_addr;
    init_unix_address(&serv_addr);

    if (connect(socket_file_descriptor, (struct sockaddr *) &serv_addr,
                strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family)) == EXIT_FAILURE) {
        printf("Error during connecting socket\n");
        exit(EXIT_FAILURE);
    }

    return socket_file_descriptor;
}

int open_socket(const char *type_of_socket) {
    if (strcmp(type_of_socket, "UNIX") == 0) {
        return open_unix();
    } else if (strcmp(type_of_socket, "INET") == 0) {
        return open_inet();
    }
    return -1;
}

bool send_data(int socket_file_descriptor, const char *data, const int data_len) {
    int sent_data = write(socket_file_descriptor, data, data_len);
    if (sent_data != data_len) {
        return false;
    }
    return true;
}

char *generate_data(const int len) {
    char *data = malloc(len);
    for (int i = 0; i < len; ++i) {
        data[i] = rand() % 26 + 'A';
    }
    return data;
}

double *MeasureSocketOpeningTime(char *type_of_socket) {
    int socket_file_descriptor = 0;
    if (DEBUG)
        printf("Opening socket ...\n");
    uint64_t startTime, finishTime;
    startTime = getCurTime();
    socket_file_descriptor = open_socket(type_of_socket);
    finishTime = getCurTime();
    //printf("%s %.06f\n", "Socket Opening Time: ", (finishTime - startTime) / 1000000.0);

    double *result = (double *)malloc(2 * sizeof(double));
    result[0] = socket_file_descriptor;
    result[1] = (finishTime - startTime) / 1000000.0;
    return result;
}

double *MeasureSendingDataTime(int socket_file_descriptor, int number_of_packages) {
    if (DEBUG)
        printf("Generating data to send ...\n");
    char *data = generate_data(PACKET_SIZE);

    if (DEBUG)
        printf("Sending the data from client ...\n");
    uint64_t startTime, finishTime;
    startTime = getCurTime();

    double *result = (double *)malloc(2 * sizeof(double));
    result[1] = 0;
    for(int i = 0; i < number_of_packages; i++) {
        if (send_data(socket_file_descriptor, data, PACKET_SIZE)) {
            //if (DEBUG)
            printf("Successfully sent %d of data\n", PACKET_SIZE);
            result[1] += PACKET_SIZE;
        } else {
            printf("Error during sending the data\n");
        }
    }

    finishTime = getCurTime();
    //printf("%s %.06f\n", "Sending Data Time: ", (finishTime - startTime) / 1000000.0);
    result[0] = (finishTime - startTime) / 1000000.0;
    return result;
}

double MeasureSocketClosingTime(int socket_file_descriptor) {
    char *data = generate_data(PACKET_SIZE);
    if (DEBUG)
        printf("Closing socket.\n");
    uint64_t startTime, finishTime;
    startTime = getCurTime();
    close(socket_file_descriptor);
    finishTime = getCurTime();
    //printf("%s %.06f\n", "Socket Closing Time: ", (finishTime - startTime) / 1000000.0);
    return (finishTime - startTime) / 1000000.0;
}

void print_table_of_experiments(const char *type_of_socket, double *opening_time, double *sending_data_time, double *closing_time, int *data_quantity, int number_of_experiments) {
    printf("+-------------+--------------+-------------------+--------------+---------------+\n");
    printf("| Socket Type | Opening Time | Sending Data Time | Closing Time | Data quantity |\n");
    printf("+-------------+--------------+-------------------+--------------+---------------+\n");
    double average_opening_time = 0.0, average_sending_data_time = 0.0, average_closing_time = 0.0;
    uint64_t average_data_quantity = 0;
    for (int i = 0; i < number_of_experiments; ++i) {
        average_opening_time += opening_time[i];
        average_sending_data_time += sending_data_time[i];
        average_closing_time += closing_time[i];
        average_data_quantity += data_quantity[i];
        printf("|    %s     |   %lf   |     %lf      |   %lf   |     %d      |\n", type_of_socket, opening_time[i], sending_data_time[i], closing_time[i], data_quantity[i]);
    }
    average_opening_time /= number_of_experiments;
    average_sending_data_time /= number_of_experiments;
    average_closing_time /= number_of_experiments;
    average_data_quantity /= number_of_experiments;
    printf("+-------------+--------------+-------------------+--------------+---------------+\n");
    printf("|   %s   |   %lf   |     %lf      |   %lf   |     %lu      |\n", "AVERAGE", average_opening_time, average_sending_data_time, average_closing_time, average_data_quantity);
    printf("+-------------+--------------+-------------------+--------------+---------------+\n");
}

int main(int argc, char *argv[]) {
    int number_of_experiments = 10,
            number_of_packages = 1;

    char type_of_socket[] = "UNIX";
    double *opening_time = (double *)malloc(number_of_experiments * sizeof(double)),
            *sending_data_time = (double *)malloc(number_of_experiments * sizeof(double)),
            *closing_time = (double *)malloc(number_of_experiments * sizeof(double));
    int   *data_quantity = (int *)malloc(number_of_experiments * sizeof(int));

    for(int i = 0; i < number_of_experiments; ++i) {
        int socket_file_descriptor = 0;
        double *result = MeasureSocketOpeningTime(type_of_socket);
        socket_file_descriptor = (int)result[0];
        opening_time[i] = result[1];
        result = MeasureSendingDataTime(socket_file_descriptor, number_of_packages);
        sending_data_time[i] = result[0];
        data_quantity[i] = (int)result[1];
        closing_time[i] = MeasureSocketClosingTime(socket_file_descriptor);
    }

    print_table_of_experiments(type_of_socket, opening_time, sending_data_time, closing_time, data_quantity, number_of_experiments);
    return 0;
}