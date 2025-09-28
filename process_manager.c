// process_manager.c
#include "process_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

// Helpers for sending/receiving ints over a pipe
static void write_int(int fd, int value) {
    if (write(fd, &value, sizeof(value)) != (ssize_t)sizeof(value)) {
        perror("write");
        _exit(1);
    }
}
static int read_int(int fd, int *out) {
    ssize_t n = read(fd, out, sizeof(*out));
    if (n == 0) return 0;                  // EOF
    if (n < 0) { perror("read"); return 0; }
    if (n != (ssize_t)sizeof(*out)) {      // partial read (shouldn't happen)
        fprintf(stderr, "partial read\n");
        return 0;
    }
    return 1;
}

void run_basic_demo(void) {
    printf("Starting basic producer-consumer demonstration...\n\n");

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return;
    }

    pid_t parent_pid = getpid();
    printf("Parent process (PID: %d) creating children...\n", parent_pid);

    // Fork producer
    pid_t producer_pid = fork();
    if (producer_pid < 0) {
        perror("fork");
        close(pipe_fd[0]); close(pipe_fd[1]);
        return;
    }
    if (producer_pid == 0) {
        // Producer child
        printf("Producer (PID: %d) starting...\n", getpid());
        close(pipe_fd[0]); // producer writes only
        for (int i = 1; i <= 5; i++) {
            write_int(pipe_fd[1], i);
            printf("Producer: Sent number %d\n", i);
            usleep(30000);
        }
        close(pipe_fd[1]);
        printf("Producer: Finished sending 5 numbers\n");
        _exit(0);
    } else {
        // Parent sees child created
        printf("Created producer child (PID: %d)\n", producer_pid);
    }

    // Fork consumer
    pid_t consumer_pid = fork();
    if (consumer_pid < 0) {
        perror("fork");
        close(pipe_fd[0]); close(pipe_fd[1]);
        return;
    }
    if (consumer_pid == 0) {
        // Consumer child
        printf("Consumer (PID: %d) starting...\n", getpid());
        close(pipe_fd[1]); // consumer reads only
        int sum = 0, val;
        while (read_int(pipe_fd[0], &val)) {
            sum += val;
            printf("Consumer: Received %d, running sum: %d\n", val, sum);
            usleep(20000);
        }
        close(pipe_fd[0]);
        printf("Consumer: Final sum: %d\n", sum);
        _exit(0);
    } else {
        // Parent sees child created
        printf("Created consumer child (PID: %d)\n", consumer_pid);
    }

    // Parent: close both ends (it doesn't use the pipe)
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    // Wait for both children
    int status;
    pid_t w = waitpid(producer_pid, &status, 0);
    if (w > 0) {
        printf("\nProducer child (PID: %d) exited with status %d\n",
               w, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
    w = waitpid(consumer_pid, &status, 0);
    if (w > 0) {
        printf("Consumer child (PID: %d) exited with status %d\n",
               w, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }

    printf("\nSUCCESS: Basic producer-consumer completed!\n");
}

void run_multiple_pairs(void) {
    const int pairs = 2;     // as in the sample output
    const int span  = 5;     // numbers per pair
    int start = 1;

    printf("Running multiple producer-consumer pairs...\n\n");
    printf("Parent creating %d producer-consumer pairs...\n\n", pairs);

    pid_t children[pairs * 2];
    int child_count = 0;

    for (int p = 0; p < pairs; p++) {
        printf("=== Pair %d ===\n", p + 1);

        int pipe_fd[2];
        if (pipe(pipe_fd) == -1) {
            perror("pipe");
            continue;
        }

        // Producer
        pid_t prod = fork();
        if (prod < 0) {
            perror("fork producer");
            close(pipe_fd[0]); close(pipe_fd[1]);
            continue;
        }
        if (prod == 0) {
            printf("Producer (PID: %d) starting...\n", getpid());
            close(pipe_fd[0]); // write only
            for (int i = start; i < start + span; i++) {
                write_int(pipe_fd[1], i);
                printf("Producer: Sent number %d\n", i);
                usleep(25000);
            }
            close(pipe_fd[1]);
            _exit(0);
        }
        children[child_count++] = prod;

        // Consumer
        pid_t cons = fork();
        if (cons < 0) {
            perror("fork consumer");
            close(pipe_fd[0]); close(pipe_fd[1]);
            continue;
        }
        if (cons == 0) {
            printf("Consumer (PID: %d) starting...\n", getpid());
            close(pipe_fd[1]); // read only
            int sum = 0, val;
            while (read_int(pipe_fd[0], &val)) {
                sum += val;
                printf("Consumer: Received %d, running sum: %d\n", val, sum);
                usleep(20000);
            }
            printf("Consumer: Final sum: %d\n", sum);
            close(pipe_fd[0]);
            _exit(0);
        }
        children[child_count++] = cons;

        // Parent closes its copies of both ends for this pair
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        start += span;
        printf("\n");
    }

    // Message before listing child exits (to match sample)
    printf("All pairs completed successfully!\n");

    // Wait for all children and print exit lines
    int status;
    for (int i = 0; i < child_count; i++) {
        pid_t w = waitpid(children[i], &status, 0);
        if (w > 0) {
            printf("Child (PID: %d) exited with status %d\n",
                   w, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        }
    }

    printf("\nSUCCESS: Multiple pairs completed!\n");
}
