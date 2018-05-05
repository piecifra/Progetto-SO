
// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <semaphore.h>

#include "common.h"

#include "so_game_protocol.h"
#include "player_list.h"
#include "network_func.h"

int id_counter = 1;
int timestamp = 0;
World w;

sem_t sem_players_list_TCP, sem_players_list_UDP;

typedef struct {
  volatile int run;
  World* w;
} UpdaterArgs;

// this is the updater threas that takes care of refreshing the agent position
// in your client it is not needed, but you will
// have to copy the x,y,theta fields from the world update packet
// to the vehicles having the corrsponding id
void* updater_thread(void* args_){
  UpdaterArgs* args=(UpdaterArgs*)args_;
  while(args->run){
    World_update(args->w);
    usleep(30000);
  }
  return 0;
}

typedef struct {

    int socket_desc;
    int socket_udp;
    struct sockaddr_in * client_addr;
    struct sockaddr_in * client_addr_udp;
    Image * SurfaceTexture;
    Image * ElevationTexture;
    int id;
    PlayersList * Players;

} player_handler_args;


void * player_handler(void * arg) {

    player_handler_args * args = (player_handler_args *)arg;
    int socket_udp = args->socket_udp;
    int socket_desc = args->socket_desc;
    int id = args->id;
    struct sockaddr_in * client_addr = args->client_addr;
    struct sockaddr_in * client_addr_udp = args->client_addr_udp;
    int client_addr_len = sizeof(*client_addr);
    Image * surfaceTexture = args->SurfaceTexture;
    Image * elevationTexture = args->ElevationTexture;
    PlayersList * players = args->Players;

    int ret, recv_bytes, send_bytes;
    char buf[1000000];

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short
    printf("[PLAYER HANDLER THREAD ID %d] Start session with %s on port %d\n", id, client_ip, client_port);

    // read message from client
    while ((recv_bytes = recv(socket_desc, buf, sizeof(buf), 0)) < 0)
    {
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "[PLAYER HANDLER THREAD] Cannot read from socket");
    }
    IdPacket* deserialized_packet = (IdPacket*)Packet_deserialize(buf, sizeof(buf));
    if(deserialized_packet->id == -1) printf("[PLAYER HANDLER THREAD ID %d] Request recived.\n", id);




    //Send id to client
    PacketHeader packetHeader;
    IdPacket * idPacket = malloc(sizeof(IdPacket));
    packetHeader.type = GetId;
    idPacket->id = id;
    idPacket->header = packetHeader;
    int buf_size = Packet_serialize(buf, &(idPacket->header));
    while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "[PLAYER HANDLER THREAD] Cannot write to socket");
    }
    //free(idPAcket)?
    printf("[PLAYER HANDLER THREAD] ID sended %d\n", id);

    //Recive texture from client
    recv_packet_TCP(socket_desc, buf);
    printf("[PLAYER HANDLER THREAD ID %d] Recived texture from player ID %d, bytes %d\n", id, id, ((PacketHeader *) buf)->size);
    ImagePacket * packetTexture = (ImagePacket *) Packet_deserialize(buf, ((PacketHeader *) buf)->size);
    Image * playerTexture = packetTexture->image;

    //Insert player in list
    player_list_insert(players, id, playerTexture);
    player_list_print(players);
    Vehicle * vehicle=(Vehicle*) malloc(sizeof(Vehicle));
    Vehicle_init(vehicle, &w, id, playerTexture);
    World_addVehicle(&w, vehicle);

    //Send SurfaceTexture to Client
    packetHeader.type = PostTexture;
    ImagePacket * imagePacket = malloc(sizeof(ImagePacket));
    imagePacket->header = packetHeader;
    imagePacket->id = 0;
    imagePacket->image = surfaceTexture;
    buf_size = Packet_serialize(buf, &(imagePacket->header));
    while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "[PLAYER HANDLER THREAD] Cannot write to socket");
    }
    printf("[PLAYER HANDLER THREAD ID %d] SurfaceTexture sent to id %d\n", id, id);


    //Send ElevationTexture to Client
    packetHeader.type = PostElevation;
    imagePacket = malloc(sizeof(ImagePacket));
    imagePacket->header = packetHeader;
    imagePacket->id = 0;
    imagePacket->image = elevationTexture;
    buf_size = Packet_serialize(buf, &(imagePacket->header));
    while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "[PLAYER HANDLER THREAD] Cannot write to socket");
    }
    printf("[PLAYER HANDLER THREAD ID %d] ElevationTexture sent to ID %d\n", id, id);
    free(imagePacket);


    while(1) {

        int i = 0;

        //ret = sem_wait(&sem_players_list_TCP);
        //ERROR_HELPER(ret, "[PLAYER HANDLER THREAD] Error sem wait");

        if(player_list_find(players, id) == NULL) {
            printf("[PLAYER HANDLER THREAD ID %d] Player ID %d quit the game\n", id, id);
            printf("[PLAYER HANDLER THREAD ID %d] Exiting\n", id);
            close(socket_desc);
            pthread_exit(NULL);
        }

        Player * p = players->first;
        ImagePacket * imagePacket = malloc(sizeof(ImagePacket));

        while(p != NULL) {


            if(p->new[id] == 1 && p->id != id) {


                p->new[id] = 0;
                packetHeader.type = PostTexture;
                imagePacket->id = p->id;
                imagePacket->image = p->texture;
                imagePacket->header = packetHeader;
                buf_size = Packet_serialize(buf, &(imagePacket->header));
                printf("[PLAYER HANDLER THREAD ID %d] Sending texture of %d to %d\n", id, id, p->id);
                while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
                    if(ret == 0) {
                        printf("[PLAYER HANDLER THREAD ID %d] Player %d quit the game\n", id, id);
                        printf("[PLAYER HANDLER THREAD ID %d] Exiting\n", id, id);
                        close(socket_desc);
                        player_list_delete(players, id);
                        pthread_exit(NULL);
                    }
                    if(errno == EINTR) continue;
                    ERROR_HELPER(ret, "[PLAYER HANDLER THREAD] Cannot send");
                }

            }
            p = p->next;

        }
        free(imagePacket);
        //ret = sem_post(&sem_players_list_TCP);
        //ERROR_HELPER(ret, "[PLAYER HANDLER THREAD] Error sem wait");


        usleep(2000000);


    }



    // close socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "[PLAYER HANDLER THREAD] Cannot close socket for incoming connection");

    pthread_exit(NULL);

}

