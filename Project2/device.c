/*
 * Device file
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
} Gateway;

typedef struct{
  int id;//id num
  char* type;
  int port;
  char* ip;
  int* clock;
}Device;

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

Device alarm; //me
Gateway front;
int max_time;
struct node* list;

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
  front.ip = strdup(strtok(raw,":"));
  token = strtok(NULL,":");
  front.port = atoi(token);
  //printf("port: %d\n",g.port);

  if (getline(&raw, &len, fp) == -1){
    printf("Error file too small.\n");
    exit(0);
  }

  s.type = strdup(strtok(raw,":"));
  s.ip = strdup(strtok(NULL,":"));
  s.port = atoi(strtok(NULL,":"));
  s.area = atoi(strtok(NULL,":"));

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
    entry->start = atoi(strtok(line,","));
    if (strcmp(s.type,"doorSensor")==0){
      entry->end = -1;
    }
    else{
      entry->end = atoi(strtok(NULL,","));
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
  //printf("MAC TIME%d\n",max_time);
  //print list to test
  //printf("TEST: %d,%d,%s\n",entry->start,entry->end,entry->temp);

  free(line);
}

//Thread
//writes the time to the gateway at every interval
void *iterate(void *sock){//add addr stuff
  print_list();
  printf("IM A THREAD: %d\n",pthread_self());
  int gate_sock = *(int*)sock;
  int time = 0;
  struct node* temp;
  struct time_state* temp_val;
  //printf("MAXTIME %d\n",max_time);
  char buffer[1024];

  //TODO: Send Time as well as type
  if (strcmp(s.type,"doorSensor")!=0){
    while(1){
      temp = list;
      while(temp != NULL){
	temp_val = temp->data;
	if (time<=(temp_val->end)&& (time>(temp_val->start)||time==0)){
	  printf("I AM SENDING Time:%d  do %s\n",time,temp_val->state);

	  //Increment Clock
	  s.clock++;
	  snprintf(buffer,sizeof(buffer),"Type:currValue;Action:%s\0",temp_val->state);
	  send(gate_sock,buffer, strlen(buffer)+1, 0);
	}
	temp = temp->next;
      }
      sleep(interval);
      time += interval;
      if (time>max_time){
	time = time-max_time;

      }
    }
  }
  else{
    while(1){
      temp = list;
      while(temp != NULL){
	temp_val = temp->data;
	if (time==(temp_val->start)){
	  //printf("I AM SENDING Time:%d  do %s\n",time,temp_val->temp);

	  //Increment Clock
	  s.clock++;
	  snprintf(buffer,sizeof(buffer),"Type:currValue;Action:%s\0",temp_val->state);
	  send(gate_sock,buffer, strlen(buffer)+1, 0);
	}
	temp = temp->next;
      }
      sleep(1);
      time += interval;
      if (time>max_time){
	time = 0;
      }
    }
  }
}

void* multicast_listener(){
  struct sockaddr_in addr;
  int addrlen, sock, cnt;
  struct ip_mreq mreq;
  char message[50];
  pthread_t thread;

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

  /* if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {

     perror("bind");
     exit(1);
     } */
  mreq.imr_multiaddr.s_addr = inet_addr(GROUP_IP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		 &mreq, sizeof(mreq)) < 0) {
    perror("setsockopt mreq");
    exit(1);
  }

  //start thread
  printf("MORE\n");
  pthread_create(&thread,NULL,iterate,&sock);

  while (1) {
    cnt = recvfrom(sock, message, sizeof(message), 0,
		   (struct sockaddr *) &addr, &addrlen);
    if (cnt < 0) {
      perror("recvfrom");
      exit(1);
    } else if (cnt == 0) {
      break;
    }
    printf("%s: message = \"%s\"\n", inet_ntoa(addr.sin_addr), message);
  }
}

//Decodes the message from the gateway
void identify(int gate_sock,char* command){
  //printf("msg: %s\n",command);
  char * type;
  char * action;
  char buffer[1024];
  pthread_t multi;
  strtok(command,":");
  type = strtok(NULL,":");
  action = strtok(NULL,":");

  printf("action: %s\n",action);

  type = strtok(type,";");

  printf("type: %s\n",type);

  if (type == NULL || action == NULL){
    return 0;
  }

  //Create vector and Start multicast
  if (strcmp(type,"clear")==0){
    s.id = atoi(strtok(action,"-"));
    s.clock = malloc(atoi(strtok(NULL,"-"))*sizeof(int));
    printf("LETS GO \n");
    //Start iterating
    if (pthread_create(&multi,NULL,multicast_listener,NULL) < 0){
      perror("Could not create new thread");
      return 1;
    }
  }
  /*if (strcmp(type,"switch")==0){
    s.state = !s.state;
    if(s.state){//ON
    snprintf(buffer,sizeof(buffer),"Type:currState;Action:on\0");
    if (strcmp(s.type,"sensor")==0){
    //start thread
    pthread_create(&thread,NULL,iterate,&gate_sock);
    }
    }
    else{
    snprintf(buffer,sizeof(buffer),"Type:currState;Action:off\0");
    }
    send(gate_sock,buffer, strlen(buffer)+1, 0);
    }
    else if (strcmp(type,"setInterval")==0){
    if (strcmp(s.type,"sensor")==0){
    interval = atoi(action);
    }
    }*/
}


int main(int argc , char *argv[])
{
  int sock;

  //add arguements
  readConfig(argv[1]);

  //change (should take care of writing only when writing)
  FILE *f = fopen(argv[3],"w");
  if (f==NULL){
    printf("Error opening file\n");
    exit(1);
  }

  struct sockaddr_in server;
  char message[1000] , server_reply[2000];

  //Create socket
  sock = socket(AF_INET , SOCK_STREAM , 0);
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
	identify(sock,server_reply);
      }
    }

  fclose(f);
  close(sock);
  return 0;
}





