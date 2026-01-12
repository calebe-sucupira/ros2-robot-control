#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include <uxr/client/profile/transport/custom/custom_transport.h>
#include "picow_udp_transports.h"

uint8_t trans_recv_buff[512] = { 0 };
uint16_t trans_recv_len = 0;

void usleep(uint64_t us) {
    sleep_us(us);
}

int clock_gettime(clockid_t unused, struct timespec *tp) {
    uint64_t m = time_us_64();
    tp->tv_sec = m / 1000000;
    tp->tv_nsec = (m % 1000000) * 1000;
    return 0;
}

static void callback_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    ST_PICOW_TRANSPORT_PARAMS* params = (ST_PICOW_TRANSPORT_PARAMS*) arg;

    cyw43_arch_lwip_begin();
    if (params) {
        if (ip_addr_cmp(addr, &params->ipaddr)) {
            if (trans_recv_len > 0) {
                printf("Aviso: possível perda de dados - buffer cheio\n");
            }
            trans_recv_len = pbuf_copy_partial(p, trans_recv_buff, sizeof(trans_recv_buff), 0);
        }
    }
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

bool picow_udp_transport_open(struct uxrCustomTransport * transport) {
    ST_PICOW_TRANSPORT_PARAMS* params = (ST_PICOW_TRANSPORT_PARAMS*) transport->args;

    if (params) {
        cyw43_arch_lwip_begin();
        params->pcb = udp_new();
        udp_recv(params->pcb, callback_recv, params);
        cyw43_arch_lwip_end();

        printf("Transporte UDP aberto\n");
        return true;
    }
    return false;
}

bool picow_udp_transport_close(struct uxrCustomTransport * transport) {
    ST_PICOW_TRANSPORT_PARAMS* params = (ST_PICOW_TRANSPORT_PARAMS*) transport->args;

    if (params) {
        cyw43_arch_lwip_begin();
        udp_remove(params->pcb);
        cyw43_arch_lwip_end();
        printf("Transporte UDP fechado\n");
    }
    return true;
}

size_t picow_udp_transport_write(struct uxrCustomTransport * transport, const uint8_t *buf, size_t len, uint8_t *errcode) {
    size_t trans_len = 0;
    *errcode = 1;

    ST_PICOW_TRANSPORT_PARAMS* params = (ST_PICOW_TRANSPORT_PARAMS*) transport->args;
    if (params) {
        cyw43_arch_lwip_begin();
        struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
        if (p) {
            memcpy(p->payload, buf, len);
            err_t error = udp_sendto(params->pcb, p, &(params->ipaddr), params->port);
            if (error == ERR_OK) {
                *errcode = 0;
                trans_len = len;
            }
            pbuf_free(p);
        }
        cyw43_arch_lwip_end();
    }
    return trans_len;
}

size_t picow_udp_transport_read(struct uxrCustomTransport * transport, uint8_t *buf, size_t len, int timeout, uint8_t *errcode) {
    size_t recv_len = 0;
    *errcode = 1;

    #if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
    #endif

    ST_PICOW_TRANSPORT_PARAMS* params = (ST_PICOW_TRANSPORT_PARAMS*) transport->args;
    if (params && (trans_recv_len > 0)) {
        cyw43_arch_lwip_begin();
        if (trans_recv_len >= len) {
            recv_len = len;
            *errcode = 0;
        } else {
            recv_len = trans_recv_len;
        }
        memcpy(buf, trans_recv_buff, recv_len);
        trans_recv_len = 0;
        cyw43_arch_lwip_end();
    }

    return recv_len;
}