void * delete_players_thread(void * args) {

    PlayersList * players = (PlayersList *) args;
    int ret;

    while(1) {

        Player * p = players->first;

        ret = sem_wait(&sem_players_list_UDP);
        ERROR_HELPER(ret, "[DELETE PLAYER THREAD] Error sem wait");
        //ret = sem_wait(&sem_players_list_TCP);
        //ERROR_HELPER(ret, "[DELETE PLAYER THREAD] Error sem wait");

        while(p != NULL) {

            if(p->last_packet_timestamp < timestamp - 500) {

                Vehicle * to_remove = World_getVehicle(&w, p->id);
                int tmpid = p->id;
                World_detachVehicle(&w, to_remove);
                Vehicle_destroy(to_remove);
                player_list_delete(players, p->id);
                printf("[DELETE PLAYER THREAD] Player %d quit game, players are now %d\n", tmpid, players->n);
                break;

            }

            if(p != NULL) p = p->next;

        }

        ret = sem_post(&sem_players_list_UDP);
        ERROR_HELPER(ret, "[DELETE PLAYER THREAD] Error sem wait");
        //ret = sem_post(&sem_players_list_TCP);
        //ERROR_HELPER(ret, "[DELETE PLAYER THREAD] Error sem wait");

        usleep(2000000);

    }

}

void * update_sender_thread_func(void * args) {

    player_handler_args * arg = (player_handler_args *) args;
    int socket_udp = arg->socket_udp;
    PlayersList * players = arg->Players;
    char buf[1000000];
    PacketHeader packetHeader;
    printf("[UPDATE SENDER THREAD] Start sending updates\n");
    int ret, buf_size;
    while(1) {

        Player * p = players->first;
        ClientUpdate * updates = malloc(sizeof(ClientUpdate)*(players->n));
        int i = 0;

        ret = sem_wait(&sem_players_list_UDP);
        ERROR_HELPER(ret, "Error sem wait");

        while(p != NULL) {

            Vehicle * v = World_getVehicle(&w, p->id);

            updates[i].id = p->id;
            updates[i].x = v->x;
            updates[i].y = v->y;
            updates[i++].theta = v->theta;

            p = p->next;

        }


        p = players->first;
        packetHeader.type = WorldUpdate;
        WorldUpdatePacket * worldUpdatePacket = malloc(sizeof(WorldUpdatePacket));
        worldUpdatePacket->header = packetHeader;
        worldUpdatePacket->num_vehicles = players->n;
        worldUpdatePacket->updates = updates;
        buf_size = Packet_serialize(buf, &(worldUpdatePacket->header));

        while(p != NULL) {


            struct sockaddr_in client_addr = p->client_addr_udp;
            ret = sendto(socket_udp, buf, buf_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port); // port number is an unsigned short

            p = p->next;

        }

        ret = sem_post(&sem_players_list_UDP);
        ERROR_HELPER(ret, "Error sem wait");

        usleep(30000);

    }



}


