#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define PORT 8888
typedef struct
{
    int client_socket;
    struct sockaddr_in client_address;
} ClientInfo;

typedef struct
{
    ClientInfo client_info;
    pthread_t thread_id;
    pthread_mutex_t mutex;
} ThreadData;

typedef struct
{
    int socket;
    ThreadData thread_data[MAX_CLIENTS];
    int num_clients;
    pthread_mutex_t mutex;
} ServerData;

void *handleClient(void *arg);
void cleanup(ServerData *server_data);

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;
    ServerData server_data;

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Bind server socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Error binding server socket");
        exit(EXIT_FAILURE);
    }

    // Listen for client connections
    if (listen(server_socket, 5) == -1)
    {
        perror("Error listening for client connections");
        exit(EXIT_FAILURE);
    }

    // Initialize server data
    server_data.socket = server_socket;
    server_data.num_clients = 0;
    pthread_mutex_init(&server_data.mutex, NULL);

    printf("Server started. Listening for client connections...\n");

    // Accept client connections and create threads to handle each client
    while (1)
    {
        client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1)
        {
            perror("Error accepting client connection");
            continue;
        }

        // Create thread to handle client
        pthread_t tid;
        ThreadData *thread_data = &(server_data.thread_data[server_data.num_clients]);
        thread_data->client_info.client_socket = client_socket;
        thread_data->client_info.client_address = client_address;
        pthread_mutex_init(&(thread_data->mutex), NULL);
        if (pthread_create(&tid, NULL, handleClient, (void *)&server_data) != 0)
        {
            perror("Error creating client thread");
            close(client_socket);
        }
        else
        {
            thread_data->thread_id = tid;
            pthread_detach(tid);
            pthread_mutex_lock(&server_data.mutex);
            server_data.num_clients++;
            pthread_mutex_unlock(&server_data.mutex);
        }
    }

    // Cleanup and close server socket
    cleanup(&server_data);
    close(server_socket);

    return 0;
}

void *handleClient(void *arg)
{
    ServerData *server_data = (ServerData *)arg;
    ThreadData *thread_data = NULL;
    int client_socket;
    struct sockaddr_in client_address;

    client_socket = server_data->thread_data[server_data->num_clients - 1].client_info.client_socket;
    client_address = server_data->thread_data[server_data->num_clients - 1].client_info.client_address;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);

    printf("Client connected: %s\n", client_ip);

    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes_received] = '\0';

        // Acquire mutex lock before printing
        pthread_mutex_lock(&(server_data->mutex));

        printf("Received message from %s: %s\n", client_ip, buffer);


        // Release mutex lock after printing
        pthread_mutex_unlock(&(server_data->mutex));

        // Broadcast message to other clients
        pthread_mutex_lock(&(server_data->mutex));
        for (int i = 0; i < server_data->num_clients; i++)
        {
            if (server_data->thread_data[i].client_info.client_socket != client_socket)
            {
                send(server_data->thread_data[i].client_info.client_socket, buffer, strlen(buffer), 0);
                send(server_data->thread_data[i].client_info.client_socket, "\n", 1, 0); // Add a newline after the message
            }
        }
        pthread_mutex_unlock(&(server_data->mutex));
    }

    printf("Client disconnected: %s\n", client_ip);

    close(client_socket);

    pthread_exit(NULL);
}

void cleanup(ServerData *server_data)
{
    for (int i = 0; i < server_data->num_clients; i++)
    {
        ThreadData *thread_data = &(server_data->thread_data[i]);
        if (thread_data->client_info.client_socket != -1)
        {
            close(thread_data->client_info.client_socket);
        }
        pthread_mutex_destroy(&(thread_data->mutex));
    }
    pthread_mutex_destroy(&(server_data->mutex));
}
