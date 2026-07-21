#include "udp_transport.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "uxr/client/profile/transport/custom/custom_transport.h"

bool udp_transport_open(struct uxrCustomTransport *transport)
{
    udp_transport_context_t *context = transport->args;

    context->socket_fd = socket(
        AF_INET,
        SOCK_DGRAM,
        IPPROTO_IP
    );

    return context->socket_fd >= 0;
}

bool udp_transport_close(struct uxrCustomTransport *transport)
{
    udp_transport_context_t *context = transport->args;

    if (context->socket_fd >= 0)
    {
        close(context->socket_fd);
        context->socket_fd = -1;
    }

    return true;
}

size_t udp_transport_write(
    struct uxrCustomTransport *transport,
    const uint8_t *buffer,
    size_t length,
    uint8_t *error_code
)
{
    udp_transport_context_t *context = transport->args;

    struct sockaddr_in destination = {
        .sin_family = AF_INET,
        .sin_port = htons(context->agent_port),
    };

    if (
        inet_pton(
            AF_INET,
            context->agent_ip,
            &destination.sin_addr
        ) != 1
    )
    {
        *error_code = EINVAL;
        return 0;
    }

    int result = sendto(
        context->socket_fd,
        buffer,
        length,
        0,
        (struct sockaddr *)&destination,
        sizeof(destination)
    );

    if (result < 0)
    {
        *error_code = errno;
        return 0;
    }

    return (size_t)result;
}

size_t udp_transport_read(
    struct uxrCustomTransport *transport,
    uint8_t *buffer,
    size_t length,
    int timeout_ms,
    uint8_t *error_code
)
{
    udp_transport_context_t *context = transport->args;

    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    if (
        setsockopt(
            context->socket_fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        ) < 0
    )
    {
        *error_code = errno;
        return 0;
    }

    int result = recvfrom(
        context->socket_fd,
        buffer,
        length,
        0,
        NULL,
        NULL
    );

    if (result < 0)
    {
        *error_code = errno;
        return 0;
    }

    return (size_t)result;
}
