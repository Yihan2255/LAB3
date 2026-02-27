#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parsePGM.h"

#define BUFF_SIZE 1024


typedef struct {
    char *path;
    int offset;
    int bytesToRead;
    unsigned int *global_hist;
    pthread_mutex_t *mutex;
    int maxval;
} ThreadInfo;


void* compute_partial_histogram(void *arg) {
    ThreadInfo *info = (ThreadInfo *)arg;
    
    // Initialize Local Histogram
    unsigned int *local_hist = calloc(info->maxval + 1, sizeof(unsigned int));
    if (!local_hist) pthread_exit(NULL);

    // Open File Independently
    int fd = open(info->path, O_RDONLY);
    if (fd < 0) {
        free(local_hist);
        pthread_exit(NULL);
    }

    // Move to Assigned Offset
    if (lseek(fd, info->offset, SEEK_SET) == -1) {
        close(fd);
        free(local_hist);
        pthread_exit(NULL);
    }

    // Process in Chunks
    unsigned char buffer[BUFF_SIZE];
    int total_read = 0;
    
    while (total_read < info->bytesToRead) {
        // Determine bytes to read
        int remaining = info->bytesToRead - total_read;
        int to_read = (remaining > BUFF_SIZE) ? BUFF_SIZE : remaining;

        int r = read(fd, buffer, to_read);
        if (r <= 0) break; // EoF or error

        // Update LOCAL histogram
        for (int i = 0; i < r; i++) {
            local_hist[buffer[i]]++;
        }
        total_read += r;
    }

    close(fd);

    // Merge Results
    pthread_mutex_lock(info->mutex);
    for (int i = 0; i <= info->maxval; i++) {
        info->global_hist[i] += local_hist[i];
    }
    pthread_mutex_unlock(info->mutex);

    free(local_hist);
    pthread_exit(NULL);
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <image_path> <output_path> <n_threads>\n", argv[0]);
        _exit(1);
    }

    int nThreads = atoi(argv[3]);
    if (nThreads < 1) nThreads = 1;

    // Parse Header
    int width, height, maxval;
    int headerSize = parse_pgm_header(argv[1], &width, &height, &maxval);
    
    if (headerSize < 0) {
        printf("Error: Invalid PGM header.\n");
        _exit(1);
    }
    
    if (maxval > 255) {
        printf("Error: Only 8-bit images (maxval <= 255) are supported.\n");
        _exit(1);
    }

    int totalDataBytes = width * height;

    // Initialize Shared
    unsigned int *global_histogram = calloc(maxval + 1, sizeof(unsigned int));
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_t *threads = malloc(nThreads * sizeof(pthread_t));
    ThreadInfo *tArgs = malloc(nThreads * sizeof(ThreadInfo));

    // Create threads
    int bytesPerThread = totalDataBytes / nThreads;
    int remainder = totalDataBytes % nThreads;
    int currentOffset = headerSize;

    for (int i = 0; i < nThreads; i++) {
        tArgs[i].path = argv[1];
        tArgs[i].offset = currentOffset;
        tArgs[i].global_hist = global_histogram;
        tArgs[i].mutex = &mutex;
        tArgs[i].maxval = maxval;

        if (i == nThreads - 1) {
            tArgs[i].bytesToRead = bytesPerThread + remainder;
        } else {
            tArgs[i].bytesToRead = bytesPerThread;
        }

        currentOffset += tArgs[i].bytesToRead;

        if (pthread_create(&threads[i], NULL, compute_partial_histogram, &tArgs[i]) != 0) {
            perror("Failed to create thread");
            _exit(1);
        }
    }

    // Wait Completion
    for (int i = 0; i < nThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Output
    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int i = 0; i <= maxval; i++) {
        char s[64];
        sprintf(s, "%d,%u\n", i, global_histogram[i]);
        write(fd_out, s, strlen(s));
    }
    
    close(fd_out);
    
    // Cleanup
    free(global_histogram);
    free(threads);
    free(tArgs);
    pthread_mutex_destroy(&mutex);

    return 0;
}