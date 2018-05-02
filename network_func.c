#include "network_func.h"
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "common.h"

void recv_packet_TCP(int socket_desc, char buf[]) {

    int recived_bytes = 0, left_bytes = 8, packetsize, ret;
    char * tmp = buf;
    while(left_bytes > 0) {
        while((ret = recv(socket_desc, tmp + recived_bytes, left_bytes, 0)) < 0) {
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }
        left_bytes -= ret;
        recived_bytes += ret;
    }

    //PacketHeader recived

    left_bytes = ((PacketHeader *) buf)->size - 8;
    printf("RECV FUNC: Incoming packet of %d bytes\n", left_bytes);
    recived_bytes = 0;
    while(left_bytes > 0) {
        while((ret = recv(socket_desc, tmp + 8 + recived_bytes, left_bytes, 0)) < 0) {
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }
        left_bytes -= ret;
        recived_bytes += ret;
    }

    printf("RECV FUNC: Recived %d bytes, left %d\n", recived_bytes, left_bytes);

    //Full packet recived

}

