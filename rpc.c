#define _POSIX_C_SOURCE 200112L
#define _DEFAULT_SOURCE 
#include <arpa/inet.h>
#include <netdb.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "rpc.h"
#include <stdlib.h>

#define AVG_STRLEN 256
#define MAX_STRLEN 1001
#define MAX_DATA2LEN 100000
#define TOTAL_STRLEN AVG_STRLEN + MAX_STRLEN

#define htonll htobe64
#define ntohll be64toh


struct rpc_server {
    /* Add variable(s) for server state */
    int sockfd;
	struct sockaddr_in6 client_addr;
    socklen_t client_addr_size;
    char **handler_names;
    rpc_handler *handlers;
    int num_handlers;
};

rpc_server *rpc_init_server(int port) {
    // Modified code taken from Week 9 solutions server.c

    int s, sockfd;
	struct addrinfo hints, *res;
    
    // Create address for listening
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;       // IPv6
	hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept

    // Convert port to string
    char service[6];
    sprintf(service, "%d", port);

    // Get the address information
    s = getaddrinfo(NULL, service, &hints, &res);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("Server socket");
		exit(EXIT_FAILURE);
	}

    // Reuse address
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(uint64_t)) < 0) {
        perror("Server setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("Server bind");
		exit(EXIT_FAILURE);
	}

    // Create server
    rpc_server *server = malloc(sizeof(rpc_server));
    if (server == NULL) {
        return NULL;
    }

    // Assign socket to server
    server->sockfd = sockfd;

    // Initialise server t
    server->num_handlers = 0;
    server->handler_names = malloc(sizeof(char*) * server->num_handlers);
    server->handlers = malloc(sizeof(rpc_handler) * server->num_handlers);

    freeaddrinfo(res);
    return server;
}

int rpc_register (rpc_server *srv, char *name, rpc_handler handler) {
    // Return if any arguments NULL
    if (srv == NULL || name == NULL || handler == NULL) {
        return -1;
    }

    // Check if any characters are not ASCII
    for(int i = 0; i < strlen(name); i++) {
        if (name[i] < 32 || name[i] > 126) {
            return -1;
        }
    }

    // Check the length of name
    if (strlen(name) < 1 || strlen(name) >= MAX_STRLEN){
        return -1;
    }

    int ID = -1;

    // Check if function exists by iterating through existing functions
    int does_exist = 0; // No function with same name
    int i = 0;
    while (i < srv->num_handlers && does_exist == 0){
        // Found function with the same exact name
        if (strcmp(srv->handler_names[i], name) == 0){
            does_exist = 1;

            // Replace with new function
            srv->handlers[i] = handler;
            srv->handler_names[i] = name;
            
            // Assign ID to the handler
            ID = i;
            break;  // Don't need to check other functions
        }
        i++;
    }

    // If name doesn't exist, add it to handlers
    int len = srv->num_handlers;
    if (does_exist == 0) {
        
        // Add handler name in the handler names array in the server
        srv->handler_names = realloc(srv->handler_names, sizeof(char*) * (srv->num_handlers + 1));
        srv->handler_names[len] = malloc(sizeof(char) * (strlen(name) + 1));
        strcpy(srv->handler_names[len], name);

        // Add handler in the handlers array stored in the server
        srv->handlers = realloc(srv->handlers, sizeof(rpc_handler) * (srv->num_handlers + 1));
        srv->handlers[len] = handler;

        // Assign ID to handler
        ID = srv->num_handlers;

        // Increase number of handlers
        srv->num_handlers++;
    }

    // Return ID of the handler
    return ID;
}

