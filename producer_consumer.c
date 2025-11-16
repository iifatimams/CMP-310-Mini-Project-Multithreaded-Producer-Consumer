// CMP-310 Mini Project – Producer–Consumer (Fall 2025)
// Multithreaded bounded buffer using pthreads, semaphores, and a mutex.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define POISON_PILL -1

// Each buffer slot stores a value and its enqueue time
typedef struct {
    int value;
    struct timeval enqueue_time;
} buffer_slot_t;

// Shared structure holding buffer state and synchronization primitives
typedef struct {
    buffer_slot_t *buffer;   // Circular buffer storage
    int size;                // Capacity of the buffer
    int in;                  // Write index
    int out;                 // Read index

    sem_t empty_slots;       // Tracks remaining empty positions
    sem_t full_slots;        // Tracks filled positions available for consumption
    pthread_mutex_t mutex;   // Ensures mutual exclusion on buffer access

    int producers;
    int consumers;
    int items_per_producer;

    int total_items;         // Expected number of real items
    int consumed_items;      // Number of consumed real items

    double total_latency_sec; // Sum of per-item latency in seconds

    pthread_mutex_t stats_mutex; // Protects consumed_items and total_latency_sec

    struct timeval start_time; // Time when program started work
    struct timeval end_time;   // Time when all consumers finished
} shared_t;

typedef struct {
    int id;          // Thread ID for logging
    shared_t *shared;
} thread_arg_t;


// Returns current wall-clock time (helper)
static inline void now(struct timeval *tv) {
    gettimeofday(tv, NULL);
}

// Producer: generates items and inserts them into the circular buffer
void *producer_thread(void *arg) {
    thread_arg_t *t = (thread_arg_t *)arg;
    shared_t *s = t->shared;

    for (int i = 0; i < s->items_per_producer; i++) {
        int item = rand() % 1000;

        sem_wait(&s->empty_slots);          // Blocks if buffer is full
        pthread_mutex_lock(&s->mutex);      // Exclusive access to buffer

        struct timeval enq_time;
        now(&enq_time);

        s->buffer[s->in].value = item;
        s->buffer[s->in].enqueue_time = enq_time;

        printf("[Producer-%d] Produced %d at index %d\n", t->id, item, s->in);

        s->in = (s->in + 1) % s->size;

        pthread_mutex_unlock(&s->mutex);
        sem_post(&s->full_slots);           // Signals a new filled slot
    }

    printf("[Producer-%d] Finished producing.\n", t->id);
    return NULL;
}


// Consumer: removes items from the circular buffer and processes them
void *consumer_thread(void *arg) {
    thread_arg_t *t = (thread_arg_t *)arg;
    shared_t *s = t->shared;

    while (1) {
        sem_wait(&s->full_slots);          // Blocks if buffer is empty
        pthread_mutex_lock(&s->mutex);     // Exclusive access to buffer

        buffer_slot_t slot = s->buffer[s->out];

        printf("[Consumer-%d] Dequeued %d from index %d\n", t->id,
               slot.value, s->out);

        s->out = (s->out + 1) % s->size;

        pthread_mutex_unlock(&s->mutex);
        sem_post(&s->empty_slots);         // Signals a newly freed slot

        if (slot.value == POISON_PILL) {   // Poison pill triggers termination
            printf("[Consumer-%d] Exiting.\n", t->id);
            break;
        }

        struct timeval deq_time;
        now(&deq_time);

        // Compute latency for this item (in seconds)
        double latency =
            (double)(deq_time.tv_sec - slot.enqueue_time.tv_sec) +
            (double)(deq_time.tv_usec - slot.enqueue_time.tv_usec) / 1e6;

        pthread_mutex_lock(&s->stats_mutex);
        s->consumed_items++;
        s->total_latency_sec += latency;
        pthread_mutex_unlock(&s->stats_mutex);

        printf("[Consumer-%d] Consumed real item %d (latency ≈ %.6f s)\n",
               t->id, slot.value, latency);
    }

    return NULL;
}


