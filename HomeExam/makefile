CC = gcc
CFLAGS = -g -ggdb -Wall -Wextra

BINARIES = client server

CLIENT = client
SERVER = server

all: $(BINARIES)


server: server.c protocol.c send_packet.c
	$(CC) $(CFLAGS) $^ -o $@


client: client.c protocol.c send_packet.c
	$(CC) $(CFLAGS) $^ -o $@


run_client:
	./$(CLIENT) localhost 2021 0.3


run_server:
	./$(SERVER) 2021 paxoi.jpg 3 0.3


clean:
	rm -f $(BINARIES)


valgrind_client:
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --trace-children=yes ./client 129.240.65.61 2021 0.3


valgrind_server:
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --trace-children=yes ./server 2021 paxoi.jpg 3 0.3