void rpc_serve_all(rpc_server *srv) {
    if (srv == NULL){
        return ;
    }
    // Listen to incoming requests
    if (listen(srv->sockfd, 5) < 0) {
        perror("serve_all listen");
        exit(EXIT_FAILURE);
    }

    // Keep server running until SIGINT
    while (1) {
        int connfd = 0, n;
        srv->client_addr_size = sizeof srv->client_addr;

        // Accept a connection request and returns new file descriptor
        connfd = accept(srv->sockfd, (struct sockaddr*)&srv->client_addr, &srv->client_addr_size);
        if (connfd < 0) {
            perror("serve_all accept");
            exit(EXIT_FAILURE);
        }

        // Run until connection closed
        while (1) {
            char buf[TOTAL_STRLEN + MAX_DATA2LEN];

            // Read message from client
            n = read(connfd, buf, TOTAL_STRLEN + MAX_DATA2LEN);
            if (n < 0) {
                perror("serve_all read");
                exit(EXIT_FAILURE);
            }

            // Break if client close the connection
            if (n == 0) {
                break;
            }


            // Null-terminate string
            buf[n] = '\0';

            // Get the type of function requested from client message 
            // and seperate the data sent
            char type[AVG_STRLEN];
            char info[TOTAL_STRLEN + MAX_DATA2LEN];
            sscanf(buf, "%[^,],%s", type, info);

            // Serve rpc_find (Find a function in server data)
            int found = 0; // Function not found
            if (strcmp("rpc_find", type) == 0) {
                char msg[TOTAL_STRLEN];

                // Iterate through the server's handlers array to find function in server
                for (int i = 0; i < srv->num_handlers; i++) {

                    // Found the function
                    if (strcmp(srv->handler_names[i], info) == 0) {
                        // Format message for client
                        found = 1;
                        // Tells client if the function is found and the id
                        sprintf(msg, "%d,%d", found, i);
                        break;
                    }
                }

                // Function not found
                if (found == 0) {
                    // Tells the client that the function is not found
                    sprintf(msg, "%d,-1", found);
                }

                // Send message back to client containing the result of search
                n = write(connfd, msg, strlen(msg));
                if (n < 0) {
                    perror("serve_all write");
                    exit(EXIT_FAILURE);
                }

            // Serve rpc_call (Call function from server)
            } else if (strcmp("rpc_call", type) == 0) {
                char msg[TOTAL_STRLEN + MAX_DATA2LEN];
                int invalid = 0; // Not an invalid result
                int id; // ID of handler

                // Read message from client, which are in network endian bytes
                uint64_t arg1;
                int n = read(connfd, &arg1, sizeof(uint64_t));
                if (n < 0) {
                    perror("serve_all read");
                    exit(EXIT_FAILURE);
                }

                // Convert from network endian byte back to int
                arg1 = ntohll(arg1);

                // Read message from client containing ID of function, data2 and size of data2
                char arg2[TOTAL_STRLEN + MAX_DATA2LEN];
                size_t arg2_len;
                sscanf(info, "%d,%[^,],%zu", &id, arg2, &arg2_len);

                // Create response struct
                rpc_data *args = malloc(sizeof(rpc_data));
                assert(args);
                args->data1 = arg1;
                args->data2_len = arg2_len;
                args->data2 = arg2;

                // Call the handler function with the created rpc_data as arguments
                rpc_data *result = srv->handlers[id](args);

                // Check if the handler returns invalid data
                if (result == NULL){
                    invalid = 1;
                    // Prepare to send NULL to client
                    sprintf(msg, "NULL");

                // Check if data2 inconsistent with its len
                } else if ((result->data2 == NULL && result->data2_len != 0) || (result->data2 != NULL && result->data2_len == 0)){
                    invalid = 1;
                    // Prepare to send NULL to client
                    sprintf(msg, "NULL");

                 // Format response to client
                } else {
                    // No data2 returned
                    if (result->data2 == NULL && result->data2_len == 0){
                        sprintf(msg, "%zu,N",result->data2_len);

                    // Format message to send to client containing data2 result information
                    } else {
                        sprintf(msg, "%zu,%s", result->data2_len, (char *)result->data2);
                    }
                }

                // Send the message to client
                n = write(connfd, msg, strlen(msg));
                if (n < 0) {
                    perror("serve_all write");
                    exit(EXIT_FAILURE);
                }

                // Send data1 result to client if result from handler is valid
                if (invalid == 0){
                    // Convert back to network endian bytes
                    uint64_t temp_data1_result = result->data1;
                    temp_data1_result = htonll(temp_data1_result);

                    // Send data1 to client
                    n = write(connfd, &temp_data1_result, sizeof(uint64_t));
                    if (n < 0) {
                        perror("serve_all write");
                        exit(EXIT_FAILURE);
                    }
                }
                free(args);
                free(result);
            }
        }
        close(connfd);
    }
}


struct rpc_client {
    /* Add variable(s) for client state */
    int sockfd;
};

struct rpc_handle {
    /* Add variable(s) for handle */
    int ID;
};

