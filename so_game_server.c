
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

#include "common.h"

#include "so_game_protocol.h"
#include "player_list.h"

int id_counter = 1;

typedef struct {

    int socket_desc;
    struct sockaddr_in * client_addr;
    Image * SurfaceTexture;
    Image * ElevationTexture;
    PlayersList * Players;

} player_handler_args;


void * player_handler(void * arg) {

    player_handler_args * args = (player_handler_args *)arg;

    int socket_desc = args->socket_desc;
    struct sockaddr_in *client_addr = args->client_addr;
    Image * surfaceTexture = args->SurfaceTexture;
    Image * elevationTexture = args->ElevationTexture;
    PlayersList * players = args->Players;

    int ret, recv_bytes, send_bytes;

    char buf[1000000];


    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short




    // read message from client
    while ((recv_bytes = recv(socket_desc, buf, sizeof(buf), 0)) < 0)
    {
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    IdPacket* deserialized_packet = (IdPacket*)Packet_deserialize(buf, sizeof(buf));
    if(deserialized_packet->id == -1) fprintf(stderr, "Request recived.\n");




    //Send id to client
    PacketHeader packetHeader;
    IdPacket * idPacket = malloc(sizeof(IdPacket));
    packetHeader.type = GetId;
    //Critical section
    int id = id_counter++;
    idPacket->id = id;
    idPacket->header = packetHeader;
    int buf_size = Packet_serialize(buf, &(idPacket->header));
    while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot write to socket");
    }
    //free(idPAcket)?



    //Recive texture from client
    while ((recv_bytes = recv(socket_desc, buf, sizeof(buf), 0)) < 0)
    {
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    printf("Recived texture from player %d\n", id);
    ImagePacket * imagePacket = (ImagePacket *) Packet_deserialize(buf, recv_bytes);
    Image * playerTexture = imagePacket->image;



    //Insert player in list
    player_list_insert(players, id, playerTexture);




    //Send SurfaceTexture to Client
    packetHeader.type = PostTexture;
    imagePacket = malloc(sizeof(ImagePacket));
    imagePacket->header = packetHeader;
    imagePacket->id = 0;
    imagePacket->image = surfaceTexture;
    buf_size = Packet_serialize(buf, &(imagePacket->header));
    while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot write to socket");
    }
    printf("SurfaceTexture sent to id %d, bytes: %d\n", id, ret);

    usleep(5000000);

    //Send ElevationTexture to Client
    packetHeader.type = PostElevation;
    imagePacket = malloc(sizeof(ImagePacket));
    imagePacket->header = packetHeader;
    imagePacket->id = 0;
    imagePacket->image = elevationTexture;
    buf_size = Packet_serialize(buf, &(imagePacket->header));
    while((ret = send(socket_desc, buf, buf_size, 0)) < 0) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot write to socket");
    }
    printf("ElevationTexture sent to id %d\n", id);


    /*
    //Send the new player to all players
    packetHeader.type = PostTexture;
    imagePacket->image = playerTexture;
    imagePacket->id = id;
    imagePacket->header = packetHeader;
    buf_size = Packet_serialize(buf, &(imagePacket->header));
    Player * player = players->first;
    while(player != NULL) {

        int socket_desc_to_send = player->socket;
        while((ret = send(socket_desc_to_send, buf, buf_size, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
        }
        player = player->next;
    }
    */



    // close socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");

    pthread_exit(NULL);

}


void * players_printer(void * args) {

    player_handler_args * arg = (player_handler_args *) args;
    Player * p = arg->Players;

    while(1) {
        player_list_print(p);
        usleep(5000000);
    }

    pthread_exit(NULL);

}

int main(int argc, char **argv) {

    if (argc<3) {
        printf("usage: <surface texture> <elevation texture>\n");
        exit(-1);
    }

    Image * surfaceTexture = Image_load(argv[1]);
    Image * elevationTexture = Image_load(argv[2]);
    //if (!elevationTexture || !surfaceTexture) exit(-1);

    // not needed here
    //   // construct the world
    // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

    // // create a vehicle
    // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
    // Vehicle_init(vehicle, &world, 0, vehicle_texture);

    // // add it to the world
    // World_addVehicle(&world, vehicle);



    // // initialize GL
    // glutInit(&argc, argv);
    // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    // glutCreateWindow("main");

    // // set the callbacks
    // glutDisplayFunc(display);
    // glutIdleFunc(idle);
    // glutSpecialFunc(specialInput);
    // glutKeyboardFunc(keyPressed);
    // glutReshapeFunc(reshape);

    // WorldViewer_init(&viewer, &world, vehicle);


    // // run the main GL loop
    // glutMainLoop();

    // // check out the images not needed anymore
    // Image_free(vehicle_texture);
    // Image_free(surface_texture);
    // Image_free(surface_elevation);

    // // cleanup
    // World_destroy(&world);

    int ret;
    int socket_desc, client_desc;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

    // initialize socket for listening
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); // don't forget about network byte order!

    /* We enable SO_REUSEADDR to quickly restart our server after a crash:
    * for more details, read about the TIME_WAIT state in the TCP protocol */
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);

    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));


    PlayersList * players = players_list_new();


    pthread_t t;
    player_handler_args * thread_args = malloc(sizeof(player_handler_args));
    thread_args->Players = players;

    ret = pthread_create(&t, NULL, players_printer, (void *) thread_args);
    ret = pthread_detach(t);


    //Start Listening for new players
    while(1) {

        client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue;

        pthread_t thread;

        // put arguments for the new thread into a buffer
        player_handler_args * thread_args = malloc(sizeof(player_handler_args));
        thread_args->socket_desc = client_desc;
        thread_args->client_addr = client_addr;
        thread_args->SurfaceTexture = surfaceTexture;
        thread_args->ElevationTexture = elevationTexture;
        thread_args->Players = players;

        ret = pthread_create(&thread, NULL, player_handler, (void *)thread_args);
        ret = pthread_detach(thread);

        client_addr = calloc(1, sizeof(struct sockaddr_in));


    }



    return 0;
}