// Validates and converts command-line arguments
int parse_int(char *arg, const char *name) {
    int x = atoi(arg);
    if (x <= 0) {
        fprintf(stderr, "Invalid %s: %s (must be > 0)\n", name, arg);
        exit(1);
    }
    return x;
}


// Computes difference in seconds between two timevals
double elapsed_sec(struct timeval start, struct timeval end) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_usec - start.tv_usec) / 1e6;
}


int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        printf("Usage: %s <producers> <consumers> <buffer_size> [items_per_producer]\n",
               argv[0]);
        return 1;
    }

    shared_t s;

    s.producers = parse_int(argv[1], "num_producers");
    s.consumers = parse_int(argv[2], "num_consumers");
    s.size = parse_int(argv[3], "buffer_size");
    s.items_per_producer = (argc == 5) ? parse_int(argv[4], "items_per_producer") : 20;

    s.total_items = s.producers * s.items_per_producer;
    s.consumed_items = 0;
    s.total_latency_sec = 0.0;

    s.buffer = (buffer_slot_t *)malloc(sizeof(buffer_slot_t) * s.size);
    if (!s.buffer) {
        perror("malloc");
        return 1;
    }

    s.in = 0;
    s.out = 0;

    pthread_mutex_init(&s.mutex, NULL);
    pthread_mutex_init(&s.stats_mutex, NULL);
    sem_init(&s.empty_slots, 0, s.size);
    sem_init(&s.full_slots, 0, 0);

    pthread_t prod_threads[s.producers];
    pthread_t cons_threads[s.consumers];
    thread_arg_t prod_args[s.producers];
    thread_arg_t cons_args[s.consumers];

    srand((unsigned int)time(NULL));

    // Record start time just before threads begin working
    now(&s.start_time);

    for (int i = 0; i < s.producers; i++) {
        prod_args[i].id = i + 1;
        prod_args[i].shared = &s;
        pthread_create(&prod_threads[i], NULL, producer_thread, &prod_args[i]);
    }

    for (int i = 0; i < s.consumers; i++) {
        cons_args[i].id = i + 1;
        cons_args[i].shared = &s;
        pthread_create(&cons_threads[i], NULL, consumer_thread, &cons_args[i]);
    }

    for (int i = 0; i < s.producers; i++)
        pthread_join(prod_threads[i], NULL);

    // Insert poison pills to signal consumer termination
    for (int i = 0; i < s.consumers; i++) {
        sem_wait(&s.empty_slots);
        pthread_mutex_lock(&s.mutex);

        struct timeval poison_time;
        now(&poison_time);

        s.buffer[s.in].value = POISON_PILL;
        s.buffer[s.in].enqueue_time = poison_time; // not used for stats
        printf("[Main] Inserted POISON_PILL at index %d\n", s.in);
        s.in = (s.in + 1) % s.size;

        pthread_mutex_unlock(&s.mutex);
        sem_post(&s.full_slots);
    }

    for (int i = 0; i < s.consumers; i++)
        pthread_join(cons_threads[i], NULL);

    // Record end time after all consumers finish
    now(&s.end_time);

    double runtime_sec = elapsed_sec(s.start_time, s.end_time);
    double avg_latency_sec =
        (s.consumed_items > 0) ? (s.total_latency_sec / s.consumed_items) : 0.0;
    double throughput =
        (runtime_sec > 0.0) ? ((double)s.consumed_items / runtime_sec) : 0.0;

    printf("\n=== Summary ===\n");
    printf("Producers: %d, Consumers: %d, Buffer size: %d\n",
           s.producers, s.consumers, s.size);
    printf("Items per producer: %d\n", s.items_per_producer);
    printf("Expected real items: %d\n", s.total_items);
    printf("Consumed real items: %d\n", s.consumed_items);
    printf("Total runtime: %.6f seconds\n", runtime_sec);
    printf("Average latency per item: %.6f seconds\n", avg_latency_sec);
    printf("Throughput: %.6f items/second\n", throughput);

    return 0;
}
