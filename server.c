#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


#define CONNECTION_PORT 10001
#define SERVER_PORT 10001
#define SERVER_ADDRESS "127.0.0.1"
#define MAX_RECORDS 100
#define PASSWORD "secret123"
#define OUT_BUFFER_SIZE 1024
#define IN_BUFFER_SIZE 64

struct contact {
	char first_name[50];
	char last_name[50];
	char phone_number[20];
};

struct contact phone_book[MAX_RECORDS];
char pending_message[OUT_BUFFER_SIZE] = {0};
int contact_count = 0;

void handle_client(int connect_socket);
void view_all_contacts(int connect_socket);
void search_contacts(int connect_socket);
void add_contact(int connect_socket);
void remove_contacts(int connect_socket);
int authenticate(int connect_socket);
int check_contact(const struct contact *contact, const char *search_term);
int try_to_send_searched_contacts(const char *search_term, int connect_socket);
void remove_contacts_by_search_term(const char *search_term, int connect_socket);
void send_message(const char *message, int send_immediately, int connect_socket);
void read_message(char *buffer, int connect_socket);

int main() {
	int server_socket, connect_socket;
	socklen_t client_address_len;
	struct sockaddr_in server_address, client_address;

	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Error in server socket()");
		exit(EXIT_FAILURE);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_PORT);
	server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

	if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
		perror("Error in server socket bind()");
		exit(EXIT_FAILURE);
	}

	if (listen(server_socket, 5) == -1) {
		perror("Error in server socket listen()");
		exit(EXIT_FAILURE);
	}

	printf("\nServer ready (CTRL-C per terminare)\n");

	while (1) {
		client_address_len = sizeof(client_address);
		if ((connect_socket = accept(server_socket, (struct sockaddr *) &client_address,
					     &client_address_len)) == -1) {
			perror("Error in accept()");
			continue;
		}

		char *clientIP = inet_ntoa(client_address.sin_addr);
		printf("\nClient %s connects on socket %d\n", clientIP, connect_socket);

		pid_t pid = fork();
		if (pid < 0) {
			perror("Error in fork()");
			close(connect_socket);
			continue;
		}

		if (pid == 0) {
			// Processo figlio
			close(server_socket); // Chiudi il socket di ascolto nel processo figlio
			handle_client(connect_socket);
			close(connect_socket); // Chiudi il socket del client nel processo figlio
			exit(0); // Termina il processo figlio
		} else {
			// Processo padre
			close(connect_socket); // Chiudi il socket del client nel processo padre
			waitpid(pid, NULL, 0); // Aspetta che il processo figlio termini
		}
	}
}

void handle_client(int connect_socket) {
	const char *auth_menu = "1. Visualizza rubrica\n2. Cerca contatto\n3. Aggiungi contatto\n4. "
				"Rimuovi contatto\n";
	const char *menu = "1. Visualizza rubrica\n2. Cerca contatto\n3. Autenticati per modificare la "
			   "rubrica\n";
	char buffer[OUT_BUFFER_SIZE] = {0};
	int is_auth = 0, option;

	while (1) {
		send_message(is_auth ? auth_menu : menu, 1, connect_socket);
		read_message(buffer, connect_socket);
		option = atoi(buffer); // convert char to int

		switch (option) {
			case 1:
				view_all_contacts(connect_socket);
				break;
			case 2:
				search_contacts(connect_socket);
				break;
			case 3:
				if (is_auth)
					add_contact(connect_socket);
				else
					is_auth = authenticate(connect_socket);
				break;
			case 4:
				// if auth, 4 is a valid option, otherwise go to default
				if (is_auth) {
					remove_contacts(connect_socket);
					break;
				}
			default:
				send_message("Opzione non valida\n", 0, connect_socket);
				break;
		}
	}
}

void view_all_contacts(int connect_socket) {
	if (contact_count == 0) {
		send_message("Nessun contatto nella rubrica\n", 0, connect_socket);
		return;
	}

	char buffer[OUT_BUFFER_SIZE] = {0};
	for (int i = 0; i < contact_count; i++) {
		sprintf(buffer, "Nome: %s, Cognome: %s, Telefono: %s\n", phone_book[i].first_name,
			phone_book[i].last_name, phone_book[i].phone_number);
		send_message(buffer, 0, connect_socket);
	}
}

void search_contacts(int connect_socket) {
	if (contact_count == 0) {
		send_message("Nessun contatto nella rubrica\n", 0, connect_socket);
		return;
	}

	char search_term[OUT_BUFFER_SIZE] = {0};
	send_message("Inserisci nome, cognome o numero:\n", 1, connect_socket);
	read_message(search_term, connect_socket);

	if (!try_to_send_searched_contacts(search_term, connect_socket))
		send_message("Contatto non trovato\n", 0, connect_socket);
}

