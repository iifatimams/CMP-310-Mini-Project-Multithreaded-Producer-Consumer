// producer_consumer.c
// -----------------------------------------------------------------------------

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>      // O_CREAT, O_EXCL
#include <sys/stat.h>   // mode constants
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

// Named semaphore identifiers (must be unique system-wide)
#define EMPTY_SEM_NAME "/pc_empty_sem_example"
#define FULL_SEM_NAME  "/pc_full_sem_example"

// Priority values
#define PRIORITY_NORMAL 0
#define PRIORITY_URGENT 1

// Percentage of urgent items (at least 25% as required)
#define URGENT_PERCENT 25

// Special poison-pill marker
#define POISON_VALUE   -1

// Fixed number of items produced by each producer
int ITEMS_PER_PRODUCER = 20;

// Structure that represents a single item in the buffer
typedef struct {
    int value;                  // The integer payload
    int priority;               // 0 = normal, 1 = urgent
    int is_poison;              // 1 if this is a poison pill, 0 otherwise
    struct timespec enq_time;   // Timestamp when producer enqueues the item
} buffer_item_t;

// Global configuration parameters
int NUM_PRODUCERS = 0;
int NUM_CONSUMERS = 0;
int BUFFER_SIZE   = 0;

// Global shared queues for priorities (each is circular)
buffer_item_t *urgent_queue  = NULL;
buffer_item_t *normal_queue  = NULL;

int urgent_head  = 0, urgent_tail  = 0, urgent_count  = 0;
int normal_head  = 0, normal_tail  = 0, normal_count  = 0;

// Synchronization primitives
sem_t *sem_empty = NULL;    // Counts free slots in the whole buffer
sem_t *sem_full  = NULL;    // Counts filled slots (any priority)
pthread_mutex_t buffer_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t metrics_mutex  = PTHREAD_MUTEX_INITIALIZER;

// Metrics
uint64_t total_latency_ns = 0;   // Sum of per-item latency in nanoseconds
uint64_t total_items      = 0;   // Number of real (non-poison) items consumed

struct timespec start_time_global;
struct timespec end_time_global;

// Structure passed to each thread
typedef struct {
    int thread_id;   // 1-based index
} thread_arg_t;

// Utility function: converts timespec difference to nanoseconds
static uint64_t diff_timespec_ns(const struct timespec *start,
                                 const struct timespec *end)
{
    int64_t sec  = (int64_t)end->tv_sec  - (int64_t)start->tv_sec;
    int64_t nsec = (int64_t)end->tv_nsec - (int64_t)start->tv_nsec;

    if (nsec < 0) {
        sec--;
        nsec += 1000000000LL;
    }
    return (uint64_t)sec * 1000000000ULL + (uint64_t)nsec;
}

// Utility: prints error from errno and exits
static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function that enqueues an item into the appropriate priority queue
static void enqueue_item(const buffer_item_t *item)
{
    // The caller already holds buffer_mutex and has decremented sem_empty.
    if (item->priority == PRIORITY_URGENT && !item->is_poison) {
        // Inserts into urgent queue
        urgent_queue[urgent_tail] = *item;
        urgent_tail = (urgent_tail + 1) % BUFFER_SIZE;
        urgent_count++;
    } else {
        // Inserts into normal queue (also used for poison pills)
        normal_queue[normal_tail] = *item;
        normal_tail = (normal_tail + 1) % BUFFER_SIZE;
        normal_count++;
    }
}

// Function that dequeues an item, giving priority to urgent items
static buffer_item_t dequeue_item(void)
{
    buffer_item_t item;

    // The caller already holds buffer_mutex and has decremented sem_full.
    if (urgent_count > 0) {
        // Consumer takes from urgent queue first
        item = urgent_queue[urgent_head];
        urgent_head = (urgent_head + 1) % BUFFER_SIZE;
        urgent_count--;
    } else {
        // If urgent queue is empty, it takes from normal queue
        item = normal_queue[normal_head];
        normal_head = (normal_head + 1) % BUFFER_SIZE;
        normal_count--;
    }
    return item;
}

