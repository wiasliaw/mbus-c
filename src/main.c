#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "mbus.h"

#define NUM_THREADS 4

/* Data passed to each thread */
typedef struct {
    bus_t *bus;
    unsigned int id;
} thread_data_t;

/* Function to be called by the bus for each new message */
static void bus_callback(void *_ctx, void *_msg)
{
    unsigned int ctx = *(unsigned int *) _ctx, msg = *(unsigned int *) _msg;
    printf("Callback for thread %u received: %u\n", ctx, msg);
}

/* This funcion will be spawned NUM_THREADS times as a separate thread. */
static void *thread_func(void *_data)
{
    thread_data_t *data = (thread_data_t *) _data;
    bus_client_id_t dest = (data->id + 1) % NUM_THREADS;

    /* Register our callback */
    if (!bus_register(data->bus, data->id, &bus_callback, &(data->id))) {
        perror("bus_register");
        return NULL;
    }
    printf("Registered callback from thread %u\n", data->id);

    /* Loop until the destination is registered from a separate thread */
    while (!bus_send(data->bus, dest, &(data->id), false))
        ;

    if (bus_unregister(data->bus, dest))
        return NULL;

    return NULL;
}

int main()
{
    pthread_t threads[NUM_THREADS];
    thread_data_t ctx[NUM_THREADS];

    bus_t *bus;
    if (!bus_new(&bus, 0)) {
        perror("bus_new");
        exit(EXIT_FAILURE);
    }

    /* Launch threads, each with their own context containing a reference to the
     * bus and their ID
     */
    for (int i = 0; i < NUM_THREADS; ++i) {
        ctx[i].bus = bus, ctx[i].id = i;
        if (pthread_create(&threads[i], NULL, thread_func, &ctx[i]))
            perror("pthread_create");
    }

    /* Wait until completion */
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_join(threads[i], NULL))
            perror("pthread_join");
    }

    bus_free(bus);

    return 0;
}
