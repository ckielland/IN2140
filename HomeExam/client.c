#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>

#include "send_packet.h"
#include "protocol.h"


int client_id;
char * port;
char *server_ip;
unsigned char seq = 0;

int main(int argc, char *argv[]){

  /* Check command line arguments */
    if(argc < 4)
    {
      fprintf(stderr, "usage : %s <ip address/hostname> <port> <probability loss>\n", argv[0]);
      exit(EXIT_FAILURE);
    }

    server_ip = argv[1];
    port = argv[2];
    float drop_percentage = atof(argv[3]);
    set_loss_probability(drop_percentage);

   /* Set up and get the socket_fd from the RDP protocol */
   struct sockaddr_in server_addr = { 0 };
   socklen_t server_len = sizeof(server_addr);

   int socket_fd = rdp_clientSocket(server_ip, port, &server_addr, &server_len);

   if (socket_fd == -1)
   {
      fprintf(stderr, "Failure while configuring socket.");
      exit(EXIT_FAILURE);
    }

    /*Create a random client ID*/
    srand(time(0));
    client_id = rand();
    printf("Hello! I am client id: %d\n",client_id );

    unsigned int server_id = 0;

    /*
    * Send connection request to server. Wait for 1 sec, if no
    * answer is received, terminate.
    */
     if (rdp_connect(socket_fd, client_id, server_addr, server_len) == -1)
    {
      perror ("Failure while sending connection request.\n");
      rdp_close (socket_fd);
      exit (EXIT_FAILURE);
    }

    struct timeval tv = {1, 0};
    fd_set file_descriptors;

    FD_ZERO(&file_descriptors);
    FD_SET(socket_fd, &file_descriptors);
    int ret = select(socket_fd +1, &file_descriptors, NULL, NULL, &tv);

    struct rdp_header header = {0};

    ssize_t message;
    if(ret == -1){
          perror("Failure while setting up select");
          exit(EXIT_FAILURE);
      }

    else if(ret > 0)
    {
   /* Receive connection request response from server */
   message = rdp_serverResponse(socket_fd, &header, server_addr, server_len);

   if (message == -1)
   {
     perror ("Failure while receiving server response.\n");
     rdp_close (socket_fd);
     exit (EXIT_FAILURE);
   }
   else if (message == -2)
   {
     printf("Invalid header flags -- discarding message and closing connection\n");
     rdp_closeConnection(socket_fd, client_id, server_id, RDP_INVALID_FLAGS, server_addr, server_len);
     exit (EXIT_FAILURE);
   }
 }
 else{
   printf("No response to the connection request from the server within 1 sec.\n");
   printf("Exiting\n");
   rdp_closeConnection(socket_fd, client_id, server_id, RDP_DELAYED_RESPONSE, server_addr, server_len);
   exit(EXIT_SUCCESS);
 }

   if (header.flags == RDP_ACCEPT_REQUEST){
     server_id = header.senderid;
      printf("Connection request accepted!\n");
      printf("CONNECTED <%d> <%d> \n\n", client_id, server_id );

      /* Start receiving packets*/
      unsigned char received;

      char filename[30];
      sprintf(filename, "kernel-file-<%d>", client_id);

      int running = 1;
      /* While there are packets to receive */
      while(running) {
        char* payload = malloc(1023);

        ssize_t packet = rdp_receivePacket(socket_fd, payload, server_addr, server_len);
        payload[packet] = '\0';

        FILE *output_file;

        if((output_file = fopen(filename, "a")) == NULL)
        {
          perror("Error opening the output file");
          rdp_close (socket_fd);
          fclose(output_file);
          free(payload);
          exit(EXIT_FAILURE);
        }

        if (packet == -1)
        {
          perror("Failure while receiving a packet.\n");
          rdp_close(socket_fd);
          fclose(output_file);
          free(payload);
          exit(EXIT_FAILURE);
        }

        unsigned char flag = 0;
        unsigned int metadata = 0;
        ssize_t bytes_sent;

        memcpy(&(flag), &(payload[0]), sizeof(unsigned char));
        memcpy(&(metadata), &(payload[12]), sizeof(unsigned int));
        /* The ACK packet contains the seq number of the last received packet */
        memcpy(&(received), &(payload[1]), sizeof(unsigned char));

        /*
        * If bytes received are equal to -2, then the header flag contains
        * more than 1 bit that are set and therefore the packet needs to be discarded.
        * No ack is sent neither is the packet written in file.
        */
        if (packet == -2)
        {
          printf("Invalid header flag -- packet gets discarded.\n");
        }
        /* Check for empty packet */
        else if ((flag == 4) && (metadata == 0))
        {
          printf("****** Received last packet! ***** \n");

          ssize_t bytes_sent = rdp_sendACK(socket_fd, received, client_id, server_id, server_addr, server_len);
          if (bytes_sent == -1)
          {
            fclose(output_file);
            free(payload);
            perror("Failure while sending ack.");
            exit(EXIT_FAILURE);
          }

          ssize_t close = rdp_closeConnection(socket_fd, client_id, server_id, RDP_READY_CLIENT, server_addr, server_len);
          if (close == -1)
          {
            perror("Failure while closing the connection. \n");
            rdp_close (socket_fd);
            free(payload);
            fclose(output_file);
            exit(EXIT_FAILURE);
          }
          printf("File succesfully written: %s\n", filename );
          free(payload);
          fclose(output_file);
          exit(EXIT_SUCCESS);
        }

        else if(received == seq)
        {
          bytes_sent = rdp_sendACK(socket_fd, received, client_id, server_id, server_addr, server_len);
          if (bytes_sent == -1)
          {
            fclose(output_file);
            free(payload);
            perror("Failure while sending ack.");
            exit(EXIT_FAILURE);
          }
          seq++;
          fwrite(&(payload[16]), 1, packet-16, output_file);
          fseek(output_file, 0, SEEK_END);
        }

        else if((received == seq -1) || ((received == 255) && (seq == 0)))
        {
          printf("The packet has been already received.\n");
          printf("Resending ACK, but do not write in file.\n");
          bytes_sent = rdp_sendACK(socket_fd, received, client_id, server_id, server_addr, server_len);
          if (bytes_sent == -1)
          {
            fclose(output_file);
            free(payload);
            perror("Failure while sending ack.");
            exit(EXIT_FAILURE);
          }
        }

        fclose(output_file);
        free(payload);
      }
    }

    /* In case of a connection request denial, the client terminates */
  else if (header.flags == RDP_REFUSE_REQUEST)
  {
  printf("Connection request denied. Exiting\n");
  rdp_close(socket_fd);
  exit(EXIT_SUCCESS);
  }

}
