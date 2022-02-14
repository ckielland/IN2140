#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/time.h>
#include <netdb.h>

#include "protocol.h"
#include "send_packet.h"


int rdp_serverSocket(char *server_port)
{
  /* Create hints about socket type and a linked list of addringo structs */
  struct addrinfo hints;
  struct addrinfo *res;

  memset(&hints, 0, sizeof(hints));

  /*Initialize hints structs. Use IPv4 protocol and UDP stream socket */
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int rc = getaddrinfo(NULL, server_port, &hints, &res);

  if (rc != 0)
  {
    fprintf(stderr, "getaddrinfo error : %s\n", gai_strerror(rc));
    return -1;
  }

  /* Create a socket_fd and bind it to the port*/
  int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  if (socket_fd == -1)
  {
     perror("Failure while creating socket. \n");
     freeaddrinfo(res);
     return -1;
  }

  int ret = bind(socket_fd, (struct sockaddr *)res->ai_addr, res->ai_addrlen);

  if (ret == -1)
  {
     perror("Failure while binding socket. \n");
     freeaddrinfo(res);
     return -1;
   }

  freeaddrinfo(res);

  return socket_fd;
}



int rdp_clientSocket(char *server_ip, char *server_port, struct sockaddr_in *host, socklen_t *len)
{
  struct addrinfo hints;
  struct addrinfo *res;

  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  int rc = getaddrinfo(server_ip, server_port, &hints, &res);

   if (rc != 0)
   {
       fprintf(stderr, "getaddrinfo error : %s\n", gai_strerror(rc));
       return -1;
   }

   int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (socket_fd == -1)
    {
        perror("Failure while creating socket");
        freeaddrinfo(res);
        return -1;
    }

   memcpy(host, res->ai_addr, sizeof(struct sockaddr_in));
   memcpy(len, &res->ai_addrlen, sizeof(socklen_t));

   freeaddrinfo(res);

   return socket_fd;
}



int rdp_connect(int socket_fd, int client_id,  struct sockaddr_in server_addr, socklen_t server_len)
{
  /*Adjust the header structure */
  struct rdp_header header = {
    .flags = RDP_CONNECTION_REQUEST,
    .pktseq = 0,
    .ackseq = 0,
    .unassigned = 0,
    .senderid = htonl(client_id),
    .recvid = htonl(0), //Flags is set to 0x01, so server's ID is 0
    .metadata = htonl(0)
  };

  printf("***** Sending connection request ******\n");
  ssize_t bytes_sent = sendto(socket_fd, &header, RDP_HEADER_SIZE, 0, (struct sockaddr*) &server_addr, server_len);
  if (bytes_sent == -1) return -1;
  return 0;
}



struct rdp_connection* rdp_accept(int socket_fd, struct rdp_header *header, struct rdp_connection **clients, int N)
{
  struct sockaddr_in client_addr;
  socklen_t client_len;

  client_len = sizeof(struct sockaddr_in);

  memset(&client_addr, 0, client_len);

  ssize_t bytes_received = recvfrom(socket_fd, header, RDP_HEADER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len);

  if (bytes_received == -1)
    {
      fprintf(stderr, "Unexpected failure when receiving the connection request. \n");
      rdp_close (socket_fd);
      exit (EXIT_FAILURE);
    }
  else if (bytes_received == 0)
    {
      fprintf(stderr, "No message to receive. \n");
      return NULL;
    }

  if(rdp_checkFlags(header->flags) == -1)
    {
      printf("Invalid header flag -- packet gets discarded. \n");
      return NULL;
    }

   header->senderid = ntohl(header->senderid);
   header->recvid = ntohl(header->recvid);

