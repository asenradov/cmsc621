//Winston Tong
//CMSC 621

#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include <stdlib.h>
#include <pthread.h>

#define GROUP_PORT 6000
#define GROUP_IP "239.0.0.1"
#define MULTI_DEVICES 1 //Exculding Gateway
//5th device is gateway
//<0,1,2,3,4>

struct device{
  int id;
  char *type;
  int port;
  char *ip;
  char *value;
  int socket;
};

typedef struct{
  int port;
  char *ip;
  int socket;
}BackGate;
  
struct node{
  struct device* data;
  struct node* next;
};

typedef struct{
  int port;
  char ip[255];
  int* clock;
} Gateway;

//list
struct node* list;
int device_count = 0;
Gateway g;
BackGate b;

//print the list of devices
void print_list(){
  struct node* temp = list;
  struct device* temp_device;
  printf("-----------------------------------------------------\n");
  while (temp != NULL){
    temp_device = temp->data;
    //Do not print if the sensor is just on for some reason...
    if((strcmp(temp_device->type,"sensor")==0 && strcmp(temp_device->value,"on")!=0) || strcmp(temp_device->type,"device")==0){
      printf("%d----%s:%d----%s----%s\n",temp_device->id,temp_device->ip,temp_device->port,temp_device->type,temp_device->value);
    }
    temp = temp->next;
  }
  printf("-----------------------------------------------------\n");
}

//remove the node with the socket from the list
void remove_node(int sock){
  struct node* temp = list;
  struct node* previous = NULL;
  struct device* temp_device;
  while (temp!= NULL){
    temp_device = temp->data;
    if(temp_device->socket == sock){
      if (previous==NULL){//item is the first element
	//printf("DONIG WORK\n");
	free(list->data->type);
	free(list->data->ip);
	free(list->data->value);
	free(list);
	list = list->next;
	//printf("CHECK %d\n",list==NULL);
	break;
      }
      else{
	previous->next = temp->next;
	free(temp_device->type);
	free(temp_device->ip);
	free(temp_device->value);
	free(temp_device);
	free(temp);
	break;
      }
    }
    previous = temp;
    temp = temp->next;
  }
  //print_list();
}

//insert the device to the list
void insert(struct device **addition){
  struct node* temp = list;
  if(list == NULL){
    list = malloc(sizeof(struct node));
    list -> data = (*addition);
    list -> next = NULL;
  }
  else{
    while (temp->next!= NULL){
      temp = temp->next;
    }
    temp -> next = malloc(sizeof(struct node));
    temp = temp -> next;
    temp -> data = (*addition);
    temp -> next = NULL;
  }
}

//Read in gateway characteristics
void readConfig(char* file){
  char raw[255];
  FILE *fp;
  fp = fopen(file,"r");
  if(fp == NULL){
    printf("Error opening file.\n");
    exit(0);
  }
  fscanf(fp,"%s",raw);
  strcpy(g.ip,(strtok(raw,",")));
  g.port = atoi(strtok(NULL,","));
  
  fclose(fp);
}

//register node. append it to the list in off state
//device_count 0 is reserved for the backend
void register_node(struct device **client_device,char *action,int sock){
  char buffer[1024];
  char* type = strtok(action,"-");
  char* ip = strtok(NULL,"-");
  char* port = strtok(NULL,"-");

  (*client_device)=malloc(sizeof(struct device)); 

  if (strcmp(type,"backGate")==0){
    b.ip = strdup(ip);
    b.port =atoi(port);
    b.socket = sock;
  }
  else{
    (*client_device)->id = device_count;
    device_count++;
    (*client_device)->type = strdup(type);
    (*client_device)->ip = strdup(ip);
    (*client_device)->port = atoi(port);
    (*client_device)->value = strdup("off");
    (*client_device)->socket = sock;
    insert(client_device);
  }
  printf("REGISTERED\n");
  //if(strcmp("sensor",(*client_device)->type)==0){
  //  snprintf(buffer,sizeof(buffer),"Type:switch;Action:on\0");
  //  write((*client_device)->socket,buffer,strlen(buffer)+1);
  //}
  //else{//remove later
  //  print_list();
  //}
}

//check if the new value is different.
//If it is update the value
//Also check the value against the threshhold. Take appropriate action.
//else do nothing
void update(struct device **client_device,char *action){
  
}

//Send a clear to each device excluding backGate
//Type:clear:Action:value-(MULTI_DEVICES) + 1 gateway
void sendClears(){
  char buffer[1024];
  struct node* temp = list;
  struct device* temp_device;
  while (temp != NULL){
    temp_device = temp->data;
    snprintf(buffer,sizeof(buffer),"Type:clear;Action:%d-%d\0",temp_device->id,MULTI_DEVICES+1);
    write(temp_device->socket,buffer,strlen(buffer)+1);
    temp = temp->next;
  }
}

