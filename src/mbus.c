#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mbus.h"

/*
 * Allocate a new bus. If @n_clients is non-zero, it allocates space for
 * specific number of clients; otherwise, it uses BUS_DEFAULT_CLIENTS.
 * @n_clients can not be greater than BUS_MAX_CLIENTS. Returns true on success.
 */
bool __attribute__((warn_unused_result))
bus_new(bus_t **bus, unsigned int n_clients)
{
    if (n_clients > BUS_MAX_CLIENTS)
        return false;

    bus_t *b;
    if (!(b = malloc(sizeof(bus_t))))
        return false;

    /* Initialize bus struct */
    *(unsigned int *) &b->n_clients =
        !n_clients ? BUS_DEFAULT_CLIENTS : n_clients;
    if (!(b->clients = calloc(b->n_clients, sizeof(bus_client_t)))) {
        free(b);
        return false;
    }

    *bus = b;
    return true;
}

/*
 * Register a new client with the specified @id.
 * The ID must satisfy 0 <= ID < n_clients and not be in use; otherwise the
 * function would fail. Whenever a message is sent to this client, @callback
 * will be called. The first argument for @callback is the the user-supplied
 * context, @ctx (can be ommitted by passing NULL). The second argument for
 * @callback will be the received message. Returns true on success.
 */
bool __attribute__((warn_unused_result, nonnull(1)))
bus_register(bus_t *bus,
             bus_client_id_t id,
             bus_client_cb_t callback,
             void *ctx)
{
    if (id >= bus->n_clients)
        return false;

    bus_client_t null_client = {0};
    bus_client_t new_client = {
        .registered = true,
        .callback = callback,
        .ctx = ctx,
        .refcnt = 0,
    };

    return (bool) CAS(&(bus->clients[id]), &null_client, &new_client);
}

/*
 * Attempt to call a client's callback function to send a message.
 * Might fail if such client gets unregistered while attempting to send message.
 */
static bool execute_client_callback(bus_client_t *client, void *msg)
{
    /* Load the client with which we are attempting to communicate. */
    bus_client_t local_client;
    __atomic_load(client, &local_client, __ATOMIC_SEQ_CST);

    /* Loop until reference count isupdated or client becomes unregistered */
    while (local_client.registered) {
        /* The expected reference count is the current one + 1 */
        bus_client_t new_client = local_client;
        ++(new_client.refcnt);

        /* If CAS succeeds, the client had the expected reference count, and
         * we updated it successfully. If CAS fails, the client was updated
         * recently. The actual value is copied to local_client.
         */
        if (CAS(client, &local_client, &new_client)) {
            /* Send a message and decrease the reference count back */
            local_client.callback(local_client.ctx, msg);
            __atomic_fetch_sub(&(client->refcnt), 1, __ATOMIC_SEQ_CST);
            return true;
        }
    }

    /* Client was not registered or got unregistered while we attempted to send
     * a message
     */
    return false;
}

/*
 * If @broadcast is set to false, it sends a message to the client with the
 * specified @id. If @broadcast is set to true, the message is sent to every
 * registered client, and the supplied ID is ignored. Returns true on success.
 */
bool __attribute__((warn_unused_result, nonnull(1)))
bus_send(bus_t *bus, bus_client_id_t id, void *msg, bool broadcast)
{
    if (broadcast) {
        for (id = 0; id < bus->n_clients; ++id)
            execute_client_callback(&(bus->clients[id]), msg);
        return true;
    }
    if (id >= bus->n_clients)
        return false;
    return execute_client_callback(&(bus->clients[id]), msg);
}

/*
 * Unregister the client with the specified @id. No additional can be made
 * to the specified client. Returns true on success.
 */
bool __attribute__((warn_unused_result, nonnull(1)))
bus_unregister(bus_t *bus, bus_client_id_t id)
{
    if (id >= bus->n_clients)
        return false;

    /* Load the client we are attempting to unregister */
    bus_client_t local_client, null_client = {0};
    __atomic_load(&(bus->clients[id]), &local_client, __ATOMIC_SEQ_CST);

    /* It was already unregistered */
    if (!local_client.registered)
        return false;

    do {
        local_client.refcnt = 0; /* the expected reference count */

        /* If CAS succeeds, the client had refcnt = 0 and got unregistered.
         * If CAS does not succeed, the value of the client gets copied into
         * local_client.
         */
        if (CAS(&(bus->clients[id]), &local_client, &null_client))
            return true;
    } while (local_client.registered);

    /* Someone else unregistered this client */
    return true;
}

/* Free the bus object */
void bus_free(bus_t *bus)
{
    if (!bus)
        return;
    free(bus->clients);
    free(bus);
}