  if (header->flags == RDP_CONNECTION_REQUEST){
    printf("New connection request received.\n");

    /*
    * Check if a connection/client with the same ID already exists.
    * Here I assumed that it is best that the RDP checks for the IDs (and not the server),
    * as these are part of the RDP connection/multiplexing that the RDP is responsible for.
    */
    for (int i = 0; i < N; i++)
    {
      if (clients[i] != NULL && clients[i]->finished != 1)
      {
        if (clients[i]->client_id == header->senderid)
        {
          int d = rdp_deny(socket_fd, header->senderid, 100, RDP_EXISTING_CLIENT, (struct sockaddr*) &client_addr, client_len);
          if (d == -1)
          {
            perror("Connection denial failed. \n");
            rdp_close(socket_fd);
            exit(EXIT_FAILURE);
          }
          else if (d == 0)
          {
            printf("A client with the same id already exists.\n");
            return NULL;
          }
          break;
        }
      }
    }

    /* Create a new rdp connection struct */
    struct rdp_connection* connection = malloc(RDP_CONNECTION_SIZE);

    connection->socket_fd = socket_fd;
    connection->client_id = header->senderid;
    connection->last_ack = 0;
    connection->last_sent = 0;
    connection->address = client_addr;
    connection->len = client_len;
    connection->finished = 0;
    connection->total_packets = 0;
    return connection;
    }

  /*
  * At times, ACK packets are not picked up by the RDP_sendPacket method and
  * instead arrive in the socket_fd. Therefore, this needs to be checked
  * and handled.
  */
  else if (header->flags == RDP_ACK)
  {
    return NULL;
  }
  /*
  * Check if a connection termination has arrived, for example due to
  * server's delayed response in connection request
  */
  else if(header->flags == RDP_CONNECTION_TERMINATION)
  {
    printf("***** Received connection termination from client with id nr: %d *****\n\n", header->senderid);
    return NULL;
  }
  else
  {
    printf("Unexpected message received.\n");
    return NULL;
  }
}



int rdp_confirm(int socket_fd, int client_id, int server_id, struct sockaddr* client_addr, socklen_t len)
{
  struct rdp_header header = {
    .flags = RDP_ACCEPT_REQUEST,
    .pktseq = 0,
    .ackseq = 0,
    .unassigned = 0,
    .senderid = htonl(server_id),
    .recvid = htonl(client_id),
    .metadata = htonl(0)
  };


   ssize_t bytes_sent = sendto (socket_fd, &header, RDP_HEADER_SIZE, 0, client_addr, len);
   if (bytes_sent == -1) return -1;
   printf("CONNECTED <%d> <%d>\n\n", client_id, server_id);
  return 0;
}



int rdp_deny (int socket_fd, int client_id, int server_id, int metadata, struct sockaddr* client_addr, socklen_t len)
{
  struct rdp_header header = {
    .flags = RDP_REFUSE_REQUEST,
    .pktseq = 0,
    .ackseq = 0,
    .unassigned = 0,
    .senderid = htonl(server_id),
    .recvid = htonl(client_id),
    .metadata = htonl(metadata)
  };

  ssize_t bytes_sent = sendto (socket_fd, &header, RDP_HEADER_SIZE, 0, client_addr, len);
  if (bytes_sent == -1) return -1;
  printf("Cannot accept connection request. \n");
  printf("NOT CONNECTED <%d> <%d>\n\n", client_id, server_id);
  return 0;
}



int rdp_serverResponse(int socket_fd, struct rdp_header * header, struct sockaddr_in server_addr, socklen_t server_len)
{

  printf("***** Receiving server response ******\n\n");

  ssize_t bytes_received = recvfrom(socket_fd, header, sizeof(struct rdp_header), 0, (struct sockaddr *) &server_addr, &server_len);

  if(rdp_checkFlags(header->flags) == -1)
  {
    printf("Invalid header flag -- packet gets discarded. \n");
    return -2;
   }

  header->senderid = ntohl(header->senderid);
  header->recvid = ntohl(header->recvid);
  header->metadata = ntohl(header->metadata);

  if (bytes_received == -1)
   {
     return -1;
   }
  return 0;
}



int rdp_checkFlags(unsigned char flags){
  if ((flags & (flags-1)) != 0)
  {
    fprintf(stderr, "Header flags contains more than 1 bit set to 1.\n\n" );
    return -1;
  }
  return 0;
}



