#include "picow_udp_transport.h"

#include <string.h>
#include <time.h>

#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

void usleep(uint64_t microseconds)
{
    sleep_us(microseconds);
}

int clock_gettime(
    clockid_t clock_id,
    struct timespec *time_point
)
{
    (void)clock_id;

    if (time_point == NULL)
    {
        return -1;
    }

    uint64_t microseconds = time_us_64();

    time_point->tv_sec = microseconds / 1000000;
    time_point->tv_nsec =
        (microseconds % 1000000) * 1000;

    return 0;
}

static void receive_callback(
    void *argument,
    struct udp_pcb *pcb,
    struct pbuf *packet,
    const ip_addr_t *address,
    u16_t port
)
{
    (void)pcb;

    picow_udp_transport_t *context = argument;

    if (
        context == NULL ||
        packet == NULL ||
        !ip_addr_cmp(address, &context->agent_address) ||
        port != context->agent_port
    )
    {
        if (packet != NULL)
        {
            pbuf_free(packet);
        }

        return;
    }

    if (packet->tot_len <= sizeof(context->receive_buffer))
    {
        critical_section_enter_blocking(
            &context->receive_lock
        );

        if (context->receive_length == 0)
        {
            context->receive_length = pbuf_copy_partial(
                packet,
                context->receive_buffer,
                packet->tot_len,
                0
            );
        }

        critical_section_exit(
            &context->receive_lock
        );
    }

    pbuf_free(packet);
}

bool picow_udp_transport_init(
    picow_udp_transport_t *context,
    const char *agent_ip,
    uint16_t agent_port
)
{
    if (
        context == NULL ||
        agent_ip == NULL ||
        agent_port == 0
    )
    {
        return false;
    }

    memset(context, 0, sizeof(*context));

    if (!ipaddr_aton(agent_ip, &context->agent_address))
    {
        return false;
    }

    context->agent_port = agent_port;
    critical_section_init(&context->receive_lock);

    return true;
}

bool picow_udp_transport_open(
    struct uxrCustomTransport *transport
)
{
    if (transport == NULL || transport->args == NULL)
    {
        return false;
    }

    picow_udp_transport_t *context = transport->args;

    cyw43_arch_lwip_begin();

    context->pcb = udp_new();

    if (context->pcb == NULL)
    {
        cyw43_arch_lwip_end();
        return false;
    }

    err_t result = udp_bind(
        context->pcb,
        IP_ADDR_ANY,
        0
    );

    if (result == ERR_OK)
    {
        udp_recv(
            context->pcb,
            receive_callback,
            context
        );
    }
    else
    {
        udp_remove(context->pcb);
        context->pcb = NULL;
    }

    cyw43_arch_lwip_end();

    return result == ERR_OK;
}

bool picow_udp_transport_close(
    struct uxrCustomTransport *transport
)
{
    if (transport == NULL || transport->args == NULL)
    {
        return false;
    }

    picow_udp_transport_t *context = transport->args;

    cyw43_arch_lwip_begin();

    if (context->pcb != NULL)
    {
        udp_remove(context->pcb);
        context->pcb = NULL;
    }

    cyw43_arch_lwip_end();

    critical_section_enter_blocking(
        &context->receive_lock
    );

    context->receive_length = 0;

    critical_section_exit(
        &context->receive_lock
    );

    return true;
}

size_t picow_udp_transport_write(
    struct uxrCustomTransport *transport,
    const uint8_t *buffer,
    size_t length,
    uint8_t *error_code
)
{
    if (error_code != NULL)
    {
        *error_code = 1;
    }

    if (
        transport == NULL ||
        transport->args == NULL ||
        buffer == NULL ||
        length == 0 ||
        length > UINT16_MAX
    )
    {
        return 0;
    }

    picow_udp_transport_t *context = transport->args;

    if (context->pcb == NULL)
    {
        return 0;
    }

    cyw43_arch_lwip_begin();

    struct pbuf *packet = pbuf_alloc(
        PBUF_TRANSPORT,
        (u16_t)length,
        PBUF_RAM
    );

    if (packet == NULL)
    {
        cyw43_arch_lwip_end();
        return 0;
    }

    err_t result = pbuf_take(
        packet,
        buffer,
        length
    );

    if (result == ERR_OK)
    {
        result = udp_sendto(
            context->pcb,
            packet,
            &context->agent_address,
            context->agent_port
        );
    }

    pbuf_free(packet);
    cyw43_arch_lwip_end();

    if (result != ERR_OK)
    {
        return 0;
    }

    if (error_code != NULL)
    {
        *error_code = 0;
    }

    return length;
}

size_t picow_udp_transport_read(
    struct uxrCustomTransport *transport,
    uint8_t *buffer,
    size_t length,
    int timeout_ms,
    uint8_t *error_code
)
{
    if (error_code != NULL)
    {
        *error_code = 1;
    }

    if (
        transport == NULL ||
        transport->args == NULL ||
        buffer == NULL ||
        length == 0
    )
    {
        return 0;
    }

    picow_udp_transport_t *context = transport->args;
    absolute_time_t deadline =
        make_timeout_time_ms(timeout_ms);

    do
    {
        critical_section_enter_blocking(
            &context->receive_lock
        );

        size_t received = context->receive_length;

        if (received > 0)
        {
            if (received > length)
            {
                received = length;
            }

            memcpy(
                buffer,
                context->receive_buffer,
                received
            );

            size_t remaining =
                context->receive_length - received;

            if (remaining > 0)
            {
                memmove(
                    context->receive_buffer,
                    context->receive_buffer + received,
                    remaining
                );
            }

            context->receive_length = remaining;

            critical_section_exit(
                &context->receive_lock
            );

            if (error_code != NULL)
            {
                *error_code = 0;
            }

            return received;
        }

        critical_section_exit(
            &context->receive_lock
        );

        sleep_ms(1);
    }
    while (!time_reached(deadline));

    return 0;
}