rpc_client *rpc_init_client(char *addr, int port) {
    // Check validity of address
    if (addr == NULL){
        return NULL;
    }

    // Modified code taken from Week 9 solutions cl ient.c
    int sockfd, s;
	struct addrinfo hints, *servinfo, *rp;

    // Create address
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

    // Convert port to string
    char service[6];
    sprintf(service, "%d", port);

    // Get addrinfo of server
	s = getaddrinfo(addr, service, &hints, &servinfo);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

    // Connect to first valid result
    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
        // Create socket
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1)
			continue;

        // Connect to socket
		if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; // Stop trying to connect if successfull

		close(sockfd);
	}

    // Did not form successful connection
	if (rp == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(servinfo);

    // Create client
    rpc_client *client = malloc(sizeof(rpc_client));
    if (client == NULL) {
        return NULL;
    }

    // Assign socket to client
    client->sockfd = sockfd;

    return client;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    // Check if invalid arguments
    if (cl == NULL || name == NULL) {
        return NULL;
    }

    // Format message that server will read 
    // containing the function type and handlername
	char type[AVG_STRLEN] = "rpc_find,";
    char buf[MAX_STRLEN + AVG_STRLEN];
    strcpy(buf, type);
    strcat(buf, name);

    // Send message formed to server
    int n;
	n = write(cl->sockfd, buf, strlen(buf));
	if (n < 0) {
		perror("rpc_find write");
		exit(EXIT_FAILURE);
	}

	// Read result from server
	n = read(cl->sockfd, buf, TOTAL_STRLEN);
    buf[n] = '\0'; // Null terminate string
	if (n < 0) {
		perror("rpc_find read failed");
        exit(EXIT_FAILURE);
	}

    // Retrieve result from server message
    int ID, found;
    sscanf(buf, "%d,%d", &found, &ID);

    // Function not found
    if (found == 0){
        return NULL;
    }

    // Create handle containing handler ID
    rpc_handle *handle = malloc(sizeof(rpc_handle));
    handle->ID = ID;

    return handle;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // Check if any arguments invalid
    if (cl == NULL || h == NULL || payload == NULL) {
        return NULL;
    }
    // Check if payload invalid
    if ((payload->data2 == NULL && payload->data2_len != 0) || (payload->data2 != NULL && payload->data2_len == 0)){
        return NULL;
    }

    // Unpack handle
    int id = h->ID;

    // Format message that server will read
    char type[AVG_STRLEN] = "rpc_call,";
    char buf[TOTAL_STRLEN + MAX_DATA2LEN];

    // Format function info
    char handle_info[TOTAL_STRLEN + MAX_DATA2LEN];

    // Check if packet format can handle data2_len
    if (payload->data2_len > MAX_DATA2LEN){
        perror("Overlength error");
        exit(EXIT_FAILURE);
    }

    // Format message to send to server
    // containing handler id and data2 information
    sprintf(handle_info, "%d,%s,%zu", id, (char *)payload->data2, payload->data2_len);

    // Combine into one message
    strcpy(buf, type);
    strcat(buf, handle_info);
    
    // Send message to server
    int n;
    n = write(cl->sockfd, buf, strlen(buf));
    if (n < 0) {
        perror("rpc_call write");
        exit(EXIT_FAILURE);
    }

    // Convert data1 to network bytes
    uint64_t temp_data1 = payload->data1;
    temp_data1 = htonll(temp_data1);

    // Send data1 to server
    n = write(cl->sockfd, &temp_data1, sizeof(uint64_t));
    if (n < 0) {
        perror("rpc_call write");
        exit(EXIT_FAILURE);
    }

    // Read message from server
    n = read(cl->sockfd, buf, TOTAL_STRLEN+MAX_DATA2LEN);
    if (n < 0) {
        perror("rpc_call read");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0'; // Null terminate string

    // Create response struct
    rpc_data *response = malloc(sizeof(rpc_data));
    assert(response);
    char temp[TOTAL_STRLEN + MAX_DATA2LEN + 1];  // Allocate memory for temp
    char tempbuf[TOTAL_STRLEN + MAX_DATA2LEN + 1];

    // Retrieve information from server
    sscanf(buf, "%s", tempbuf);

    // Check if response is invalid
    if (strcmp(buf, "NULL") == 0) {
        return NULL;
    }

    // Read data1 information from server
    uint64_t data1;
    n = read(cl->sockfd, &data1, sizeof(uint64_t));
    if (n < 0) {
        perror("rpc_call read");
        exit(EXIT_FAILURE);
    }
    // Convert to int from network bytes
    data1 = ntohll(data1);



    // Read data2 response from server
    sscanf(tempbuf, "%zu,%s", &response->data2_len, temp);

    // Assign data1 to response
    response->data1 = data1;

    // No data2 returned
    if (response->data2_len == 0) {
        response->data2 = NULL;
    
    // Assign data2 to response
    } else {
        response->data2 = malloc(response->data2_len + 1);
        memcpy(response->data2, temp, response->data2_len);
        ((char *)response->data2)[response->data2_len] = '\0';
    }
    return response;
}


void rpc_close_client(rpc_client *cl) {
    // Check if closed already
    if (cl == NULL || cl->sockfd == -1) {
        return;
    }

    // Close client socket
    close(cl->sockfd);
    cl->sockfd = -1;
    
    free(cl);
}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