void * update_reciver_thread_func(void * args) {

    player_handler_args * arg = (player_handler_args *) args;
    int socket_udp = arg->socket_udp;
    PlayersList * players = arg->Players;
    int slen, ret;
    struct sockaddr_in client_addr_udp;
    char buf[1000000];
    printf("[UPDATE RECIVER TREAD] Start recivng updates\n");
    while(1) {

        ret = recvfrom(socket_udp, buf, sizeof(buf), 0, (struct sockaddr *) &client_addr_udp, &slen);
        ERROR_HELPER(ret, "[UPDATE RECIVER THREAD] Error recive");
        VehicleUpdatePacket * vehicleUpdatePacket = Packet_deserialize(buf, ret);
        Player * p = player_list_find(players, vehicleUpdatePacket->id);
        Vehicle * v = World_getVehicle(&w, p->id);
        if(v != 0) {
            v->translational_force_update = vehicleUpdatePacket->translational_force;
            v->rotational_force_update = vehicleUpdatePacket->rotational_force;
        }
        timestamp += 1;
        p->last_packet_timestamp = timestamp;
        p->client_addr_udp = client_addr_udp;

    }

}


int main(int argc, char **argv) {

    if (argc<3) {
        printf("usage: <surface texture> <elevation texture>\n");
        exit(-1);
    }

    Image * surfaceTexture = Image_load(argv[1]);
    Image * elevationTexture = Image_load(argv[2]);


    int ret;
    int socket_desc, client_desc;

    World_init(&w, surfaceTexture, elevationTexture,  0.5, 0.5, 0.5);
    pthread_t runner_thread;
    pthread_attr_t runner_attrs;
    UpdaterArgs runner_args={
    .run=1,
    .w=&w
    };
    pthread_attr_init(&runner_attrs);
    pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

    // initialize socket for listening
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); // don't forget about network byte order!


    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);

    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));



    //ret = sem_init(&sem_players_list_TCP, NULL, 1);
    //ERROR_HELPER(ret, "Error sem init");
    ret = sem_init(&sem_players_list_UDP, NULL, 1);
    ERROR_HELPER(ret, "Error sem init");

    PlayersList * players = players_list_new();
    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in si_me = { 0 };
    ERROR_HELPER(udp_socket, "[MAIN] Cannot open udp socket");
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(SERVER_PORT);
    si_me.sin_addr.s_addr = INADDR_ANY;

    int reuseaddr_opt_udp = 1;
    ret = setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));

    ret = bind(udp_socket, &si_me, sizeof(si_me));
    ERROR_HELPER(ret, "[MAIN] Error bind");
    printf("[MAIN] Opened UDP socket %d\n", udp_socket);

    player_handler_args * urt_arg = malloc(sizeof(player_handler_args));
    urt_arg->socket_udp = udp_socket;
    urt_arg->Players = players;
    pthread_t urt;
    pthread_create(&urt, NULL, update_reciver_thread_func, (void *) urt_arg);
    pthread_detach(urt);

    player_handler_args * ust_arg = malloc(sizeof(player_handler_args));
    ust_arg->socket_udp = udp_socket;
    ust_arg->Players = players;
    pthread_t ust;
    pthread_create(&ust, NULL, update_sender_thread_func, (void *) ust_arg);
    pthread_detach(ust);

    pthread_t pdt;
    pthread_create(&pdt, NULL, delete_players_thread, (void *) players);
    pthread_detach(pdt);



    //Start Listening for new players
    while(1) {

        client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue;

        pthread_t session_init_thread, update_reciver_thread;

        //Put arguments for the new thread into a buffer
        player_handler_args * sit_arg = malloc(sizeof(player_handler_args));


        //sit thread arguments
        size_t peeraddrlen;
        struct sockaddr_in * si_other = malloc(sizeof(struct sockaddr_in));
        struct sockaddr_in * client_addr_udp = malloc(sizeof(struct sockaddr_in));

        sit_arg->socket_desc = client_desc;
        sit_arg->socket_udp = udp_socket;
        sit_arg->client_addr = client_addr;
        sit_arg->client_addr_udp = client_addr_udp;
        sit_arg->SurfaceTexture = surfaceTexture;
        sit_arg->ElevationTexture = elevationTexture;
        sit_arg->Players = players;
        sit_arg->id = id_counter++;


        ret = pthread_create(&session_init_thread, NULL, player_handler, (void *)sit_arg);
        ret = pthread_detach(session_init_thread);


        client_addr = calloc(1, sizeof(struct sockaddr_in));


    }



    return 0;
}
