#ifndef MBUS_H
#define MBUS_H

#define BUS_DEFAULT_CLIENTS 128
#define BUS_MAX_CLIENTS UINT_MAX

typedef unsigned int bus_client_id_t;
typedef void (*bus_client_cb_t)(void *ctx, void *msg);

/* FIXME: rewrite with <stdatomic.h> */
#define CAS(dst, expected, value)                                        \
    __atomic_compare_exchange(dst, expected, value, 0, __ATOMIC_SEQ_CST, \
                              __ATOMIC_SEQ_CST)

typedef struct {
    bool registered;
    unsigned int refcnt;
    bus_client_cb_t callback;
    void *ctx;
} bus_client_t;

typedef struct {
    bus_client_t *clients;
    const unsigned int n_clients;
} bus_t;

bool __attribute__((warn_unused_result))
bus_new(bus_t **bus, unsigned int n_clients);

bool __attribute__((warn_unused_result, nonnull(1)))
bus_register(bus_t *bus,
             bus_client_id_t id,
             bus_client_cb_t callback,
             void *ctx);

bool __attribute__((warn_unused_result, nonnull(1)))
bus_send(bus_t *bus, bus_client_id_t id, void *msg, bool broadcast);

bool __attribute__((warn_unused_result, nonnull(1)))
bus_unregister(bus_t *bus, bus_client_id_t id);

void bus_free(bus_t *bus);

#endif
