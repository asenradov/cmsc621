/*
  C ECHO client example using sockets
*/
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inetcount += 1;count += 1;_addr
#include<stdlib.h>
#include <pthread.h>

typedef struct{
  int port;
  char* ip;
} Gateway;

typedef struct{
  int state;//on or off
  char* type;
  int port;
  int area;
  char* ip;
}Device;

//Holds the times and temps
struct time_temp{
  int start;
  int end;
  char* temp;
};

//Linked list of time_temp
struct node{
  struct time_temp* data;
  struct node* next;
};

Device s;//me
Gateway g;
int interval = 5;//default
int max_time;
struct node* list;

//inserts to list
void insert(struct time_temp* addition){
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
  struct time_temp* temp_data;

  while (temp != NULL){
    temp_data = temp->data;
    //printf("TEST: %d,%d,%s\n",temp_data->start,temp_data->end,temp_data->temp);
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
  g.ip = strdup(strtok(raw,":"));
  token = strtok(NULL,":");
  g.port = atoi(token);
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

  struct time_temp* entry;
  
  while ((getline(&line,&len,fp)) != -1){
    entry = malloc(sizeof(struct time_temp));
    entry->start = atoi(strtok(line,";"));
    entry->end = atoi(strtok(NULL,";"));
    entry->temp = strdup(strtok(NULL,"\n"));
    insert(entry);
  }
  max_time = entry->end;
  //printf("MAC TIME%d\n",max_time);
  //print list to test
  //printf("TEST: %d,%d,%s\n",entry->start,entry->end,entry->temp);
 
  free(line);
}

//Thread
//writes the time to the gateway at every interval
void *iterate(void *sock){
  print_list();
  //printf("IM A THREAD: %d\n",pthread_self());
  int gate_sock = *(int*)sock;
  int time = 0;
  struct node* temp;
  struct time_temp* temp_val;
  //printf("MAXTIME %d\n",max_time);
  char buffer[1024];
  while (s.state){//while it's on
    sleep(interval);
    time += interval;
    if (time>max_time){
      time = time-max_time;
    }
    temp = list;
    while(temp != NULL){
      temp_val = temp->data;
      if (time<=(temp_val->end)&& (time>(temp_val->start)||time==0)){
	//printf("I AM SENDING Time:%d  do %s\n",time,temp_val->temp);
	snprintf(buffer,sizeof(buffer),"Type:currValue;Action:%s\0",temp_val->temp);
	send(gate_sock,buffer, strlen(buffer)+1, 0);
      }
      temp = temp->next;
    }
  }
}

//Decodes the message from the gateway
void identify(int gate_sock,char* command){
  //printf("msg: %s\n",command);
  char * type;
  char * action;
  char buffer[1024];
  pthread_t thread;
  strtok(command,":");
  type = strtok(NULL,":");
  action = strtok(NULL,":");
  
  //printf("action: %s\n",action);

  type = strtok(type,";");
  
  //printf("type: %s\n",type);

  if (type == NULL || action == NULL){
    return 0;
  }

  if (strcmp(type,"switch")==0){
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
  }
}

int main(int argc , char *argv[])
{
  int sock;

  //add arguements
  readConfig(argv[1]);
  if (strcmp(s.type,"sensor")==0){
    readInput(argv[2]);
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
  snprintf(buffer,sizeof(buffer),"Type:register;Action:%s-%s-%d-%d\0",s.type,s.ip,s.port,s.area);
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
     
  close(sock);
  return 0;
}
