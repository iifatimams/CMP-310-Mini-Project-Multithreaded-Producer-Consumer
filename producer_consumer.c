// CMP-310 Mini Project (Fall 2025)
// Producer–Consumer implementation using pthreads, semaphores, and a circular buffer.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define POISON_PILL -1

// Shared structure holding the circular buffer and all synchronization objects
typedef struct {
    int *buffer;
    int size;
    int in;
    int out;

    sem_t empty_slots;          // Counts available empty positions in the buffer
    sem_t full_slots;           // Counts available filled positions
    pthread_mutex_t mutex;      // Protects buffer access

    int producers;
    int consumers;
    int items_per_producer;

    int total_items;
    int consumed_count;
    pthread_mutex_t count_mutex;

} shared_t;

// Thread argument containing the ID and reference to shared memory
typedef struct {
    int id;
    shared_t *shared;
} thread_arg_t;


// Producer thread: generates items and inserts them into the buffer
void *producer(void *arg) {
    thread_arg_t *t = arg;
    shared_t *s = t->shared;

    for (int i = 0; i < s->items_per_producer; i++) {
        int item = rand() % 1000;

        sem_wait(&s->empty_slots);
        pthread_mutex_lock(&s->mutex);

        s->buffer[s->in] = item;
        printf("[Producer-%d] Produced %d at %d\n", t->id, item, s->in);
        s->in = (s->in + 1) % s->size;

        pthread_mutex_unlock(&s->mutex);
        sem_post(&s->full_slots);
    }

    printf("[Producer-%d] Finished producing.\n", t->id);
    return NULL;
}


// Consumer thread: removes and processes items from the buffer
void *consumer(void *arg) {
    thread_arg_t *t = arg;
    shared_t *s = t->shared;

    while (1) {
        sem_wait(&s->full_slots);
        pthread_mutex_lock(&s->mutex);

        int item = s->buffer[s->out];
        printf("[Consumer-%d] Dequeued %d from %d\n", t->id, item, s->out);
        s->out = (s->out + 1) % s->size;

        pthread_mutex_unlock(&s->mutex);
        sem_post(&s->empty_slots);

        if (item == POISON_PILL) {
            printf("[Consumer-%d] Exiting.\n", t->id);
            break;
        }

        pthread_mutex_lock(&s->count_mutex);
        s->consumed_count++;
        pthread_mutex_unlock(&s->count_mutex);
    }

    return NULL;
}


// Converts command-line argument to a positive integer
int parse_int(char *s) {
    int x = atoi(s);
    if (x <= 0) {
        fprintf(stderr, "Invalid argument: %s\n", s);
        exit(1);
    }
    return x;
}


int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        printf("Usage: %s <producers> <consumers> <buffer_size> [items_per_producer]\n", argv[0]);
        return 1;
    }

    shared_t s;

    s.producers = parse_int(argv[1]);
    s.consumers = parse_int(argv[2]);
    s.size = parse_int(argv[3]);
    s.items_per_producer = (argc == 5) ? parse_int(argv[4]) : 20;

    s.total_items = s.items_per_producer * s.producers;
    s.consumed_count = 0;

    s.buffer = malloc(sizeof(int) * s.size);
    s.in = s.out = 0;

    pthread_mutex_init(&s.mutex, NULL);
    pthread_mutex_init(&s.count_mutex, NULL);

    sem_init(&s.empty_slots, 0, s.size);
    sem_init(&s.full_slots, 0, 0);

    pthread_t prod_threads[s.producers];
    pthread_t cons_threads[s.consumers];
    thread_arg_t prod_args[s.producers];
    thread_arg_t cons_args[s.consumers];

    srand(time(NULL));

    for (int i = 0; i < s.producers; i++) {
        prod_args[i].id = i + 1;
        prod_args[i].shared = &s;
        pthread_create(&prod_threads[i], NULL, producer, &prod_args[i]);
    }

    for (int i = 0; i < s.consumers; i++) {
        cons_args[i].id = i + 1;
        cons_args[i].shared = &s;
        pthread_create(&cons_threads[i], NULL, consumer, &cons_args[i]);
    }

    for (int i = 0; i < s.producers; i++)
        pthread_join(prod_threads[i], NULL);

    // Inserts poison pills equal to the number of consumers
    for (int i = 0; i < s.consumers; i++) {
        sem_wait(&s.empty_slots);
        pthread_mutex_lock(&s.mutex);

        s->buffer[s->in] = POISON_PILL;
        s->in = (s.in + 1) % s.size;

        pthread_mutex_unlock(&s.mutex);
        sem_post(&s.full_slots);
    }

    for (int i = 0; i < s.consumers; i++)
        pthread_join(cons_threads[i], NULL);

    printf("Summary: expected = %d, consumed = %d\n", s.total_items, s.consumed_count);

    return 0;
}
