/*
  Sensor file.
*/
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inetcount += 1;count += 1;_addr
#include<stdlib.h>
#include <pthread.h>

#define GROUP_PORT 6000
#define GROUP_IP "239.0.0.1"

typedef struct{
  int port;
  char* ip;
  int sock;
} Gateway;

typedef struct{
  struct sockaddr_in addr;
  int addrlen;
  int sock;
} Multichannel;

typedef struct{
  int id;//id num
  char* type;
  int port;
  char* ip;
  int* clock;
  int* state;
  int clock_size;
}Sensor;

//Holds the times and temps
struct time_state{
  int start;
  int end;
  char* state;
};

//Linked list of time_temp
struct node{
  struct time_state* data;
  struct node* next;
};

Multichannel m;//the multichannel
Sensor s;//me
Gateway g;
int interval = 5;//default
int max_time;
struct node* list;
pthread_mutex_t mutex;

// Global pointer to the file
FILE *f;

//Prototype
void identify(char* command);

//inserts to list
void insert(struct time_state* addition){
  struct node* temp = list;
  if(list == NULL){
    list = malloc(sizeof(struct node));
    list -> data = addition;
    list -> next = NULL;
  }
  else{
    while (temp->next!= NULL){
      temp = temp->next;
    }
    temp -> next = malloc(sizeof(struct node));
    temp = temp -> next;
    temp -> data = addition;
    temp -> next = NULL;
  }
}
//Debugger
void print_list(){
  struct node* temp = list;
  struct time_state* temp_data;

  while (temp != NULL){
    temp_data = temp->data;
    printf("TEST: %d,%d,%s\n",temp_data->start,temp_data->end,temp_data->state);
    temp=temp->next;
  }
}

//Reads in config file
void readConfig(char* file){
  char *raw = NULL;
  char *token;
  FILE *fp;
  size_t len = 0;
  
  fp = fopen(file,"r");
  if(fp == NULL){
    printf("Error opening file.\n");
    exit(0);
  }
  
  if (getline(&raw, &len, fp) == -1){
    printf("Error file too small.\n");
    exit(0);
  }
  
  //printf("%s\n",raw);
  g.ip = strdup(strtok(raw,","));
  token = strtok(NULL,",");
  g.port = atoi(token);
  //printf("port: %d\n",g.port);

  if (getline(&raw, &len, fp) == -1){
    printf("Error file too small.\n");
    exit(0);
  }
  
  s.type = strdup(strtok(raw,","));
  s.ip = strdup(strtok(NULL,","));
  s.port = atoi(strtok(NULL,","));

  //set interate for door
  if(strcmp(s.type,"doorSensor")==0){
    interval=1;
  }
  free(raw);
  fclose(fp);
}

//Read in the input file
void readInput(char* file){
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  fp = fopen(file,"r");

  if(fp == NULL){
    printf("Error opening file.\n");
    exit(0);
  }

  struct time_state* entry;
  
  while ((getline(&line,&len,fp)) != -1){
    entry = malloc(sizeof(struct time_state));
    entry->start = atoi(strtok(line,";"));
    if (strcmp(s.type,"doorSensor")==0){      
      entry->end = -1;
    }
    else{
      entry->end = atoi(strtok(NULL,";"));
    }
    entry->state = strdup(strtok(NULL,"\n"));
    insert(entry);
  }
  if (strcmp(s.type,"doorSensor")!=0){ 
    max_time = entry->end;
  }
  else{
    max_time = entry->start;
  }
  max_time++;//add one cuz inclusive
  //printf("MAC TIME%d\n",max_time);
  //print list to test
  //printf("TEST: %d,%d,%s\n",entry->start,entry->end,entry->temp);
 
  free(line);
}

