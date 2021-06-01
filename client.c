#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<fcntl.h> // for open
#include<unistd.h> // for close
#include<pthread.h>

#define MAX_BUFFER 1024

struct query
{
    int user;
    int action;
    int data;
};

void *cientThread (void *arg)
{
    char ip_address[16] = "127.0.0.1\0";
    int port = *((int *)arg);

    char send_buffer[MAX_BUFFER];
    char recv_buffer[MAX_BUFFER];
    int clientSocket;
    struct sockaddr_in serverAddr;
    socklen_t addr_size;

    // Create the socket. 
    clientSocket = socket(PF_INET, SOCK_STREAM, 0);

    //Configure settings of the server address
    // Address family is Internet 
    serverAddr.sin_family = AF_INET;

    //Set port number, using htons function 
    serverAddr.sin_port = htons(port);

    //Set IP address to localhost
    serverAddr.sin_addr.s_addr = inet_addr(ip_address);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    //Connect the socket to the server using the address
    addr_size = sizeof serverAddr;
    if (connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size) == -1) {
        printf("connect() error!\n");
	close(clientSocket);
	exit(1);
    } else {
        printf("server connected\n");
	char full[3];

	int size=read(clientSocket,full,2);
	//printf("%s\nsize of full is %d\n",full,size);
	//printf("strncmp result is %d\n",strncmp(full,"-1",2));
	if(strncmp(full,"-1",2)==0){
		printf("Server denied this connection\n");
		exit(1);
	}else if(strncmp(full,"+1",2)==0){
		//printf("client ok\n");
	}
        char ch;
        while (1) {
            //Wait command
            int i = 0;
            while (0 < read(STDIN_FILENO, &ch, sizeof(char))) {
                if (ch == '\n') {
                    send_buffer[i] = '\0';
                    break;
                }
                send_buffer[i] = ch;
                i++;
			}

			struct query request;
            // FIXME... 
            if (sscanf(send_buffer, "[%d, %d, %d]", &request.user, &request.action, &request.data) <= 0) {
                continue;
            }
            //printf("(%d, %d, %d)", request.user, request.action, request.data);

            //Send request
            if (send(clientSocket, (char *)&request, sizeof(request), 0) < 0) {
            /* strcpy(send_buffer,"[7, 1, 333]");
            if (send(clientSocket, send_buffer, strlen(send_buffer), 0) < 0) { */
                printf("Send failed\n");
                break;
            }

            //Read the send_buffer from the server into the recv_buffer
            if (recv(clientSocket, recv_buffer, MAX_BUFFER, 0) < 0) {
                printf("Receive failed\n");
                break;
            }

            //Print the received send_buffer
            if (request.action == 0) {
                printf("connection closed\n");
                printf("%s\n", recv_buffer);
                close(clientSocket);
                pthread_exit(NULL);
                return NULL;
            }

            printf("%s\n", recv_buffer);
            memset(recv_buffer, '\0', MAX_BUFFER*sizeof(char));
        }
    }

    printf("connection closed\n");
    close(clientSocket);
    pthread_exit(NULL);

    return NULL;
}

int main (int argc, char *argv[]) {
    int port = atoi(argv[1]);
    int number_of_client = 1;

    int n, cfd;
    struct sockaddr_in saddr;
    char buf[MAX_BUFFER];

    if((cfd=socket(AF_INET,SOCK_STREAM, 0))<0){
	    printf("socket failed\n");
	    exit(1);
    }

    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family=AF_INET;
    saddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    saddr.sin_port=htons(port);

    if(connect(cfd,(struct sockaddr *)&saddr,sizeof(saddr))<0){
	    printf("connect() failed\n");
	    exit(3);
    } else {
        printf("server connected\n");
	char full[3];
	int size=read(clientSocket,full,2);
	//printf("%s\nsize of full is %d\n",full,size);
	//printf("strncmp result is %d\n",strncmp(full,"-1",2));
	if(strncmp(full,"-1",2)==0){
		printf("Server denied : Clints are fully used\n");
		exit(1);
	}else if(strncmp(full,"+1",2)==0){
		//printf("client ok\n");
	}
        char ch;
        while (1) {
            //Wait command
            int i = 0;
            /*
	    while (0 < read(STDIN_FILENO, &ch, sizeof(char))) {
                if (ch == '\n') {
                    send_buffer[i] = '\0';
                    break;
                }
                send_buffer[i] = ch;
                i++;
	    }
	    */
	    struct query request;
            //FIXME
	    char command[25];
	    char c;
	    request.user=-1;
	    request.action=-1;
	    request.data=-1;
	    gets(command);
	    while(true){
		    c=command[i];
		    if(c=='\0')
			    break;
		    //gets는 문장 마지막에 '\0'을 붙여서 반환한다. 
		    if(c=='['){
			    int idx=0;
			    i++;
			    char id[5];//최대값이 1024
			    while(command[i]!=','){
				    id[idx++]=command[i++];
			    }
			    printf("len of user id is %d\n",strlen(id));
			    //idx is set as id len
			    request.user=0;
			    int w=idx;
			    for(int j=0;j<idx;j++){
				    if(w==0)
					    break;

				    if(w==1){
					    //한자리 수, j와 w는 맞물려서, w가 0이 될 일이 없음
					    request.user+=(id[j]-48);
				    }else{
					    //나머지 자리 수 
					    request.user+=(id[j]-48)*pow(10.0,w-1);
					    w--;//지수
				    }
			    }
			    printf("user id is %d\n",request.user);//현재 i는 ','를 가리키고 있으니, i=i+2 쉼표, 공백 띄고 //-> action check : 한자리 수
			    i=i+2;//comma and space
			    request.action=0;
			    request.action=command[i]-48;//action 1 digit
			    i=i+2;//comma and space 
			    request.data=0;

			    idx=0;
			    char data[12];
			    while(command[i]!=']'){
				    data[idx++]=command[i++];
			    }
			    printf("len of data is %d\n",strlen(data));

			    w=idx;
			    for(int j=0;j<idx;j++){
				    if(w==0)
					    break;
				    if(w==1)
					    request.data+=(data[j]-48);
				    else{
					    request.data+=(data[j]=48)*pow(10.0,w-1);
					    w--;
				    }
			    }
			    printf("data is %d\n",request.data);
		    }

		    printf("user : %d\n",request.user);
		    printf("action : %d\n",request.action);
		    printf("data : %d\n",request.data);
	    }
    return 0;
}