// Producer thread function
void *producer_thread(void *arg)
{
    thread_arg_t *targ = (thread_arg_t *)arg;
    int id = targ->thread_id;

    // Each producer uses its own RNG seed
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(id * 7919);

    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        buffer_item_t item;
        item.is_poison = 0;

        // Generates a random value
        item.value = rand_r(&seed) % 1000;

        // Assigns a priority: URGENT_PERCENT chance to be urgent
        int rnd = rand_r(&seed) % 100;
        if (rnd < URGENT_PERCENT) {
            item.priority = PRIORITY_URGENT;
        } else {
            item.priority = PRIORITY_NORMAL;
        }

        // Waits for an empty slot in the buffer
        if (sem_wait(sem_empty) == -1) {
            die("sem_wait(sem_empty) in producer");
        }

        // Records enqueue time as soon as an empty slot is reserved
        if (clock_gettime(CLOCK_MONOTONIC, &item.enq_time) == -1) {
            die("clock_gettime in producer");
        }

        // Inserts the item into the appropriate queue
        if (pthread_mutex_lock(&buffer_mutex) != 0) {
            die("pthread_mutex_lock buffer_mutex in producer");
        }

        enqueue_item(&item);

        // Prints producer message
        printf("[Producer-%d] Produced item: %d (priority: %s)\n",
               id,
               item.value,
               item.priority == PRIORITY_URGENT ? "urgent" : "normal");

        if (pthread_mutex_unlock(&buffer_mutex) != 0) {
            die("pthread_mutex_unlock buffer_mutex in producer");
        }

        // Signals that there is at least one full slot
        if (sem_post(sem_full) == -1) {
            die("sem_post(sem_full) in producer");
        }
    }

    // Producer reports completion
    printf("[Producer-%d] Finished producing %d items.\n", id, ITEMS_PER_PRODUCER);
    return NULL;
}

// Consumer thread function
void *consumer_thread(void *arg)
{
    thread_arg_t *targ = (thread_arg_t *)arg;
    int id = targ->thread_id;

    while (1) {
        // Waits for at least one filled slot
        if (sem_wait(sem_full) == -1) {
            die("sem_wait(sem_full) in consumer");
        }

        // Removes an item (urgent first) under mutual exclusion
        if (pthread_mutex_lock(&buffer_mutex) != 0) {
            die("pthread_mutex_lock buffer_mutex in consumer");
        }

        buffer_item_t item = dequeue_item();

        if (pthread_mutex_unlock(&buffer_mutex) != 0) {
            die("pthread_mutex_unlock buffer_mutex in consumer");
        }

        // Signals that a slot became empty
        if (sem_post(sem_empty) == -1) {
            die("sem_post(sem_empty) in consumer");
        }

        // Checks for poison pill
        if (item.is_poison) {
            printf("[Consumer-%d] Finished consuming.\n", id);
            break;
        }

        // Records dequeue time and updates latency metrics
        struct timespec deq_time;
        if (clock_gettime(CLOCK_MONOTONIC, &deq_time) == -1) {
            die("clock_gettime in consumer");
        }

        uint64_t latency_ns = diff_timespec_ns(&item.enq_time, &deq_time);

        if (pthread_mutex_lock(&metrics_mutex) != 0) {
            die("pthread_mutex_lock metrics_mutex in consumer");
        }

        total_latency_ns += latency_ns;
        total_items++;

        if (pthread_mutex_unlock(&metrics_mutex) != 0) {
            die("pthread_mutex_unlock metrics_mutex in consumer");
        }

        // Processes the consumed item (here it simply prints it)
        printf("[Consumer-%d] Consumed item: %d (priority: %s)\n",
               id,
               item.value,
               item.priority == PRIORITY_URGENT ? "urgent" : "normal");
    }
    return NULL;
}

// Inserts poison pills after all producers are done
static void insert_poison_pills(void)
{
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        buffer_item_t poison;
        poison.value     = POISON_VALUE;
        poison.priority  = PRIORITY_NORMAL; // Poison uses normal priority
        poison.is_poison = 1;
        // Timestamp is irrelevant for poison; it is left uninitialized

        if (sem_wait(sem_empty) == -1) {
            die("sem_wait(sem_empty) for poison pill");
        }

        if (pthread_mutex_lock(&buffer_mutex) != 0) {
            die("pthread_mutex_lock buffer_mutex for poison pill");
        }

        enqueue_item(&poison);

        if (pthread_mutex_unlock(&buffer_mutex) != 0) {
            die("pthread_mutex_unlock buffer_mutex for poison pill");
        }

        if (sem_post(sem_full) == -1) {
            die("sem_post(sem_full) for poison pill");
        }
    }
}

