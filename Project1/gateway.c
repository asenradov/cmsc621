//Winston Tong
//CMSC 621

#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include <stdlib.h>
#include <pthread.h>

struct device{
  int id;
  char *type;
  int port;
  char *ip;
  int area;
  char *value;
  int socket;
};
  
struct node{
  struct device* data;
  struct node* next;
};

typedef struct{
  int port;
  char ip[255];
} Gateway;

//list
struct node* list;
int device_count = 0;
Gateway g;

//print the list of devices
void print_list(){
  struct node* temp = list;
  struct device* temp_device;
  printf("-----------------------------------------------------\n");
  while (temp != NULL){
    temp_device = temp->data;
    //Do not print if the sensor is just on for some reason...
    if((strcmp(temp_device->type,"sensor")==0 && strcmp(temp_device->value,"on")!=0) || strcmp(temp_device->type,"device")==0){
      printf("%d----%s:%d----%s----%d----%s\n",temp_device->id,temp_device->ip,temp_device->port,temp_device->type,temp_device->area,temp_device->value);
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

//Read in gatewat characteristics
void readConfig(char* file){
  char raw[255];
  FILE *fp;
  fp = fopen(file,"r");
  if(fp == NULL){
    printf("Error opening file.\n");
    exit(0);
  }
  fscanf(fp,"%s",raw);
  strcpy(g.ip,(strtok(raw,":")));
  g.port = atoi(strtok(NULL,":"));
  
  fclose(fp);
}

//register node. append it to the list in off state
void register_node(struct device **client_device,char *action,int sock){
  char buffer[1024];
  char* type = strtok(action,"-");
  char* ip = strtok(NULL,"-");
  char* port = strtok(NULL,"-");
  char* area = strtok(NULL,"-");

  (*client_device)=malloc(sizeof(struct device)); 

  (*client_device)->id = device_count;
  device_count++;
  (*client_device)->type = strdup(type);
  (*client_device)->ip = strdup(ip);
  (*client_device)->port = atoi(port);
  (*client_device)->area = atoi(area);
  (*client_device)->value = strdup("off");
  (*client_device)->socket = sock;
  insert(client_device);

  if(strcmp("sensor",(*client_device)->type)==0){
    snprintf(buffer,sizeof(buffer),"Type:switch;Action:on\0");
    write((*client_device)->socket,buffer,strlen(buffer)+1);
  }
  //else{//remove later
  //  print_list();
  //}
}

//check if the new value is different.
//If it is update the value
//Also check the value against the threshhold. Take appropriate action.
//else do nothing
void update(struct device **client_device,char *action){
  struct node* temp = list;
  struct device* temp_device;
  char buffer[1024];
  int temp_val = atoi(action);
  int turn_off = 1;//Flag to check multiple sensors in area
  
  //printf("the value is %d\n",temp_val);
  
  if (strcmp((*client_device)->value,action)!=0){
    free((*client_device)->value);
    (*client_device)->value = strdup(action);
    print_list();//Print since value changed
  }
  
  if (temp_val<32){//Turn on devices if the temp is less than 32
    while (temp != NULL){
      temp_device = temp->data;
      if((strcmp(temp_device->type,"device")==0)&&
	 (temp_device->area==(*client_device)->area)&&
	 (strcmp(temp_device->value,"off")==0)){
	
	snprintf(buffer,sizeof(buffer),"Type:switch;Action:on\0");
	write(temp_device->socket,buffer,strlen(buffer)+1);
      }
      temp = temp->next;
    }
  }
  else if (temp_val>34){//Turn off devices in area
    while (temp != NULL){
      temp_device = temp->data;
      if ((temp_device->area==(*client_device)->area)&&//not ok to turn off devices
	  (strcmp(temp_device->value,"on")!=0)&&
	  (strcmp(temp_device->value,"off")!=0)&&
	  (atoi(temp_device->value)<32)){
	turn_off = 0;
      }
      temp = temp->next;
    }
    if (turn_off){
      temp = list;//find devices to turn off
      while (temp != NULL){
	temp_device = temp->data;
	if((strcmp(temp_device->type,"device")==0)&&
	   (temp_device->area==(*client_device)->area)&&
	   (strcmp(temp_device->value,"on")==0)){
	  
	  snprintf(buffer,sizeof(buffer),"Type:switch;Action:off\0");
	  write(temp_device->socket,buffer,strlen(buffer)+1);
	}
	temp = temp->next;
      }
    }
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

int main(int argc , char *argv[])
{
  //Add arguments
  readConfig(argv[1]);

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
