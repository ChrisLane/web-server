#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>

#include "network.h"
#include "connection.h"

int initialSocket;
char *home;
pid_t pid;

/**
 * Check that the program arguments were correct.
 *
 * @param argc The number of arguments.
 * @param argv The array of arguments.
 */
void checkRunParams(int argc, char *argv[]) {
    // Check for the correct number of arguments
    if (argc != 3) {
        fprintf(stderr, "usage %s port homedir\n", argv[0]);
        exit(1);
    }

    // Convert port string to number
    size_t len = strlen(argv[1]) + 1;
    char portChar[len];
    strncpy(portChar, argv[1], len);
    portChar[len - 1] = '\0'; // Terminate the string...just in case
    char *ptr;
    long int port = strtol(portChar, &ptr, 10);

    // Check port is valid
    if (port != 0) {
        if (!(port > 0 && port <= 65535)) {
            fprintf(stderr, "Port number not within valid range\n");
            exit(1);
        }
    } else { // Not a number
        fprintf(stderr, "Invalid port number\n");
        exit(1);
    }

    // Check homedir is valid
    DIR *dir = opendir(argv[2]);
    if (dir) { // Successfully opened dir
        closedir(dir);
    } else { // Dir cannot be used
        error("Error with web directory");
    }
}

/**
 * Handle signals.
 *
 * @param signo The signal number.
 */
void sig_handler(int signo) {
    if (signo == SIGINT) {
        if (pid != 0) {
            // Inform of interrupt being received.
            printf("\nReceived SIGINT\n");

            wait(NULL);

            // Close the initial socket.
            printf("Closing socket...\n");
            if (close(initialSocket) != 0) {
                fprintf(stderr, "Failed to close socket.\n");
            }

            // Exit the program cleanly after threads.
            exit(0);
        } else {
            exit(0);
        }
    }
}

/**
 * Handle processes based on parent/child.
 *
 * @param clientSocket The socket that the client is on.
 */
void handleProcess(int clientSocket) {
    pid = fork();

    // Check that forking didn't return an error.
    if (pid < 0) {
        fprintf(stderr, "Failed to fork.");
        return;
    }

    if (pid == 0) { // Child process.
        // The child process doesn't need the listening socket.
        close(initialSocket);

        // Handle the client's request.
        handleRequest(clientSocket, home);

        // Request handled, cleanup process.
        close(clientSocket);
        exit(0);
    } else { // Parent Process.
        // Parent doesn't need the new connection socket.
        close(clientSocket);
    }
}

/**
 * Accept connections to new sockets.
 */
void acceptConnections() {
    int clientSocket;

    // Create a new socket for a connecting client.
    while ((clientSocket = acceptConnection(initialSocket))) {
        handleProcess(clientSocket);

        // Check if an interrupt signal has been given and cleanup if so.
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            printf("Failed to handle SIGINT.\n");
        }
    }
}

int main(int argc, char *argv[]) {
    // Check that correct arguments were given.
    checkRunParams(argc, argv);

    // Set the home directory
    home = argv[2];

    // Initialise a socket listening on the given port.
    initialSocket = init(argv[1]);

    // Listen on the socket.
    if (listen(initialSocket, 20) != 0) {
        error("Failed listening on the socket");
    }

    // Accept client connections.
    acceptConnections();

    // Close the initial socket.
    close(initialSocket);

    return 0;
}