int rdp_sendPacket(int socket_fd, char * packet, int packet_size, struct rdp_connection* client, struct rdp_connection** clients, int N){
  struct sockaddr_in client_addr = client->address;
  socklen_t len = client->len;
  ssize_t bytes_sent;

  /*
  * Checks if the packet can be sent, or if it is the
  * first packet to be sent -- i.e. no ack required before sending
  */
  if ((client->last_sent -1 == client->last_ack) || (client->total_packets == 0)
  ||((client->last_sent == 0) && (client->last_ack == 255)))
  {
    printf("***** Sending packet to client_id nr: %d *****\n\n", client->client_id);
    bytes_sent = send_packet(socket_fd, packet, packet_size, 0, (struct sockaddr*) &client_addr, len);
    if(bytes_sent == -1) return -1;

    client->last_sent++;
    client->total_packets++;

    /*Setting waiting time to 100 ms */
    struct timeval tv;
    tv.tv_sec  = 100 / 1000;
    tv.tv_usec = (100 % 1000) * 1000;


    fd_set file_descriptor;
     FD_ZERO(&file_descriptor);
     FD_SET(socket_fd, &file_descriptor);
    int ret = select(socket_fd + 1, &file_descriptor, NULL, NULL, &tv);

    if(ret == -1)
      {
        perror("Failure in select.");
        exit(EXIT_FAILURE);
      }
    else if(ret > 0)
    {
      /*Need to check that an ack is received */
      struct rdp_header ack = {0};
      ssize_t ack_bytes = recvfrom(socket_fd, &ack, sizeof(struct rdp_header), 0, (struct sockaddr*) &client_addr, &len);

      if (ack_bytes == -1)
      {
        perror("Receiving ack failed.");
        exit(EXIT_FAILURE);
      }

      if(rdp_checkFlags(ack.flags) == -1)
      {
        printf("Invalid header flag -- ack packet gets discarded.\n");
        client->last_sent--;
        client->total_packets--;
        return 0;
      }

      ack.senderid = ntohl(ack.senderid);
      ack.recvid = ntohl(ack.recvid);
      ack.metadata = ntohl(ack.metadata);

      /*
      * First check that the packet originates from the client that
      * has called rdp_sendPacket. (I assume that due to the implemented timeouts
      * (both here and in the server) I have noticed that sometimes here are received packets
      * from other clients.
      */
      if(ack.senderid == client->client_id)
      {
        if (ack.flags == RDP_ACK)
        {
          client->last_ack = ack.ackseq;
          printf("***** Received ack packet from client id nr: %d *****\n\n", ack.senderid);
        }
        /*
        * Even if ack for the last packet is dropped, server should still be able
        * to receive a connection termination packet from client.
        */
        else if (ack.flags == RDP_CONNECTION_TERMINATION)
        {
          printf("***** Received connection termination from client with id nr: %d *****\n", ack.senderid);
          printf("DISCONNECTED <%d> <%d> \n\n", ack.senderid, ack.recvid);
          client->finished = 1;
        }
      }
      else if(ack.senderid != client->client_id)
      {
        client->last_sent--;
        client->total_packets--;
        bytes_sent = 0;

        for (int i = 0; i < N; i++)
        {
          if (clients[i] != NULL)
          {
            if (clients[i]->client_id == ack.senderid)
             {
               if(ack.flags == RDP_ACK)
               {
                  clients[i]->last_ack = ack.ackseq;
                  clients[i]->last_sent++;
                  clients[i]->total_packets++;
                  printf("***** Received ack packet from client id nr: %d *****\n\n", ack.senderid);
               }
               else if (ack.flags == RDP_CONNECTION_TERMINATION)
               {
                 printf("***** Received connection termination from client with id nr: %d *****\n", ack.senderid);
                 printf("DISCONNECTED <%d> <%d> \n\n", ack.senderid, ack.recvid);
                 clients[i]->finished = 1;
               }
              }
            }
          }
        }
      }
    else
    {
      printf("No ack received.");
      printf("The packet needs to be resent in the next round.\n\n");
      client->last_sent--;
      client->total_packets--;
      bytes_sent = 0;
    }
  }
  else
  {
    printf("Cannot send packet in this round due to missing ack.\n");
    bytes_sent = 0;
  }
  return bytes_sent;
}



