#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct uxrCustomTransport;

typedef struct
{
    int socket_fd;
    const char *agent_ip;
    uint16_t agent_port;
} udp_transport_context_t;

bool udp_transport_open(struct uxrCustomTransport *transport);
bool udp_transport_close(struct uxrCustomTransport *transport);

size_t udp_transport_write(
    struct uxrCustomTransport *transport,
    const uint8_t *buffer,
    size_t length,
    uint8_t *error_code
);

size_t udp_transport_read(
    struct uxrCustomTransport *transport,
    uint8_t *buffer,
    size_t length,
    int timeout_ms,
    uint8_t *error_code
);