// Prints throughput and latency metrics
static void print_metrics(void)
{
    uint64_t local_total_items;
    uint64_t local_total_latency_ns;

    if (pthread_mutex_lock(&metrics_mutex) != 0) {
        die("pthread_mutex_lock metrics_mutex in print_metrics");
    }

    local_total_items = total_items;
    local_total_latency_ns = total_latency_ns;

    if (pthread_mutex_unlock(&metrics_mutex) != 0) {
        die("pthread_mutex_unlock metrics_mutex in print_metrics");
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_time_global) == -1) {
        die("clock_gettime end_time_global");
    }

    uint64_t runtime_ns = diff_timespec_ns(&start_time_global, &end_time_global);
    double runtime_sec  = (double)runtime_ns / 1e9;

    double avg_latency_us = 0.0;
    if (local_total_items > 0) {
        avg_latency_us = ((double)local_total_latency_ns /
                          (double)local_total_items) / 1000.0;
    }

    double throughput = 0.0;
    if (runtime_sec > 0.0) {
        throughput = (double)local_total_items / runtime_sec;
    }

    // EXACT format you requested:
    printf("\n========== METRICS ==========\n");
    printf("Total items consumed: %llu\n",
           (unsigned long long)local_total_items);
    printf("Average latency: %.2f microseconds\n", avg_latency_us);
    printf("Throughput: %.2f items/second\n", throughput);
    printf("Runtime: %.3f seconds\n", runtime_sec);
}

// Parses and validates command-line arguments
static void parse_args(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <num_producers> <num_consumers> <buffer_size>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    NUM_PRODUCERS = atoi(argv[1]);
    NUM_CONSUMERS = atoi(argv[2]);
    BUFFER_SIZE   = atoi(argv[3]);

    if (NUM_PRODUCERS <= 0 || NUM_CONSUMERS <= 0 || BUFFER_SIZE <= 0) {
        fprintf(stderr, "All arguments must be positive integers.\n");
        exit(EXIT_FAILURE);
    }

    if (BUFFER_SIZE < 2) {
        fprintf(stderr, "Buffer size should be at least 2 for a meaningful test.\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);

    // Allocates queues
    urgent_queue = calloc(BUFFER_SIZE, sizeof(buffer_item_t));
    normal_queue = calloc(BUFFER_SIZE, sizeof(buffer_item_t));
    if (!urgent_queue || !normal_queue) {
        die("calloc for queues");
    }

    // Ensures old named semaphores are removed
    sem_unlink(EMPTY_SEM_NAME);
    sem_unlink(FULL_SEM_NAME);

    // Creates named semaphores
    sem_empty = sem_open(EMPTY_SEM_NAME, O_CREAT | O_EXCL, 0644, BUFFER_SIZE);
    if (sem_empty == SEM_FAILED) {
        die("sem_open sem_empty");
    }

    sem_full = sem_open(FULL_SEM_NAME, O_CREAT | O_EXCL, 0644, 0);
    if (sem_full == SEM_FAILED) {
        die("sem_open sem_full");
    }

    // Main thread records start time just before creating worker threads
    if (clock_gettime(CLOCK_MONOTONIC, &start_time_global) == -1) {
        die("clock_gettime start_time_global");
    }

    pthread_t *producers  = calloc(NUM_PRODUCERS, sizeof(pthread_t));
    pthread_t *consumers  = calloc(NUM_CONSUMERS, sizeof(pthread_t));
    thread_arg_t *prod_args = calloc(NUM_PRODUCERS, sizeof(thread_arg_t));
    thread_arg_t *cons_args = calloc(NUM_CONSUMERS, sizeof(thread_arg_t));

    if (!producers || !consumers || !prod_args || !cons_args) {
        die("calloc for thread arrays");
    }

    // Creates consumer threads first (they can start waiting immediately)
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        cons_args[i].thread_id = i + 1;
        int rc = pthread_create(&consumers[i], NULL, consumer_thread, &cons_args[i]);
        if (rc != 0) {
            fprintf(stderr, "Failed to create consumer thread %d: %s\n",
                    i + 1, strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    // Creates producer threads
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        prod_args[i].thread_id = i + 1;
        int rc = pthread_create(&producers[i], NULL, producer_thread, &prod_args[i]);
        if (rc != 0) {
            fprintf(stderr, "Failed to create producer thread %d: %s\n",
                    i + 1, strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    // Waits for all producers to finish
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_join(producers[i], NULL);
    }

    // Inserts one poison pill for each consumer so they can exit gracefully
    insert_poison_pills();

    // Waits for all consumers to finish
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_join(consumers[i], NULL);
    }

    // Prints collected metrics
    print_metrics();

    // Cleans up resources
    free(urgent_queue);
    free(normal_queue);
    free(producers);
    free(consumers);
    free(prod_args);
    free(cons_args);

    if (sem_close(sem_empty) == -1) {
        die("sem_close sem_empty");
    }
    if (sem_close(sem_full) == -1) {
        die("sem_close sem_full");
    }

    // The semaphores are unlinked here so that they do not persist after program exit
    sem_unlink(EMPTY_SEM_NAME);
    sem_unlink(FULL_SEM_NAME);

    pthread_mutex_destroy(&buffer_mutex);
    pthread_mutex_destroy(&metrics_mutex);

    return 0;
}