void remove_contacts(int connect_socket) {
	if (contact_count == 0) {
		send_message("Nessun contatto nella rubrica\n", 0, connect_socket);
		return;
	}

	char buffer[OUT_BUFFER_SIZE] = {0};
	char search_term[OUT_BUFFER_SIZE] = {0};

	send_message("Inserisci nome, cognome o numero di chi vuoi rimuovere:\n", 1, connect_socket);
	read_message(search_term, connect_socket);

	if (!try_to_send_searched_contacts(search_term, connect_socket)) {
		send_message("Contatto non trovato\n", 0, connect_socket);
		return;
	}

	send_message("Vuoi eliminare tutti questi contatti? (s/n)\n", 1, connect_socket);
	read_message(buffer, connect_socket);

	if (strcmp(buffer, "s") != 0) {
		send_message("Nessun contatto è stato rimosso\n", 0, connect_socket);
		return;
	}

	remove_contacts_by_search_term(search_term, connect_socket);
	send_message("Contatti rimossi con successo\n", 0, connect_socket);
}

int authenticate(int connect_socket) {
	char buffer[OUT_BUFFER_SIZE] = {0};

	send_message("Inserisci la password:\n", 1, connect_socket);
	read_message(buffer, connect_socket);

	if (strcmp(buffer, PASSWORD) == 0) {
		send_message("Autenticato con successo!\n", 0, connect_socket);
		return 1;
	}

	send_message("Password errata\n", 0, connect_socket);
	return 0;
}

void add_contact(int connect_socket) {
	if (contact_count >= MAX_RECORDS) {
		send_message("La rubrica è piena\n", 0, connect_socket);
		return;
	}
	struct contact c;

	send_message("Inserisci il nome:\n", 1, connect_socket);
	read_message(c.first_name, connect_socket);

	send_message("Inserisci il cognome:\n", 1, connect_socket);
	read_message(c.last_name, connect_socket);

	send_message("Inserisci il numero di telefono:\n", 1, connect_socket);
	read_message(c.phone_number, connect_socket);

	phone_book[contact_count++] = c;
	send_message("Contatto aggiunto con successo\n", 0, connect_socket);
}

int check_contact(const struct contact *contact, const char *search_term) {
	return strcmp(contact->first_name, search_term) == 0 || strcmp(contact->last_name, search_term) == 0 ||
	       strcmp(contact->phone_number, search_term) == 0;
}

int try_to_send_searched_contacts(const char *search_term, int connect_socket) {
	char buffer[OUT_BUFFER_SIZE] = {0};
	int found = 0;
	for (int i = 0; i < contact_count; i++) {
		if (!check_contact(&phone_book[i], search_term))
			continue;

		sprintf(buffer, "Nome: %s, Cognome: %s, Telefono: %s\n", phone_book[i].first_name,
			phone_book[i].last_name, phone_book[i].phone_number);
		send_message(buffer, 0, connect_socket);
		found = 1;
	}
	return found;
}

void remove_contacts_by_search_term(const char *search_term, int connect_socket) {
	struct contact new_phone_book[MAX_RECORDS];
	int new_contact_count = 0;

	for (int i = 0; i < contact_count; i++) {
		if (!check_contact(&phone_book[i], search_term))
			new_phone_book[new_contact_count++] = phone_book[i];
	}

	// Copia i nuovi contatti non eliminati nella rubrica originale
	for (int i = 0; i < new_contact_count; i++)
		phone_book[i] = new_phone_book[i];

	contact_count = new_contact_count;
}

void send_message(const char *message, const int send_immediately, int connect_socket) {
	if (!send_immediately) {
		strcat(pending_message, message); // Salva il messaggio in sospeso
		return;
	}
	char buffer[OUT_BUFFER_SIZE];

	// Se c'è un messaggio in sospeso, aggiungilo al buffer corrente
	if (strlen(pending_message) > 0) {
		snprintf(buffer, OUT_BUFFER_SIZE, "%s\n%s", pending_message, message);
		pending_message[0] = '\0'; // Clear the pending message buffer
	} else {
		snprintf(buffer, OUT_BUFFER_SIZE, "%s", message);
	}

	if (write(connect_socket, buffer, strlen(buffer)) < 0) {
		perror("Errore nella scrittura al socket");
		close(connect_socket);
		exit(0); // Exit the child process only
	}
	fsync(connect_socket); // Ensure the message is sent immediately
}

void read_message(char *buffer, int connect_socket) {
	const int nb = read(connect_socket, buffer, OUT_BUFFER_SIZE - 1);
	if (nb <= 0) {
		perror("Errore nella lettura dal socket o connessione chiusa");
		close(connect_socket);
		exit(0); // Exit the child process
	}
	buffer[nb] = '\0'; // Terminare la stringa correttamente
	buffer[strcspn(buffer, "\n")] = '\0'; // Sostituire /n con /0
}