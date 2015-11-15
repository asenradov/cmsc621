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
#define MULTI_DEVICES 3 //Exculding Gateway
//5th device is gateway
//<0,1,2,3,4>

struct device{//Door,Motion,Key
  int id;
  char *type;
  int port;
  char *ip;
  char *value;
  int socket;
  int* clock;
};

typedef struct{
  int port;
  char *ip;
  int socket;
  int registered;

}BackGate;
  
struct node{
  struct device* data;
  struct node* next;
};

typedef struct{
  int id;
  int port;
  char ip[255];
  int* clock;
  int clock_size;
} Gateway;

//list
struct node* list;
struct device* recentMotion;
struct device* recentKey;
struct device* recentDoor;
struct device* security;

pthread_mutex_t mutex;

int device_count = 0;
Gateway g;
BackGate b;
int record = 0;//When door close even occurs 1
int motion = 0;// Valid motion avalible
int key = 0; //Valid Key avalible

//print the list of devices
void print_list(){
  struct node* temp = list;
  struct device* temp_device;
  printf("-----------------------------------------------------\n");
  while (temp != NULL){
    temp_device = temp->data;
    //Do not print if the sensor is just on for some reason...
    printf("%d----%s:%d----%s----%s\n",temp_device->id,temp_device->ip,temp_device->port,temp_device->type,temp_device->value);
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

  if (strcmp(type,"backGate")==0){
    b.ip = strdup(ip);
    b.port = atoi(port);
    b.socket = sock;
    b.registered = 1;
  }
  else{
    (*client_device)=malloc(sizeof(struct device)); 
    (*client_device)->id = device_count;
    device_count++;
    (*client_device)->type = strdup(type);
    (*client_device)->ip = strdup(ip);
    (*client_device)->port = atoi(port);
    (*client_device)->value = strdup("off");//Device starts off User is home
    (*client_device)->socket = sock;
    (*client_device)->clock = malloc(g.clock_size*sizeof(int));
    memset((*client_device)->clock,0,g.clock_size*sizeof(int));

    if (strcmp((*client_device)->type,"motionSensor")==0){
      recentMotion = (*client_device);
    }
    else if(strcmp((*client_device)->type,"keySensor")==0){
      recentKey = (*client_device);
    }
    else if(strcmp((*client_device)->type,"doorSensor")==0){
      recentDoor = (*client_device);
    }
    else if(strcmp((*client_device)->type,"device")==0){
      security = (*client_device);
    }
    insert(client_device);
  }
  printf("REGISTERED: %d\n",(*client_device)->id);
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

void multi_identify(char *msg){
  char* type;
  char* action;
  char* value;
  char* senseType;
  int id;
  char buffer[1024];
  struct node* temp = list;
  strtok(msg,":");
  type = strtok(NULL,":");
  action = strtok(NULL,":");
  //printf("action: %s\n",action);
  type = strtok(type,";");
  //printf("type: %s\n",type);
  
  if (strcmp(type,"currValue")==0){
    int* temp_clock[g.clock_size];
    int a;
    //Parse
    id = atoi(strtok(action,"-"));
    (strtok(NULL,"-"));//Parse
    value = (strtok(NULL,"-"));//Value
    senseType = (strtok(NULL,"-"));//Type
    
    temp_clock[0] = (atoi(strtok(action,",")));
    for (a = 1; a<g.clock_size; a++){
      temp_clock[a] = (atoi(strtok(NULL,",")));
    }
    
    //synch clock
    pthread_mutex_lock(&mutex);
    //increment own clock upon recieving
    g.clock[g.id]++;

    for (a=0; a<g.clock_size; a++){
      if (temp_clock[a]>g.clock[a]){
	g.clock[a] = temp_clock[a];
      }
    }
    pthread_mutex_unlock(&mutex);
    
    printf("NEW CLOCK ");
    for (a=0; a<g.clock_size;a++){
      printf(",%d",g.clock[a]);
    }
    printf("\n");

    //Log to backend
    time_t diff = difftime(time(NULL),0);
    int newest = 0;//1 if data is newer
    puts("BEGIN TESTING");
    if (strcmp(senseType,"doorSensor")==0){
      puts("FOUND DOOR");
      snprintf(buffer,sizeof(buffer),"Type:insert;Action:%d,%s,%s,%d,%s,%d\0",id,senseType,value,diff,(*recentDoor).ip,(*recentDoor).port);
      (*recentDoor).clock;
      //Update clock
      for (a =0; a< g.clock_size; a++){
	if ((*recentDoor).clock[a]<temp_clock[a]){
	  newest = 1;
	  (*recentDoor).clock[a] = temp_clock[a];
	  //puts("CLOCK UPDATED");
	}
      }
      if (newest){//New data is valid
	(*recentDoor).value = value;
	if (strcmp(value,"Close")==0){//Door close. record
	  puts("RECORDING");
	  record = 1;//flag to record recent values
	  //check if recentmotion and recentkey are more recent
	  for (a =0; a< g.clock_size; a++){
	    if ((*recentDoor).clock[a]<(*recentMotion).clock[a]){
	      motion = 1;
	    }
	    if ((*recentDoor).clock[a]<(*recentKey).clock[a]){
	      key = 1;
	    }
	  }
	}
	else{
	  puts("STOP RECORDING");
	  record = 0;
	  motion = 0;
	  key = 0;
	}
      }
    }
    else if (strcmp(senseType,"keySensor")==0){
      puts("FOUND KEY");
      snprintf(buffer,sizeof(buffer),"Type:insert;Action:%d,%s,%s,%d,%s,%d\0",id,senseType,value,diff,(*recentKey).ip,(*recentKey).port);
      for (a =0; a< g.clock_size; a++){
	if ((*recentKey).clock[a]<temp_clock[a]){
	  newest = 1;
	  (*recentKey).clock[a] = temp_clock[a];
	}
      }
      if (newest){//New Data is valid
	(*recentKey).value = value;
      }
      //check if the event happened after
      if (record){
	for (a =0; a< g.clock_size; a++){
	  if ((*recentKey).clock[a]>(*recentDoor).clock[a]){
	    key =1;
	  }
	}
      }
    }
    else if(strcmp(senseType,"motionSensor")==0){
      puts("FOUND MOTION");
      snprintf(buffer,sizeof(buffer),"Type:insert;Action:%d,%s,%s,%d,%s,%d\0",id,senseType,value,diff,(*recentMotion).ip,(*recentMotion).port);
      for (a =0; a< g.clock_size; a++){
	if ((*recentMotion).clock[a]<temp_clock[a]){
	  newest = 1;
	  (*recentMotion).clock[a] = temp_clock[a];
	}
      }
      if (newest){
	(*recentMotion).value = value;
      }
      
      //check if the event happened after
      if (record){
	for (a =0; a< g.clock_size; a++){
	  if ((*recentMotion).clock[a]>(*recentDoor).clock[a]){
	    motion = 1;
	  }
	}
      }
    }
    //send new value to backend
    puts("SENDING TO BACKEND");
    send(b.socket,buffer,strlen(buffer)+1,0);

    //0 out buffer
    memset(buffer,0,strlen(buffer));
    
    if (record&&key){//All criteria met. Determine what to do with deivce
       puts("CHECKING SYSTEM");
      //Short circut. Turn on system if off
      if (strcmp("True",(*recentKey).value)==0){
	if (strcmp("on",(*security).value)==0){
	  //Send turn off
	  printf("User In. Turn off\n");
	  snprintf(buffer,sizeof(buffer),"Type:switch;Action:off\0");
	  send((*security).socket,buffer,strlen(buffer)+1,0);
	}
      }
      else if (motion){//Check the motion or wait
	if (strcmp("True",(*recentMotion).value)==0){//Intruder!
	  if (strcmp("on",(*security).value)==0){
	    puts("INTRUDER!!!!!!!!!!!!!!!!");
	    snprintf(buffer,sizeof(buffer),"Type:insert;Action:Intruder Alert!\0");
	    send(b.socket,buffer,strlen(buffer)+1,0);
	  }
	}
	else{//User left
	  puts("CRASH TURN IT OFF");
	  if (strcmp("off",(*security).value)==0){
	    //Send turn on
	    printf("User left. Turn on\n");
	    snprintf(buffer,sizeof(buffer),"Type:switch;Action:On\0");
	    send((*security).socket,buffer,strlen(buffer)+1,0);
      	  }
	}
	record = 0;
	motion = 0;
	key = 0;
      }
    }
  }
}

//Decodes the message from the client UNICAST
void identify(struct device **client_device,char *msg,int socket){
  //printf("msg: %s\n",msg);
  char * type;
  char * action;
  char buffer[1024];
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
    if (device_count == MULTI_DEVICES && b.registered){//All devices registered
      sendClears();
    }
    //send register to backend
    snprintf(buffer,sizeof(buffer),"Type:insert;Action:Registered:%d:%s",(*client_device)->id,(*client_device)->type);
    send(b.socket,buffer,strlen(buffer)+1,0);
  }
  else if (strcmp(type,"currState")==0){
    if (strcmp(action,"on")==0){
      free((*client_device)->value);
      (*client_device)->value = strdup("on");
      puts("SYSTEM IS ON!!!!");
      snprintf(buffer,sizeof(buffer),"Type:insert;Action:System is on");
      send(b.socket,buffer,strlen(buffer)+1,0);
    }
    else{
      free((*client_device)->value);
      (*client_device)->value = strdup("off");
      puts("SYSTEM IS OFF");
      snprintf(buffer,sizeof(buffer),"Type:insert;Action:System is off");
      send(b.socket,buffer,strlen(buffer)+1,0);
    }
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
      //remove_node(client_sock);

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
  char message[100];
  int reuse = 1;

  /* set up socket */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }
  bzero((char *)&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(GROUP_IP);
  //addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(GROUP_PORT);
  addrlen = sizeof(addr);

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
    perror("setsockopt reuse");
    return 1;
  }
  
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
    printf("RECIVED: %s\n\0", message);
    multi_identify(message);
  }
}

int main(int argc , char *argv[])
{
  //Add arguments
  readConfig(argv[1]);

  //create and clear the file
  FILE *f = fopen(argv[2],"w");
  if (f==NULL){
    printf("Error opening file\n");
    exit(1);
  }

  fclose(f);
  
  //Start Listening on Multichannel
  pthread_t multi;
  g.clock_size = MULTI_DEVICES+1;
  g.clock = malloc((g.clock_size)*sizeof(int));
  memset(g.clock,0,g.clock_size*sizeof(int));
  g.id = MULTI_DEVICES;
  
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
  }
  if (client_sock < 0)
    {
      perror("accept failed");
      return 1;
    }

  return 0;
}
