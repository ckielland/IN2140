#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

/*Define some of the used constants: flags, payload, metadata  */
enum
{
    RDP_CONNECTION_REQUEST = 0x01,
    RDP_CONNECTION_TERMINATION = 0x02,
    RDP_DATA = 0x04,
    RDP_ACK = 0x08,
    RDP_ACCEPT_REQUEST = 0x10,
    RDP_REFUSE_REQUEST = 0x20,

    RDP_PAYLOAD_SIZE = 999,

    RDP_READY_CLIENT = 100,
    RDP_FULL_SERVER = 200,
    RDP_EXISTING_CLIENT = 300,
    RDP_DELAYED_RESPONSE = 400,
    RDP_UNKNOWN = 500,
    RDP_INVALID_FLAGS = 600
  };

/* Size of the RDP header (i.e. excluding the payload)*/
#define RDP_HEADER_SIZE (sizeof(unsigned int) *3) + (sizeof(unsigned char) * 4)

/* Size of the RDP connection struct */
#define RDP_CONNECTION_SIZE (sizeof(unsigned int)*3) + (sizeof(unsigned char)*3) + (sizeof (socklen_t)) + (sizeof(struct sockaddr_in))


/* Define the RDP header*/
struct rdp_header
{
  unsigned char flags;
  unsigned char pktseq;
  unsigned char ackseq;
  unsigned char unassigned;
  unsigned int senderid;
  unsigned int recvid;
  unsigned int metadata;
  char * payload;
} __attribute__ ((packed));

/* Define the RDP connection */
struct rdp_connection
{
  unsigned int socket_fd;
  unsigned int client_id;
  unsigned char last_ack;
  unsigned char last_sent;
  struct sockaddr_in address;
  socklen_t len;
  unsigned char finished;
  unsigned int total_packets; //In case of files requiring more than 255 packets. Keep track of file location.
} __attribute__ ((packed));


/*
* Creates a linked list of addrinfo structs, initializes the hints structs,
* and sets IPv4 as the protocol to be used. Sets up the UDP stream socket,
* creates and binds the socket file descriptor (socket_fd).
*
* Returns the socket_fd, otherwise -1 is returned and ERRNO is set.
*/
int rdp_serverSocket(char *server_port);

/*
* Creates a linked list of addrinfo structs, initializes the hints structs,
* and sets IPv4 as the protocol to be used. Sets up the UDP stream socket
* and creates the socket file descriptor (socket_fd).
*
* Returns the socket_fd, otherwise -1 is returned and ERRNO is set.
*/
int rdp_clientSocket(char *ipaddr, char *port, struct sockaddr_in *host, socklen_t *len);
/*
* Checks if a new network connection request has arrived, accepts it
* and creates a new rdp connection.
*
* Returns upon acceptance a pointer to an rdp connection struct,
* otherwise a NULL pointer.
*/
struct rdp_connection* rdp_accept(int socket_fd, struct rdp_header *header, struct rdp_connection **clients, int N);

/*
 * Sends a client connection request to the server.
 *
 * Returns 0 if the message is sent succesfully, otherwise -1 is
 * returned and ERRNO is set.
 */
int rdp_connect(int socket_fd, int client_id,  struct sockaddr_in server_addr, socklen_t server_len);

/*
 * Sends a server message indicating that the server has accepted the
 * client connection request.
 *
 * Returns 0 if the message is sent succesfully, otherwise -1 is
 * returned and ERRNO is set.
 */
int rdp_confirm(int socket_fd, int client_id, int server_id, struct sockaddr* client_addr, socklen_t len);


/*
 * Sends a server message indicating that the server has denied the client
 * connection request.
 *
 * Returns 0 if the message is sent succesfully, otherwise -1 is
 * returned and ERRNO is set.
 */
int rdp_deny(int socket_fd, int client_id, int server_id, int metadata, struct sockaddr* client_addr, socklen_t len);

/*
* Receives a server message as a response to a packet sent by the client.
* Checks the flag of the message and identifies the type of reponse.
*
* Returns 0 if the message is received succesfully, otherwise -1 is
* returned and ERRNO is set.
*/
int rdp_serverResponse(int socket_fd, struct rdp_header * header, struct sockaddr_in server_addr, socklen_t server_len);


/*
* Checks that in every packet only 1 of the bits in flags is set to 1.
*
* Returns 0 if the flags contains only 1 bit set to 1, otherwise -1
* is returned and an error message is printed.
*/
int rdp_checkFlags(unsigned char flags);

/*
* Receives a packet with payload sent by the server.
*
* Returns 0 if the message is received succesfully, otherwise -1 is
* returned and ERRNO is set.
*/
int rdp_sendPacket(int socket_fd, char * packet, int packet_size, struct rdp_connection* client, struct rdp_connection** clients, int N);

/*
* Creates a packet with payload to be later sent to the client.
*
* Returns a char pointer, otherwise a NULL-pointer.
*/
char* rdp_createPacket(int client_id, unsigned char seq, char * data, int payload_size, int server_id);


/*
* Receives a packet sent by the server to the socket_fd.
*
* Returns 0 if the message is received succesfully, otherwise -1 is
* returned and ERRNO is set.
*/
int rdp_receivePacket(int socket_fd, char * data, struct sockaddr_in server_addr, socklen_t server_len);

/*
* Sends a client message indicating an ACK sequence number
* of the last packet received to the server.
*
* Returns 0 if the message is received succesfully, otherwise -1 is
* returned and ERRNO is set.
*/
int rdp_sendACK(int socket_fd, unsigned char ack_seq, int client_id, int server_id, struct sockaddr_in server_addr, socklen_t server_len);

/*
* Sends a client message indicating that the client is closing
* the connection to the server and closes the SOCKET_FD.
*
* Returns 0 if the message is received succesfully, otherwise -1 is
* returned and ERRNO is set.
*/
int rdp_closeConnection(int socket_fd, int client_id, int server_id, int metadata, struct sockaddr_in server_addr, socklen_t server_len);

/*
* Receives a message from the client sent to the socket_fd.
*
* Returns 0 if the message is received succesfully, otherwise -1 is
* returned and ERRNO is set.
*/
int rdp_clientResponse(int socket_fd, struct rdp_header * header, struct rdp_connection* client);


/*
 * Closes the socket_fd.
 *
 * Returns 0 if the socket was closed succesfully, otherwise -1 is
 * returned and ERRNO is set.
 */
int rdp_close();


#endif
