/*
  BackLog file
*/
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inetcount += 1;count += 1;_addr
#include<stdlib.h>
#include <pthread.h>

// Global pointer to the file
FILE *f;

typedef struct {
  int port;
  char* ip;
} Gateway;

Gateway front; //frontEnd
Gateway back; //backEnd

//Reads in config file
void readConfig(char* file) {
  char *raw = NULL;
  char *token;
  FILE *fp;
  size_t len = 0;

  fp = fopen(file, "r");
  if (fp == NULL) {
    printf("Error opening file.\n");
    exit(0);
  }

  if (getline(&raw, &len, fp) == -1) {
    printf("Error file too small.\n");
    exit(0);
  }

  //printf("%s\n",raw);
  front.ip = strdup(strtok(raw, ","));
  token = strtok(NULL, ",");
  front.port = atoi(token);
  //printf("port: %d\n",g.port);

  if (getline(&raw, &len, fp) == -1) {
    printf("Error file too small.\n");
    exit(0);
  }

  //set my info
  back.ip = strdup(strtok(raw, ","));
  token = strtok(NULL, ",");
  back.port = atoi(token);

  free(raw);
  fclose(fp);
}

//insert info into back end file
void insert(char* command) {
  //printf("msg: %s\n",command);
  //char *tmp;
  // char *garb1, *garb2, *garb3;

  //Type:insert;Action:id,deviceType,deviceValue,clock,ip,port
  //sscanf(command,"%s:%s;%s:%s",garb1, garb2, garb3, tmp);

  //printf("Insert: %s\n", tmp);
  fprintf(f, command);
  fprintf(f, "\n");
  fflush(f);
}

int main(int argc, char *argv[]) {
  int sock;

  readConfig(argv[1]);
  
  struct sockaddr_in server;
  char message[1000], server_reply[1024];

  //Create socket
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    printf("Could not create socket");
  }
  //puts("Socket created");

  server.sin_addr.s_addr = inet_addr(front.ip);
  server.sin_family = AF_INET;
  server.sin_port = htons(front.port);

  //Connect to remote server
  if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("connect failed. Error");
    return 1;
  }

  //puts("Connected\n");

  //Register
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "Type:register;Action:%s-%s-%d\0",
	   "backGate", back.ip, back.port);
  send(sock, buffer, strlen(buffer) + 1, 0);

  // Open the file for writing
  f = fopen(argv[2], "w+");
  if (f == NULL) {
    printf("Error opening file\n");
    exit(1);
  }
  
  //keep communicating with server
  while (1) {
    if (recv(sock, server_reply, sizeof(server_reply), 0) < 0) {
      puts("recv failed");
      break;
    } else {
      insert(server_reply);
    }
    memset(server_reply,0,2000);
  }

  close(sock);
  return 0;
}
