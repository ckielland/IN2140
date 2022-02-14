#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/stat.h>

#include "protocol.h"
#include "send_packet.h"


char *server_port;
int N;
int clients_so_far = 0;
unsigned int n; //total amount of packets needed to send the whole file
int terminated = 0; //amount of clients that have received the file
unsigned int serverID = 100; //Not specified in the text, so I take the liberty to give it an arbitrary number

/*Globally declared array of pointers of struct router pointers */
struct rdp_connection** clients;

/* Frees all allocated memory */
void freeAll(){
  int i;
  for (i = 0; i < N; i++){
   free(clients[i]);
  }
  free(clients);
}


int main(int argc, char *argv[]){

  /* Check command line arguments */
    if(argc < 5)
    {
      fprintf(stderr, "usage : %s <port> <filename> <N> <probability loss>\n", argv[0]);
      exit(EXIT_FAILURE);
    }

    server_port = argv[1];
    char* filename = argv[2];
    FILE *file = fopen(filename, "rb");

    /*Check filename */
    if(file == NULL)
    {
      perror("Failure in fopen");
      exit(EXIT_FAILURE);
    }

    N = atoi(argv[3]);
    float drop_percentage = atof(argv[4]);
    set_loss_probability(drop_percentage);

    int i;

    /* Set up and get the socket_fd from the RDP protocol */

    int socket_fd = rdp_serverSocket(server_port);

    if (socket_fd == -1)
    {
      fprintf(stderr, "Failure while configuring socket\n");
      fclose (file);
      exit(EXIT_FAILURE);
    }


    /* Set up to reuse the port */
    int yes = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      perror("Failure while setting up setsockopt.\n");
      rdp_close(socket_fd);
      fclose (file);
      exit(EXIT_FAILURE);
    }

    /* Calculate the amount of packages required to send the whole file */
    int fd = fileno(file);
    struct stat file_size;
    fstat(fd, &file_size);

    int payload_size = (sizeof(char) * file_size.st_size);
    float m = ceil(payload_size/999);

    n = (int) m +1;
    /* Allocate memory for the array of connections. Max clients = N */
    clients = malloc(sizeof(struct rdp_connection*) * N);

    /* Initialize array */
    for (i = 0; i < N; i++)
    {
      clients[i] = NULL;
    }

    printf("***** Waiting for connections *****\n");

    /* Create file descriptor set to be used in the event loop */
    fd_set file_descriptors;

    /*
    * This is the servers event loop based on the
    * first implementation option, i.e. waits for 1 sec before
    * trying again.
    */
    int running = 1;
    while (running){
      struct timeval tv = {1, 0};
      FD_ZERO(&file_descriptors);
      FD_SET(socket_fd, &file_descriptors);
      int ret = select(socket_fd +1, &file_descriptors, NULL, NULL, &tv);

      if(ret == -1){
            perror("Failure in select");
            rdp_close(socket_fd);
            freeAll();
            fclose (file);
            exit(EXIT_FAILURE);
        }

      if (FD_ISSET(socket_fd, &file_descriptors))
      {

      /*
      * First it checks if a new RDP connection request has arrived
      * and has been accepted.
      */
      struct rdp_header header = {0};

      struct rdp_connection *newClient = rdp_accept(socket_fd, &header, clients, N);

       /*
       * Here I have decided to include the control of whether to accept or not
       * the connection according to how many clients have been served/meaning how many times
       * the file has been send so far in the server and not inside RDP.
       * I assumed that it is the server's responsibility to keep track of how many clients
       * it has served. (While RDP is responsible for the connection IDs.)
       */

       if (newClient != NULL)
       {
         if (clients_so_far+1 > N)
         {
           int d = rdp_deny(socket_fd, newClient->client_id, serverID, RDP_FULL_SERVER, (struct sockaddr*) &newClient->address, newClient->len);
           if (d == -1)
           {
             perror("Connection denial failed. ");
             rdp_close(socket_fd);
             free(newClient);
             freeAll();
             fclose (file);
             exit(EXIT_FAILURE);
           }
           else if (d == 0)
           {
             printf("Have served max number of clients. \n");
             free(newClient);
           }
         }

         else if(clients_so_far+1 <= N)
          {
            clients[clients_so_far] = newClient;
             if (rdp_confirm(socket_fd, clients[clients_so_far]->client_id, serverID, (struct sockaddr*)&clients[clients_so_far]->address, clients[clients_so_far]->len) == -1)
             {
               perror("Connection confirmation failed. ");
               rdp_close(socket_fd);
               freeAll();
               fclose (file);
               exit(EXIT_FAILURE);
              }
              clients_so_far++;
          }
        }

      /*
      * Check that rdp_accept returned NULL because of an ACK packet or
      * connection termination packet.
      */
      else
      {
        /*
        * If instead of a connection request, an ACK packet has been received
        * identify it and update the last received ack nr on the client
        * who sent it.
        */
        if (header.flags == RDP_ACK)
        {
          unsigned int client_id = header.senderid;
          unsigned char ack = header.ackseq;
          for (i = 0; i < N; i++)
          {
             if ((clients[i] != NULL) && (clients[i]->client_id == client_id))
             {
               clients[i]->last_ack = ack;
               clients[i]->last_sent++;
               clients[i]->total_packets++;
             }
          }
        }
        else if(header.flags == RDP_CONNECTION_TERMINATION)
        {
          printf("DISCONNECTED <%d> <%d> \n\n", header.senderid, header.recvid);

          for (i = 0; i < N; i++)
          {
             if ((clients[i] != NULL) && (clients[i]->client_id == header.senderid))
             {
               clients[i]->finished = 1;
             }
          }
        }
      }
    }

    /*
    * Then, tries to deliver the next packet to each connected client that can
    * accept a packet. It also checks if a connection termination packet
    * has been received after sending the empty packet.
    * Here the responsibility of the server is to fetch the right payload from the
    * file for each client. Everything else happens via RDP. It is RDP that creates
    * the packets and impelements the stop and wait.
    */
    for (i = 0; i < N; i++)
    {
       if ((clients[i] != NULL) && (clients[i]->finished != 1))
       {
         ssize_t bytes_sent;
         unsigned char seq = clients[i]->last_sent;
         unsigned int file_location = clients[i]->total_packets;


         /* If it is the last data packet, create a packet with less than 999 bytes */
         if (file_location == n - 1)
         {
           int remaining_payload = payload_size - (999 * (n-1));
           ssize_t remaining_packet_size = remaining_payload + 16;
           char* data = malloc(remaining_payload);
           fseek(file, (file_location)*999, SEEK_SET);
           fread(data, 1, remaining_payload, file);

           char * remaining_packet = rdp_createPacket(clients[i]->client_id, seq, data, remaining_payload, serverID);
           bytes_sent = rdp_sendPacket(socket_fd, remaining_packet, remaining_packet_size, clients[i], clients, N);
           if (bytes_sent == -1 )
           {
             perror("Failure while sending packet");
             rdp_close(socket_fd);
             freeAll();
             fclose (file);
             free(data);
             free(remaining_packet);
             exit(EXIT_FAILURE);
           }
           free(data);
           free(remaining_packet);
         }
         /* Check if an empty packet should be sent */
         else if(file_location == n)
         {
           struct rdp_header empty = {
             .flags = RDP_DATA,
             .pktseq = file_location,
             .ackseq = 0,
             .unassigned = 0,
             .senderid = htonl(serverID),
             .recvid = htonl(clients[i]->client_id),
             .metadata = htonl(0)
           };

           bytes_sent = rdp_sendPacket(socket_fd, (char*) &empty, RDP_HEADER_SIZE, clients[i], clients, N);

           if (bytes_sent == -1 )
           {
             perror("Failure while sending packet");
             rdp_close(socket_fd);
             freeAll();
             fclose (file);
             exit(EXIT_FAILURE);
           }
           /*
           * If ack for empty packet has been received (and no connection termination
           * in RDP sendPacket), check for connection termination.
           */
           if((clients[i]->total_packets == n + 1) && (clients[i]->finished != 1))
           {
             struct rdp_header header = {0};

             ssize_t bytes_received = rdp_clientResponse(socket_fd, &header, clients[i]);
             if(bytes_received == -1)
             {
               perror("Failure while receiving client response.\n");
               rdp_close(socket_fd);
               freeAll();
               fclose(file);
               exit(EXIT_FAILURE);
             }
             else if ((header.flags == RDP_CONNECTION_TERMINATION) && (bytes_received == -2))
             {
              printf("Connection termination packet invalid.\n");
              printf("However, client gets disconnected anyway.\n");
              printf("DISCONNECTED <%d> <%d> \n\n", header.senderid, header.recvid);
              clients[i]->finished = 1;
              terminated++;
             }

             if (header.flags == RDP_CONNECTION_TERMINATION)
             {
               header.senderid = ntohl(header.senderid);
               header.recvid = ntohl(header.recvid);
               printf("***** Received connection termination from client with id nr: %d *****\n\n", header.senderid);
               printf("DISCONNECTED <%d> <%d> \n\n", header.senderid, header.recvid);
               clients[i]->finished = 1;
               terminated++;
             }
           }
           /*
           * Check if a connection termination has been received in RDP sendPacket
           * if yes, increment terminated clients.
           */
           else if(clients[i]->finished == 1)
           {
             terminated++;
           }
         }
         else
         {
           char* data = malloc(999);
           fseek(file, (file_location)*999, SEEK_SET);
           fread(data, 1, 999, file);
           char* packet = rdp_createPacket(clients[i]->client_id, seq, data, 999, serverID);
           bytes_sent = rdp_sendPacket(socket_fd, packet, 1015, clients[i], clients, N);
           if (bytes_sent == -1 )
           {
             perror("Failure while sending packet");
             rdp_close(socket_fd);
             freeAll();
             fclose (file);
             free(data);
             free(packet);
             exit(EXIT_FAILURE);
           }
           free(data);
           free(packet);
         }
       }
     }
     /* Check if all clients have terminated */
     if (terminated == N)
       {
         printf("All clients have terminated their connection. Server is terminating. \n");
         running = 0;
       }
   }

   /* In the end, free all allocated memory and close the file */
    freeAll();

    fclose(file);

    exit(EXIT_SUCCESS);
}
