#ifndef PICOW_UDP_TRANSPORTS_H
#define PICOW_UDP_TRANSPORTS_H

#include <uxr/client/profile/transport/custom/custom_transport.h>
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct udp_pcb* pcb;
    ip_addr_t ipaddr;
    uint16_t port;
} ST_PICOW_TRANSPORT_PARAMS;

bool picow_udp_transport_open(struct uxrCustomTransport * transport);
bool picow_udp_transport_close(struct uxrCustomTransport * transport);
size_t picow_udp_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t picow_udp_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

#ifdef __cplusplus
}
#endif

#endif