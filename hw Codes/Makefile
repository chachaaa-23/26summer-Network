CC = gcc
CFLAGS = -Wall -Wextra -g
all: hw1_server hw2_server hw3_server hw4_server final_server hw1_client hw2_client hw3_client hw4_client final_client

hw1_server: hw1_server.c
	$(CC) $(CFLAGS) -o hw1_server hw1_server.c

hw2_server: hw2_server.c
	$(CC) $(CFLAGS) -o hw2_server hw2_server.c

hw3_server: hw3_server.c
	$(CC) $(CFLAGS) -o hw3_server hw3_server.c

hw4_server: hw4_server.c
	$(CC) $(CFLAGS) -o hw4_server hw4_server.c

final_server: final_server.c
	$(CC) $(CFLAGS) -o final_server final_server.c


hw1_client: hw1_client.c
	$(CC) $(CFLAGS) -o hw1_client hw1_client.c

hw2_client: hw2_client.c
	$(CC) $(CFLAGS) -o hw2_client hw2_client.c

hw3_client: hw3_client.c
	$(CC) $(CFLAGS) -o hw3_client hw3_client.c

hw4_client: hw4_client.c
	$(CC) $(CFLAGS) -o hw4_client hw4_client.c

final_client: final_client.c
	$(CC) $(CFLAGS) -o final_client final_client.c

clean:
	rm -rf hw1_client hw2_client hw3_client hw4_client final_client hw1_server  hw2_server hw3_server hw4_server final_server *.dSYM