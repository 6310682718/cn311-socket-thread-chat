#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>

#define BUFFER_SIZE 1024
#define PORT 8888
typedef struct
{
    int socket;
    GtkWidget *text_view;
    GtkWidget *entry;
    gchar *received_message; // New field to store received message
    GtkWidget *name_entry;
    // char name[100];
} ClientData;

void *receiveMessages(void *arg);
void sendDirectMessage(GtkWidget *widget, gpointer data);
void sendGroupMessage(GtkWidget *widget, gpointer data);
void appendMessage(GtkWidget *text_view, const gchar *message);

gboolean appendMessageIdle(gpointer data);
void freeClientData(ClientData *client_data);

int main(int argc, char *argv[])
{
    GtkWidget *window, *vbox, *hbox, *button_direct, *button_group, *scroll, *text_view, *entry;
    GtkTextBuffer *buffer;
    ClientData *client_data = (ClientData *)malloc(sizeof(ClientData));
    pthread_t tid;

    gtk_init(&argc, &argv);

    // Create window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create VBox
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create buttons
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    button_direct = gtk_button_new_with_label("Direct Message");
    button_group = gtk_button_new_with_label("Group Chat");
    gtk_box_pack_start(GTK_BOX(hbox), button_direct, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), button_group, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    // Create text view
    scroll = gtk_scrolled_window_new(NULL, NULL);
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_container_add(GTK_CONTAINER(scroll), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    // Create name entry
    GtkWidget *name_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, TRUE, TRUE, 0);

    // Create entry
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), entry, TRUE, TRUE, 0);

    // Set client data
    client_data->socket = -1;
    client_data->text_view = text_view;
    client_data->entry = entry;
    client_data->received_message = NULL;
    client_data->name_entry = name_entry; // Assign the name_entry widget


    // Connect button signals
    g_signal_connect(button_direct, "clicked", G_CALLBACK(sendDirectMessage), (gpointer)client_data);
    g_signal_connect(button_group, "clicked", G_CALLBACK(sendGroupMessage), (gpointer)client_data);

    // Show all widgets
    gtk_widget_show_all(window);

    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        perror("Error creating socket");
        return 1;
    }

    // Connect to the server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &(server_address.sin_addr)) <= 0)
    {
        perror("Invalid address/ Address not supported");
        return 1;
    }

    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to the server\n");

    // Update the client data
    client_data->socket = socket_fd;
    // printf("Enter your name: ");
    // fgets(client_data->name, sizeof(client_data->name), stdin);
    // client_data->name[strcspn(client_data->name, "\n")] = '\0';  // Remove trailing newline character

    // Create thread to receive messages
    pthread_create(&tid, NULL, receiveMessages, (void *)client_data);

    // Start the GTK main loop
    gtk_main();

    // Clean up
    freeClientData(client_data);

    return 0;
}

void *receiveMessages(void *arg)
{
    ClientData *client_data = (ClientData *)arg;
    int socket = client_data->socket;
    GtkWidget *text_view = client_data->text_view;

    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received < 0)
        {
            perror("Error receiving message");
            break;
        }
        else if (bytes_received == 0)
        {
            appendMessage(text_view, "Server disconnected\n");
            break;
        }

        buffer[bytes_received] = '\0';

        // Allocate memory for the received message
        client_data->received_message = g_strdup(buffer);

        // Update the GUI from the main thread
        g_idle_add(appendMessageIdle, client_data);
    }

    close(socket);
    client_data->socket = -1;
    pthread_exit(NULL);
}

void sendDirectMessage(GtkWidget *widget, gpointer data)
{
    ClientData *client_data = (ClientData *)data;
    int socket = client_data->socket;
    GtkWidget *entry = client_data->entry;
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(entry));

    if (socket == -1)
    {
        appendMessage(client_data->text_view, "Not connected to a server\n");
        return;
    }

    if (strlen(message) == 0)
    {
        return;
    }

    if (send(socket, message, strlen(message), 0) < 0)
    {
        perror("Error sending message");
    }

    // Update client data with sent message
    appendMessage(client_data->text_view, message);
    appendMessage(client_data->text_view, "\n");

    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

void sendGroupMessage(GtkWidget *widget, gpointer data)
{
    ClientData *client_data = (ClientData *)data;
    int socket = client_data->socket;
    GtkWidget *entry = client_data->entry;
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(entry));
    const gchar *name = gtk_entry_get_text(GTK_ENTRY(client_data->name_entry)); // Retrieve the user's name

    if (socket == -1)
    {
        appendMessage(client_data->text_view, "Not connected to a server\n");
        return;
    }

    if (strlen(message) == 0)
    {
        return;
    }

    // Modify the message to indicate it's a group chat message
    char modified_message[BUFFER_SIZE];
    snprintf(modified_message, BUFFER_SIZE, "[Group] %s: %s", name, message);

    if (send(socket, modified_message, strlen(modified_message), 0) < 0)
    {
        perror("Error sending message");
    }

    // Update client data with sent message
    appendMessage(client_data->text_view, modified_message);
    appendMessage(client_data->text_view, "\n");

    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

void appendMessage(GtkWidget *text_view, const gchar *message)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end;

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);
}

gboolean appendMessageIdle(gpointer data)
{
    ClientData *client_data = (ClientData *)data;
    GtkWidget *text_view = client_data->text_view;
    gchar *message = client_data->received_message;

    appendMessage(text_view, message);

    // Free the allocated memory for the received message
    g_free(message);
    client_data->received_message = NULL;

    return FALSE;
}

void freeClientData(ClientData *client_data)
{
    if (client_data != NULL)
    {
        // Free the allocated memory for the received message
        if (client_data->received_message != NULL)
        {
            g_free(client_data->received_message);
        }

        // Free the client data structure
        free(client_data);
    }
}
