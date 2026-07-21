#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "pico/sync.h"
#include "uxr/client/profile/transport/custom/custom_transport.h"

#define PICOW_UDP_RX_BUFFER_SIZE 2048

typedef struct
{
    struct udp_pcb *pcb;
    ip_addr_t agent_address;
    uint16_t agent_port;
    critical_section_t receive_lock;
    uint8_t receive_buffer[PICOW_UDP_RX_BUFFER_SIZE];
    size_t receive_length;
} picow_udp_transport_t;

bool picow_udp_transport_init(
    picow_udp_transport_t *context,
    const char *agent_ip,
    uint16_t agent_port
);

bool picow_udp_transport_open(
    struct uxrCustomTransport *transport
);

bool picow_udp_transport_close(
    struct uxrCustomTransport *transport
);

size_t picow_udp_transport_write(
    struct uxrCustomTransport *transport,
    const uint8_t *buffer,
    size_t length,
    uint8_t *error_code
);

size_t picow_udp_transport_read(
    struct uxrCustomTransport *transport,
    uint8_t *buffer,
    size_t length,
    int timeout_ms,
    uint8_t *error_code
);