//Decodes the message from the client
void identify(struct device **client_device,char *msg,int socket){
  //printf("msg: %s\n",msg);
  char * type;
  char * action;
  //char buffer[1024];
  strtok(msg,":");
  type = strtok(NULL,":");

  action = strtok(NULL,":");
  
  type = strtok(type,";");
  
  //printf("type: %s\n",type);
  //printf("action: %s\n", action);
  
  if (type == NULL || action == NULL){
    return 0;
  }
  if (strcmp(type,"register")==0){
    register_node(client_device,action,socket);
    if (device_count == MULTI_DEVICES){//All devices registered
      //TODO: MAKE SURE BACKGATE IS REGISTERED AS WELL
      sendClears();
    }
  }
  else if (strcmp(type,"currState")==0){
    if (strcmp(action,"on")==0){
      free((*client_device)->value);
      (*client_device)->value = strdup("on");

      //Test setInterval  
      //snprintf(buffer,sizeof(buffer),"Type:setInterval;Action:10\0");
      //write((*client_device)->socket,buffer,strlen(buffer)+1);
    }
    else{
      free((*client_device)->value);
      (*client_device)->value = strdup("off");
    }
    //only print when device changes...
    if(strcmp((*client_device)->type,"device")==0){
      print_list();
    }
  }
  else if (strcmp(type,"currValue")==0){
    //printf("---> currValue: %s\n", (*client_device)->value);
    //printf("---> action: %s\n", action);
    update(client_device,action);
  }
}


//Thread for each client
void *connection_handler(void *socket_desc){
  int client_sock = *(int*)socket_desc;
  int read_size;
  char client_message[2000];
  //printf("IM A THREAD: %d\n",pthread_self());
  struct device* client_device; //client device of the thread
  
  //Receive a message from client
  while( (read_size = recv(client_sock , client_message , 2000 , 0)) > 0 )
    {
      //Send the message back to client
      //printf("Recieved message: %s\n",client_message);
      identify(&client_device,client_message,client_sock);
    }
     
  if(read_size == 0)
    {
      //free the node
      //unique socket
      remove_node(client_sock);

      //puts("Client disconnected");
      fflush(stdout);
    }
  else if(read_size == -1)
    {
      perror("recv failed");
    }

  //free socket pointer
  free(socket_desc);

  return 0;
}

void* multicast_listener(){
  struct sockaddr_in addr;
  int addrlen, sock, cnt;
  struct ip_mreq mreq;
  char message[50];

  /* set up socket */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }
  bzero((char *)&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(GROUP_PORT);
  addrlen = sizeof(addr);
  
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {        
    perror("bind");
    exit(1);
  }    
  mreq.imr_multiaddr.s_addr = inet_addr(GROUP_IP);         
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);         
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		 &mreq, sizeof(mreq)) < 0) {
    perror("setsockopt mreq");
    exit(1);
  }         
  while (1) {
    cnt = recvfrom(sock, message, sizeof(message), 0, 
		   (struct sockaddr *) &addr, &addrlen);
    if (cnt < 0) {
      perror("recvfrom");
      exit(1);
    } else if (cnt == 0) {
      break;
    }
    printf("%s: message = \"%s\"\n\0", inet_ntoa(addr.sin_addr), message);
    send(b.socket,message,strlen(message)+1,0);
  }
}

int main(int argc , char *argv[])
{
  //Add arguments
  readConfig(argv[1]);

  //Start Listening on Multichannel
  pthread_t multi;
  g.clock = malloc((MULTI_DEVICES+1)*sizeof(int));
  if (pthread_create(&multi,NULL,multicast_listener,NULL) < 0){
    perror("Could not create new thread");
    return 1;
  }
      
  int socket_desc , client_sock , c , read_size,*new_sock;
  struct sockaddr_in server , client;
     
  //Create socket
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc == -1)
    {
      printf("Could not create socket");
    }
  //puts("Socket created");
     
  //Prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(g.ip);
  server.sin_port = htons( g.port );
     
  //Bind
  if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
      //print the error message
      perror("bind failed. Error");
      return 1;
    }
  //puts("bind done");
     
  //Listen
  listen(socket_desc , 3);
     
  //Accept and incoming connection
  //puts("Waiting for incoming connections...");
  c = sizeof(struct sockaddr_in);
     
  //accept connection from an incoming client
  while((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))){
    //puts("Connection accepted");

    pthread_t sniffer_thread;
    new_sock = malloc(1);
    *new_sock = client_sock;

    if (pthread_create(&sniffer_thread,NULL,connection_handler,(void*)new_sock) < 0){
      perror("Could not create new thread");
      return 1;
    }
    
    //pthread_join(sniffer_thread,NULL);
    //puts("handler assigned");
    
  }
  if (client_sock < 0)
    {
      perror("accept failed");
      return 1;
    }

  return 0;
}
