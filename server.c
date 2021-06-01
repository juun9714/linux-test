#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<fcntl.h> // for open
#include<unistd.h> // for close
#include<pthread.h>
#include<errno.h>

#define MAX_BUFFER 2
#define SEAT_NUMBER 256
#define USER_NUMBER 2

struct query
{
    int user;
    int action;
    int data;
};

pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t content_lock = PTHREAD_MUTEX_INITIALIZER;

// stack for client id
////////////////////////////////////////////////////////////
int standby_clients_id[USER_NUMBER];
int standby_top = -1;
int pushStandbyClientId(int value) {
    if (standby_top >= USER_NUMBER-1) {
        return 0; // false
    }

    standby_clients_id[++standby_top] = value;
    return 1; // true
}

int popStandbyClientId() {
    if (standby_top < 0) {
        return -1; // -1
    }

    return standby_clients_id[standby_top--]; // client id
}
////////////////////////////////////////////////////////////
int active_clients_id[USER_NUMBER];
int active_top = -1;
int pushActiveClientId(int value) {
    if (active_top >= USER_NUMBER-1) {
        return 0; // false
    }

    active_clients_id[++active_top] = value;
    return 1; // true
}

int popActiveClientId() {
    if (active_top < 0) {
        return -1; // -1
    }

    return active_clients_id[active_top--]; // client id
}
////////////////////////////////////////////////////////////
// stack for client id

int clients_socket[USER_NUMBER];

int seats[SEAT_NUMBER];
char users_status[USER_NUMBER];
int users_passcode[USER_NUMBER];

int on_success = 1;
int on_fail = -1;

