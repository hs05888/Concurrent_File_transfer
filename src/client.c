#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define PORT 1000
#define INT_SIZE 4
#define LONG_SIZE 8
#define STRING_SIZE 100
#define SOCKET_CHUNK_SIZE 200


int setupSocket(int i)
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT + i);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed for Port %d\n", PORT + i);
        return -1;
    }

    return sock;
}

struct args
{
    long size;
    int thread_number;
};

void createEmptyFile(char *fileName, int x)
{
    FILE *fp = fopen(fileName, "wb");
    fseek(fp, x - 1, SEEK_SET);
    fputc('\0', fp);
    fclose(fp);
}

void *recieveChunk(void *input)
{
    long size = ((struct args *)input)->size;
    int thread_number = ((struct args *)input)->thread_number;
    char *chunk = malloc(size);
    int port = setupSocket(thread_number);
    int chunk_of_chunk_size = SOCKET_CHUNK_SIZE;

    if (size < chunk_of_chunk_size)
        read(port, chunk, size);

    else
    {
        int number_of_chunks_of_chunk = size / chunk_of_chunk_size;
        int extra_space_left = size % chunk_of_chunk_size;

        int start = 0;
        int end = chunk_of_chunk_size;

        for (int i = 0; i < number_of_chunks_of_chunk; i++)
        {
            char *chunk_of_chunk = malloc(chunk_of_chunk_size);
            read(port, chunk_of_chunk, chunk_of_chunk_size);
            int x = 0;
            for (int j = start; j < end; j++)
            {
                chunk[j] = chunk_of_chunk[x];
                x += 1;
            }
            start = end;
            end += chunk_of_chunk_size;
            free(chunk_of_chunk);
        }

        if (extra_space_left)
        {
            char *extra_space_of_chunk = malloc(extra_space_left);
            read(port, extra_space_of_chunk, extra_space_left);
            int x = 0;
            for (int j = size - extra_space_left; j < size; j++)
            {
                chunk[j] = extra_space_of_chunk[x];
                x += 1;
            }
            free(extra_space_of_chunk);
        }
    }
    return (void *)chunk;
}

int main(int argc, char const *argv[])
{
    printf("Welcome to Process B.\n");
    int sock = setupSocket(-1);

    char path_to_inputFile[STRING_SIZE];
    printf("Enter path to input file: ");
    scanf("%s", path_to_inputFile);

    send(sock, path_to_inputFile, STRING_SIZE, 0);

    char *file_found_str = malloc(STRING_SIZE);
    read(sock, file_found_str, STRING_SIZE);

    int file_found = atoi(file_found_str);

    if (!file_found)
    {
        printf("Process A could not find the requested file. ");
        exit(1);
    }

    printf("Process A successfully found the file. \n");

    char path_to_outputFile[STRING_SIZE];
    printf("Enter path to output file: ");
    scanf("%s", path_to_outputFile);

    int number_of_chunks;
    printf("Enter number of threads: ");
    scanf("%d", &number_of_chunks);

    char *number_of_chunks_str = malloc(STRING_SIZE);
    sprintf(number_of_chunks_str, "%d", number_of_chunks);

    send(sock, number_of_chunks_str, STRING_SIZE, 0);

    char *file_size_str = malloc(STRING_SIZE);
    read(sock, file_size_str, STRING_SIZE);

    long file_size = atoll(file_size_str);

    printf("%s\n", file_size_str);

    printf("File size: %ld\n", file_size);

    long chunk_size = file_size / number_of_chunks;

    printf("Chunk Size: %ld\n", chunk_size);

    int extra_space_size = file_size % number_of_chunks;
    printf("Extra space size = %d\n \n", extra_space_size);

    createEmptyFile(path_to_outputFile, (chunk_size * number_of_chunks) - extra_space_size);
    FILE *fp = fopen(path_to_outputFile, "r+b");

    printf("Successfully created an empty file %s\n \n", path_to_outputFile);

    printf("Writing Data recieved on this file...\n");

    pthread_t threads[number_of_chunks];

    struct args *data[number_of_chunks];

    for (int i = 0; i < number_of_chunks; i++)
    {
        data[i] = (struct args *)malloc(sizeof(struct args));
    }

    sleep(1);

    for (int i = 0; i < number_of_chunks; i++)
    {
        int size = chunk_size;
        if (i == (number_of_chunks - 1))
            size += extra_space_size;
        data[i]->size = size;
        data[i]->thread_number = i;
        pthread_create(&threads[i], NULL, recieveChunk, (void *)data[i]);
    }

    int x = 0;
    for (int i = 0; i < number_of_chunks; i++)
    {
        void *chunk;
        pthread_join(threads[i], &chunk);
        int size = chunk_size;
        int offset = i * size;
        if (i == number_of_chunks - 1)
            size += extra_space_size;
        pwrite(fileno(fp), (char *)chunk, size, offset);
        x += strlen((char *)chunk);
        free(chunk);
    }

    printf("ok = %d", x);

    for (int i = 0; i < number_of_chunks; i++)
    {
        free(data[i]);
    }

    printf("File transfer Successful\n");

    free(number_of_chunks_str);
    free(file_found_str);
    free(file_size_str);

    close(sock);
    fclose(fp);

    return 0;
}