//Thread
//writes the time to the gateway at every interval
void *iterate(){//add addr stuff
  //print_list();
  //printf("IM A THREAD: %d\n",pthread_self());
  int time = 0;
  struct node* temp;
  struct time_state* temp_val;
  //printf("MAXTIME %d\n",max_time);
  char buffer[1024];
  char tempbuf[60];
  int a;
  
  //TODO: Send Time as well as type
  while(1){
    temp = list;
    while(temp != NULL){
      temp_val = temp->data;
      //Works with doorSensor
      if (time == (temp_val->start) || time<=(temp_val->end)&& (time>=(temp_val->start))){
	
	//Increment Clock
	pthread_mutex_lock(&mutex);
	s.clock[s.id]++;
	pthread_mutex_unlock(&mutex);

	snprintf(buffer,sizeof(buffer),"Type:currValue;Action:%d",s.clock[0]);
	for(a=1; a< s.clock_size;a++){
	  snprintf(tempbuf,sizeof(tempbuf),",%d",s.clock[a]);
	  strcat(buffer,tempbuf);
	}
	snprintf(tempbuf,sizeof(tempbuf),"-%d",s.id);
	strcat(buffer,tempbuf);
	
	snprintf(tempbuf,sizeof(tempbuf),"-%s",temp_val->state);
	strcat(buffer,tempbuf);
	//set new state
	free(s.state);
	s.state = strdup(temp_val->state);

	snprintf(tempbuf,sizeof(tempbuf),"-%s\0",s.type);
	strcat(buffer,tempbuf);
	
	fprintf(f,"Sending at time:%d Msg: %s\n",time,buffer);
	fflush(f);
	
	sendto(m.sock,buffer, strlen(buffer)+1, 0,(struct sockaddr*)&m.addr,m.addrlen);
      }
      temp = temp->next;
    }
    sleep(interval);
    time += interval;
    if (time>max_time){
      time = time%max_time;     
    }
  }
}

void* multicast_listener(){
  struct sockaddr_in addr;
  int addrlen, sock, cnt;
  struct ip_mreq mreq;
  char message[1024];
  int reuse = 1;
  //int loop = 0;

  /* set up socket */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }
  bzero((char *)&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(GROUP_IP);
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
  
  //reocrd data
  m.addr = addr;
  m.addrlen = addrlen;
  m.sock = sock;
  	
  while (1) {
    cnt = recvfrom(sock, message, sizeof(message), 0, 
		   (struct sockaddr *) &addr, &addrlen);
    if (cnt < 0) {
      perror("recvfrom");
      exit(1);
    } else if (cnt == 0) {
      break;
    }
    
    fprintf(f,"Recieved: %s\n", message);
    fflush(f);
    identify(message);
  }
}

