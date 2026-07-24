CC = gcc
CFLAGS = -Wall -Wextra -g
all: hw1_client hw1_server hw2_client hw2_server

hw1_client: hw1_client.c
	$(CC) $(CFLAGS) -o hw1_client hw1_client.c

hw1_server: hw1_server.c
	$(CC) $(CFLAGS) -o hw1_server hw1_server.c

hw2_client: hw2_client.c
	$(CC) $(CFLAGS) -o hw2_client hw2_client.c

hw2_server: hw2_server.c
	$(CC) $(CFLAGS) -o hw2_server hw2_server.c

clean:
	rm -rf hw1_client hw1_server hw2_client hw2_server *.dSYM