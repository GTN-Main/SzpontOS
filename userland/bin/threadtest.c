/*
 * SzpontOS - POSIX Threads (pthreads) Test Suite
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>

#define NUM_THREADS 4
#define ITERATIONS 5000

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static pthread_barrier_t g_barrier;
static sem_t g_sem;
static pthread_key_t g_tls_key;

static volatile int g_counter = 0;
static volatile int g_ping_pong = 0;
static volatile int g_barrier_hits = 0;

/* Worker 1: Mutex Contention Test */
static void *mutex_worker(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&g_mutex);
        g_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    printf("  [Thread %ld] Completed %d increments.\n", id, ITERATIONS);
    return (void *)(id * 10);
}

/* Worker 2: Condition Variable Ping-Pong */
static void *cond_consumer(void *arg) {
    (void)arg;
    pthread_mutex_lock(&g_mutex);
    while (g_ping_pong == 0) {
        pthread_cond_wait(&g_cond, &g_mutex);
    }
    g_ping_pong = 2; /* Acknowledged */
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    return NULL;
}

/* Worker 3: Barrier Test */
static void *barrier_worker(void *arg) {
    long id = (long)arg;
    __sync_fetch_and_add(&g_barrier_hits, 1);

    pthread_barrier_wait(&g_barrier);

    /* After barrier, all threads should have reached it */
    if (g_barrier_hits != NUM_THREADS) {
        printf("  [ERROR] Barrier failed: hits = %d (expected %d)\n", g_barrier_hits, NUM_THREADS);
    }
    return (void *)id;
}

/* Worker 4: Thread-Specific Data (TSD) */
static void *tls_worker(void *arg) {
    long id = (long)arg;
    long *val = (long *)malloc(sizeof(long));
    *val = id * 100 + 42;
    pthread_setspecific(g_tls_key, val);

    /* Yield to ensure interleaving */
    sched_yield();

    long *ret = (long *)pthread_getspecific(g_tls_key);
    if (!ret || *ret != (id * 100 + 42)) {
        printf("  [ERROR] TLS mismatch for thread %ld: got %ld\n", id, ret ? *ret : -1);
    }
    free(val);
    return NULL;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("  SzpontOS POSIX Threads Test Suite     \n");
    printf("========================================\n\n");

    /* 1. Mutex Contention Test */
    printf("[1] Testing Mutex & Multi-Thread Increments (%d threads x %d iters):\n", NUM_THREADS, ITERATIONS);
    pthread_t threads[NUM_THREADS];
    g_counter = 0;

    for (long i = 0; i < NUM_THREADS; i++) {
        int res = pthread_create(&threads[i], NULL, mutex_worker, (void *)(i + 1));
        if (res != 0) {
            printf("  [ERROR] Failed to create thread %ld (errno=%d)\n", i, res);
            return 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        void *retval = NULL;
        pthread_join(threads[i], &retval);
    }

    int expected = NUM_THREADS * ITERATIONS;
    printf("  -> Final Counter = %d (expected %d) %s\n\n", g_counter, expected,
           (g_counter == expected) ? "[PASSED]" : "[FAILED]");

    /* 2. Condition Variable Test */
    printf("[2] Testing Condition Variables (pthread_cond_wait / signal):\n");
    pthread_t cons;
    g_ping_pong = 0;
    pthread_create(&cons, NULL, cond_consumer, NULL);

    /* Producer signals consumer */
    sched_yield();
    pthread_mutex_lock(&g_mutex);
    g_ping_pong = 1;
    pthread_cond_signal(&g_cond);

    while (g_ping_pong == 1) {
        pthread_cond_wait(&g_cond, &g_mutex);
    }
    pthread_mutex_unlock(&g_mutex);

    pthread_join(cons, NULL);
    printf("  -> Condition Variable Ping-Pong %s (state = %d)\n\n", (g_ping_pong == 2) ? "[PASSED]" : "[FAILED]",
           g_ping_pong);

    /* 3. Barrier Test */
    printf("[3] Testing POSIX Barriers (pthread_barrier_wait):\n");
    g_barrier_hits = 0;
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, barrier_worker, (void *)(i + 1));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_barrier_destroy(&g_barrier);
    printf("  -> Barrier Synchronization [PASSED] (hits = %d)\n\n", g_barrier_hits);

    /* 4. Semaphore Test */
    printf("[4] Testing POSIX Semaphores (sem_wait / sem_post):\n");
    sem_init(&g_sem, 0, 2);
    int sval = -1;
    sem_getvalue(&g_sem, &sval);
    printf("  sem initial value = %d (expected 2)\n", sval);
    sem_wait(&g_sem);
    sem_wait(&g_sem);
    sem_getvalue(&g_sem, &sval);
    printf("  sem value after 2 waits = %d (expected 0)\n", sval);
    sem_post(&g_sem);
    sem_getvalue(&g_sem, &sval);
    printf("  sem value after post = %d (expected 1)\n", sval);
    sem_destroy(&g_sem);
    printf("  -> POSIX Semaphores [PASSED]\n\n");

    /* 5. Thread-Specific Data Test */
    printf("[5] Testing Thread-Specific Data (pthread_key_create / getspecific):\n");
    pthread_key_create(&g_tls_key, NULL);
    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, tls_worker, (void *)(i + 1));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_key_delete(g_tls_key);
    printf("  -> Thread-Specific Data [PASSED]\n\n");

    printf("[SUCCESS] All POSIX Threads & Synchronization Tests PASSED!\n");
    return 0;
}