//Decodes the message from the gateway
void identify(char* command){
  //printf("msg: %s\n",command);
  char * type;
  char * action;
  char buffer[1024];
  char tempbuf[1024];
  int* temp_clock[s.clock_size];
  int a;
  strtok(command,":");
  type = strtok(NULL,":");
  action = strtok(NULL,":"); 
  //printf("action: %s\n",action);
  type = strtok(type,";");
  //printf("type: %s\n",type);

  if (type == NULL || action == NULL){
    return 0;
  }

  //Create vector
  if (strcmp(type,"clear")==0){
    s.id = atoi(strtok(action,"-"));
    s.clock_size = atoi(strtok(NULL,"-"));
    s.clock = malloc(s.clock_size*sizeof(int));
    memset(s.clock,0,s.clock_size*sizeof(int));
    s.state = strdup("Close");
    
    //start thread
    pthread_t thread;
    pthread_create(&thread,NULL,iterate,NULL);
  
  }
  else if (strcmp(type,"currValue")==0){
    //Parse
    (strtok(action,"-"));
    if(s.id!=atoi(strtok(NULL,"-"))){//Discard Loopback
            
      temp_clock[0] = (atoi(strtok(action,",")));
      for (a = 1; a<s.clock_size; a++){
	temp_clock[a] = (atoi(strtok(NULL,",")));
      }

      //synch clock
      pthread_mutex_lock(&mutex);
      //increment own clock upon recieving
      s.clock[s.id]++;

      for (a=0; a<s.clock_size; a++){
	if (temp_clock[a]>s.clock[a]){
	  s.clock[a] = temp_clock[a];
	}
      }
      pthread_mutex_unlock(&mutex);

      fprintf(f,"NEW CLOCK: ");
      fprintf(f,"%d",s.clock[0]);
      for (a=1; a<s.clock_size;a++){
	fprintf(f,",%d",s.clock[a]);
      }
      fprintf(f,"\n");
      fflush(f);
    }
    else{
      fprintf(f,"Discarding loopback\n");
      fflush(f);
    }
  }
  else if(strcmp(type,"currState")==0){//Been polled. Send value
    temp_clock[0] = (atoi(strtok(action,",")));
    for (a = 1; a<s.clock_size; a++){
      temp_clock[a] = (atoi(strtok(NULL,",")));
    }
    
    //synch clock
    pthread_mutex_lock(&mutex);
    //increment own clock upon recieving
    s.clock[s.id]++;
    
    for (a=0; a<s.clock_size; a++){
      if (temp_clock[a]>s.clock[a]){
	  s.clock[a] = temp_clock[a];
      }
    }
    pthread_mutex_unlock(&mutex);
    
    fprintf(f,"NEW CLOCK: ");
    fprintf(f,"%d",s.clock[0]);
    for (a=1; a<s.clock_size;a++){
      fprintf(f,",%d",s.clock[a]);
    }
    fprintf(f,"\n");
    fflush(f);

    //Increment Clock
    pthread_mutex_lock(&mutex);
    s.clock[s.id]++;
    pthread_mutex_unlock(&mutex);
    
    snprintf(buffer,sizeof(buffer),"Type:currValue;Action:%d",s.clock[0]);
    for(a=1; a< s.clock_size;a++){
      snprintf(tempbuf,sizeof(tempbuf),",%d",s.clock[a]);
      strcat(buffer,tempbuf);
    }
    snprintf(tempbuf,sizeof(tempbuf),"-%d",s.id);
    strcat(buffer,tempbuf);
    
    snprintf(tempbuf,sizeof(tempbuf),"-%s",s.state);
    strcat(buffer,tempbuf);
	
    snprintf(tempbuf,sizeof(tempbuf),"-%s\0",s.type);
    strcat(buffer,tempbuf);
    
    fprintf(f,"Sending at time: %d Msg: %s\n",time,buffer);
    fflush(f);
    
    sendto(m.sock,buffer, strlen(buffer)+1, 0,(struct sockaddr*)&m.addr,m.addrlen);
  }
}


int main(int argc , char *argv[])
{
  int sock;

  //add arguements
  readConfig(argv[1]);
  readInput(argv[2]);

  //create and clear the file
  f = fopen(argv[3],"w+");
  if (f==NULL){
    printf("Error opening file\n");
    exit(1);
  }
  
  //Start listening on multicast
  pthread_t multi;
  if (pthread_create(&multi,NULL,multicast_listener,NULL) < 0){
    perror("Could not create new thread");
    return 1;
  }
  
  struct sockaddr_in server;
  char message[1000] , server_reply[1024];
     
  //Create socket
  sock = socket(AF_INET , SOCK_STREAM , 0);
  g.sock = sock;
  
  if (sock == -1)
    {
      printf("Could not create socket");
    }
  //puts("Socket created");
  
  server.sin_addr.s_addr = inet_addr(g.ip);
  server.sin_family = AF_INET;
  server.sin_port = htons(g.port);
 
  //Connect to remote server
  if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
      perror("connect failed. Error");
      return 1;
    }
     
  //puts("Connected\n");

  //Register
  char buffer[1024];
  snprintf(buffer,sizeof(buffer),"Type:register;Action:%s-%s-%d\0",s.type,s.ip,s.port);
  send(sock,buffer,strlen(buffer)+1,0);

  //keep communicating with server

  while(1)
    {
      if( recv(sock , server_reply , 2000 , 0) < 0)
        {
	  puts("recv failed");
	  break;
        }
      else{
	identify(server_reply);
      }
    }

  close(sock);
  return 0;
}