char* rdp_createPacket(int client_id, unsigned char seq, char * data, int payload_size, int server_id)
{

  int packet_size = payload_size + RDP_HEADER_SIZE;

  struct rdp_header* header = malloc(RDP_HEADER_SIZE);
   header->flags = RDP_DATA; //flags is set to 0x04 so the packet can contain a payload
   header->pktseq = seq;
   header->ackseq = 0;
   header->unassigned = 0;
   header->senderid = htonl(server_id);
   header->recvid = htonl(client_id);
   header->metadata = htonl(packet_size);

  char* packet = malloc(packet_size);
  memcpy(&(packet[0]), &(header->flags), sizeof(unsigned char));
  memcpy(&(packet[1]), &(header->pktseq), sizeof(unsigned char));
  memcpy(&(packet[2]), &(header->ackseq), sizeof(unsigned char));
  memcpy(&(packet[3]), &(header->unassigned), sizeof(unsigned char));
  memcpy(&(packet[4]), &(header->senderid), sizeof(int));
  memcpy(&(packet[8]), &(header->recvid), sizeof(int));
  memcpy(&(packet[12]), &(header->metadata), sizeof(int));
  memcpy(&(packet[16]), data, payload_size);

  free(header);

  return packet;
}



int rdp_receivePacket(int socket_fd, char * data, struct sockaddr_in server_addr, socklen_t server_len)
{
  printf("***** Receiving packets from server ******\n");
  int packet_size = RDP_PAYLOAD_SIZE + RDP_HEADER_SIZE;

  ssize_t bytes_received = recvfrom(socket_fd, (char *) data, packet_size, 0, (struct sockaddr *) &server_addr, &server_len);

  if (bytes_received == -1)
   {
     perror("Failure while receiving packet from server.\n");
     exit(EXIT_FAILURE);
   }

   if(rdp_checkFlags(data[0]) == -1)
   {
     return -2;
   }

  data[4] = ntohl(data[4]);
  data[8] = ntohl(data[8]);
  data[12] = ntohl(data[12]);

  return bytes_received;
}



int rdp_sendACK(int socket_fd, unsigned char ack_seq, int client_id, int server_id, struct sockaddr_in server_addr, socklen_t server_len)
{
  struct rdp_header header = {
    .flags = RDP_ACK,
    .pktseq = 0,
    .ackseq = ack_seq,
    .unassigned = 0,
    .senderid = htonl(client_id),
    .recvid = htonl(server_id),
    .metadata = htonl(0),
      };

    printf("***** Sending ACK to server ***** \n\n");
    ssize_t bytes_sent = send_packet(socket_fd, (char*) &header, RDP_HEADER_SIZE, 0, (struct sockaddr*) &server_addr, server_len);

    if (bytes_sent == -1) return -1;
    return 0;
}



int rdp_closeConnection(int socket_fd, int client_id, int server_id, int metadata, struct sockaddr_in server_addr, socklen_t server_len)
{
  struct rdp_header header = {
    .flags = RDP_CONNECTION_TERMINATION,
    .pktseq = 0,
    .ackseq = 0,
    .unassigned = 0,
    .senderid = htonl(client_id),
    .recvid = htonl(server_id),
    .metadata = htonl(metadata),
      };

    printf("***** Connection is closing *****\n\n" );

    ssize_t bytes_sent = sendto(socket_fd, &header, RDP_HEADER_SIZE, 0, (struct sockaddr*) &server_addr, server_len);

    if (bytes_sent == -1) return -1;
    rdp_close(socket_fd);
    return 0;
}



int rdp_clientResponse(int socket_fd, struct rdp_header * header, struct rdp_connection* client)
{
  struct sockaddr_in client_addr = client->address;
  socklen_t len = client->len;

  printf("***** Receiving packet from client *****\n\n");

  ssize_t bytes_received = recvfrom(socket_fd, header, RDP_HEADER_SIZE, 0, (struct sockaddr*) &client_addr, &len);


  if(rdp_checkFlags(header->flags) == -1)
  {
    return -2;
   }

  if (bytes_received == -1) return -1;
  return 0;
}



int rdp_close (int socket_fd)
{
  if (close (socket_fd) == -1) return -1;
  return 0;
}
