/*
 * shmtest - verify System V Shared Memory & Zero-Copy IPC in SzpontOS
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define TEST_KEY 0x1337BEEF
#define TEST_SIZE 4096

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[SHMTEST] Starting System V Shared Memory & Zero-Copy IPC Test...\n");

    /* 1. Allocate Shared Memory Segment */
    int shmid = shmget(TEST_KEY, TEST_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmtest: shmget failed");
        return 1;
    }
    printf("  [OK] shmget allocated segment SHMID: %d (Size: %d bytes)\n", shmid, TEST_SIZE);

    /* 2. Attach Segment in Parent */
    char *parent_mem = (char *)shmat(shmid, NULL, 0);
    if (parent_mem == (void *)-1) {
        perror("shmtest: parent shmat failed");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }
    printf("  [OK] Parent attached segment at virtual address: %p\n", parent_mem);

    /* Initialize memory */
    strcpy(parent_mem, "INITIAL_PARENT_PAYLOAD");

    /* 3. Fork Child Process */
    pid_t pid = fork();
    if (pid < 0) {
        perror("shmtest: fork failed");
        shmdt(parent_mem);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    if (pid == 0) {
        /* Child Process */
        printf("  [CHILD] Child started (PID %d). Attaching to SHMID %d...\n", getpid(), shmid);
        char *child_mem = (char *)shmat(shmid, NULL, 0);
        if (child_mem == (void *)-1) {
            perror("  [CHILD] shmat failed");
            exit(1);
        }
        printf("  [CHILD] Attached at %p. Read parent data: '%s'\n", child_mem, child_mem);

        if (strcmp(child_mem, "INITIAL_PARENT_PAYLOAD") != 0) {
            fprintf(stderr, "  [CHILD] ERROR: Payload mismatch!\n");
            exit(1);
        }

        /* Write response back via Zero-Copy shared physical frame */
        strcpy(child_mem, "CHILD_ZERO_COPY_ACK_100_PERCENT");
        printf("  [CHILD] Updated shared memory payload. Detaching and exiting.\n");
        shmdt(child_mem);
        exit(0);
    }

    /* Parent waits for child */
    int status = 0;
    waitpid(pid, &status, 0);

    printf("  [PARENT] Child exited with status %d. Checking shared memory contents...\n", status);
    printf("  [PARENT] Read after child update: '%s'\n", parent_mem);

    if (strcmp(parent_mem, "CHILD_ZERO_COPY_ACK_100_PERCENT") != 0) {
        fprintf(stderr, "shmtest: ERROR: Shared memory was not updated across processes!\n");
        shmdt(parent_mem);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    printf("  [OK] Zero-Copy data verified across processes!\n");

    /* 4. Detach and Cleanup */
    if (shmdt(parent_mem) < 0) {
        perror("shmtest: shmdt failed");
        return 1;
    }
    printf("  [OK] Parent detached from segment.\n");

    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        perror("shmtest: shmctl(IPC_RMID) failed");
        return 1;
    }
    printf("  [OK] Shared memory segment %d removed.\n", shmid);

    printf("[SHMTEST] ALL SHARED MEMORY TESTS PASSED SUCCESSFULLY! (100%% OK)\n");
    return 0;
}
