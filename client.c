#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 10001
#define CONN_PORT 10000
#define SERVER_ADDRESS "127.0.0.1"
#define BUFFER_SIZE 1024

void handle_response(int client_socket);

int main() {
	int client_socket;
	struct sockaddr_in server_address;

	if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Error in socket()");
		exit(EXIT_FAILURE);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
	server_address.sin_port = htons(SERVER_PORT);

	if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
		perror("Error in connect()");
		exit(-1);
	}

	handle_response(client_socket);

	return 0;
}

void handle_response(const int client_socket) {
	char buffer[BUFFER_SIZE] = {0};
	int bytes_read;

	printf("Verrai connesso al server il prima possibile\n");
	printf("CTRL-C per terminare\n\n");

	while (1) {
		if ((bytes_read = read(client_socket, buffer, BUFFER_SIZE)) <= 0)
			break;

		buffer[bytes_read] = '\0';
		printf("%s", buffer);

		fgets(buffer, BUFFER_SIZE, stdin);
		send(client_socket, buffer, strlen(buffer), 0);
	}
	printf("%s", "Disconnesso dal server");
}