void *serverThread(void *arg)
{
    pthread_mutex_lock(&client_lock);

    int client_id = popActiveClientId();
    if (client_id == -1) {
        printf("error: there is no active client id\n");
        pthread_mutex_unlock(&client_lock);
        pthread_exit(NULL);
        return NULL;
    }

    int newSocket = clients_socket[client_id - 1];

    //printf("3) standby %d, active %d\n", standby_top, active_top);
    pthread_mutex_unlock(&client_lock);

    //char recv_buffer[MAX_BUFFER];
    char send_buffer[MAX_BUFFER];
    printf("client %d connected.\n", client_id);

    int user_id=-1;
    int i;
    int len;
    while (1) {
        // Receive from client socket
        struct query response;
        if (recv(newSocket, (char *)&response, sizeof(response), 0) <= 0) {
        //if (recv(newSocket, recv_buffer, MAX_BUFFER, 0) < 0) {
            printf("Receive failed\n");
            break;
        }

        //printf("%s\n", recv_buffer);
        //sscanf(recv_buffer, "[ %d, %d, %d ]", &response.user, &response.action, &response.data);
        //printf("%d, %d, %d\n", response.user, response.action, response.data);
    
        pthread_mutex_lock(&content_lock);
        
        switch (response.action) {
            case 0:
                if (response.user != 0 || response.data != 0) {
                    printf("not proper data user %d, data %d.\n", response.user, response.data);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                printf("send all seats reservation info\n");
                len = 0;
                for (i=0; i < SEAT_NUMBER; i++) {
                    len += sprintf(send_buffer+len, "%d ", seats[i]);
                }
                break;
            case 1: // log in
		if(user_id==-1){

			user_id=response.user;
			if (users_status[response.user] != -1) {
			    printf("user %d log-in fail. user %d already logged in.\n", response.user, response.user);
			    sprintf(send_buffer, "%d", on_fail);
			    break;
			}

			if (users_passcode[response.user] == -1) {
			    users_passcode[response.user] = response.data;
			}

			if (users_passcode[response.user] != response.data) {
			    printf("user %d log-in fail. incorrect password \"%d\", \"%d\".\n", response.user, response.data, users_passcode[response.user]);
			    sprintf(send_buffer, "%d", on_fail);
			    break;
			}

			printf("user %d logged in successfully with password \"%d\"\n", response.user, users_passcode[response.user]);
			users_status[response.user] = client_id;
			sprintf(send_buffer, "%d", on_success);
			break;
		}
            case 2: // reserve
                if (users_status[response.user] != client_id) {
                    printf("query failed. user %d doesn't logged in at this client %d, %d.\n", response.user, client_id, users_status[response.user]);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                if (seats[response.data] != -1) {
                    printf("query failed. seat %d already reserved by user %d.\n", response.data, seats[response.data]);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                printf("user %d reserved seat %d.\n", response.user, response.data);
                seats[response.data] = response.user;
                sprintf(send_buffer, "%d", response.data);
                break;
            case 3: // check reservation
                if (users_status[response.user] != client_id) {
                    printf("query failed. user %d doesn't logged in at this client %d, %d.\n", response.user, client_id, users_status[response.user]);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                len = 0;
                for (i=0; i < SEAT_NUMBER; i++) {
                    if (seats[i] == response.user) {
                        len += sprintf(send_buffer+len, "%d ", i);
                    }
                }

                if (len == 0) {
                    printf("there is no check reservation for seat failed.\n");
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                printf("send reservation seat for %d, %s.\n", response.user, send_buffer);
                break;
            case 4: // cancel reservation
                if (users_status[response.user] != client_id) {
                    printf("query failed. user %d doesn't logged in at this client %d, %d.\n", response.user, client_id, users_status[response.user]);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                if (seats[response.data] != response.user) {
                    printf("query failed. user %d doesn't reserved seat %d.\n", response.user, response.data);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                printf("cancel reservation seat %d success.\n", response.data);
                seats[response.data] = -1;
                sprintf(send_buffer, "%d", response.data);
                break;
            case 5: // logout
                if (users_status[response.user] != client_id) {
                    printf("user %d doesn't logged out. client %d, %d.\n", response.user, client_id, users_status[response.user]);
                    sprintf(send_buffer, "%d", on_fail);
                    break;
                }

                printf("user %d logged out.\n", response.user);
                users_status[response.user] = -1;
		user_id=-1;
                sprintf(send_buffer, "%d", on_success);
                break;
        }
        
        // seat check!!!
        pthread_mutex_unlock(&content_lock);

        // Send to the client socket 
        if (send(newSocket, send_buffer, strlen(send_buffer), 0) < 0) {
            printf("Send failed\n");
            break;
        }
    }
    
    printf("client %d terminated.\n", client_id);

    // mutex
    pthread_mutex_lock(&client_lock);

    clients_socket[client_id - 1] = -1;
    if (pushStandbyClientId(client_id) == 0) {
        printf("clients_id is full, critical error\n");
    }

    // printf("4) standby %d, active %d\n", standby_top, active_top);
    pthread_mutex_unlock(&client_lock);

    close(newSocket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    memset(seats, -1, SEAT_NUMBER * sizeof(int)); 
    memset(users_status, -1, USER_NUMBER * sizeof(char)); 
    memset(users_passcode, -1, USER_NUMBER * sizeof(int)); 

    char ip_address[16] = "127.0.0.1\0";
    int port = argv[1] != NULL ? atoi(argv[1]) : 7799;
    int max_connection = USER_NUMBER;

    int serverSocket, newSocket, serverOption;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;

    //Create the socket. 
    serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        printf("socket() error\n");
        return -1;
    }
    
    // set option 
    serverOption = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &serverOption, sizeof(int));         

    // Configure settings of the server address struct
    // Address family = Internet 
    serverAddr.sin_family = AF_INET;

    //Set port number, using htons function to use proper byte order 
    serverAddr.sin_port = htons(port);

    //Set IP address to localhost 
    serverAddr.sin_addr.s_addr = inet_addr(ip_address);

    //Set all bits of the padding field to 0 
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    //Bind the address struct to the socket 
    bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

    //Listen on the socket, with max connection requests queued 
    if (listen(serverSocket, max_connection) !=0) {
        printf("listen failed\n");
        return -1;
    }

    //Clients id
    for (int i = max_connection; i > 0; i--) {
        pushStandbyClientId(i);
    }

    pthread_t tid[max_connection + 1];
    while (1) {
        //Accept call creates a new socket for the incoming connection
        addr_size = sizeof(serverStorage);
        newSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);
        if (newSocket == -1) {
            printf("socket create failed %s\n", strerror(errno));
            close(newSocket);
            break;
        }

        // mutex
        pthread_mutex_lock(&client_lock);
        // printf("1) standby %d, active %d\n", standby_top, active_top);

        int client_id = popStandbyClientId();
        // refuse connection cause over max
        if (client_id == -1) {
            printf("Over Connection\n");
            write(newSocket,"-1",2);
	    close(newSocket);
            pthread_mutex_unlock(&client_lock);
            continue;
        }else{
		write(newSocket,"+1",2);
	}
    
        if (pushActiveClientId(client_id) == 0) {
            printf("critical error check client_id\n");
            close(newSocket);

            pthread_mutex_unlock(&client_lock);
            continue;
        }
        
        //printf("2) standby %d, active %d\n", standby_top, active_top);
        clients_socket[client_id - 1] = newSocket;
        pthread_mutex_unlock(&client_lock);

        //for each client request creates a thread and assign the client request to it to process
        //so the main thread can entertain next request
        if (pthread_create(&tid[client_id], NULL, serverThread, NULL) != 0) {
            printf("Failed to create thread\n");
            return -1;
        }

        pthread_detach(tid[client_id]);
    }
  
    return 0;
}
