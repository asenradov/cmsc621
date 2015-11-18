/*
  Device file.
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
  int clock_size;
  char* state;
} Device;
  
Multichannel m;//the multichannel
Device s;//me
Gateway g;
pthread_mutex_t mutex;

// Global pointer to the file
FILE *f;

//Prototype
void identify(char* command);

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

  free(raw);
  fclose(fp);
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
    
    identify(message);
  }
}

//Decodes the message from the gateway
void identify(char* command){
  //printf("msg: %s\n",command);
  char preparse[1024];
  char * type;
  char * action;
  char buffer[1024];
  char tempbuf[1024];
  int a;
  strcpy(preparse,command);
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
    fprintf(f,"Recieved: %s\n", preparse);
    fflush(f);
    s.id = atoi(strtok(action,"-"));
    s.clock_size = atoi(strtok(NULL,"-"));
    s.clock = malloc(s.clock_size*sizeof(int));
    memset(s.clock,0,s.clock_size*sizeof(int));
    s.state = strdup("off");//off
  }
  else if (strcmp(type,"currValue")==0){
    int* temp_clock[s.clock_size];
    //Parse
    (strtok(action,"-"));
    if(s.id!=atoi(strtok(NULL,"-"))){//Discard Loopback
      fprintf(f,"Recieved: %s\n", preparse);
      fflush(f);
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

       fprintf(f,"New clock: ");
       fprintf(f,"%d",s.clock[0]);
       for (a=1; a<s.clock_size;a++){
	 fprintf(f,",%d",s.clock[a]);
       }
       fprintf(f,"\n");
       fflush(f);
    }
  }
  else if (strcmp(type,"switch")==0){//set value
    fprintf(f,"Recieved: %s\n", preparse);
    fflush(f);
    char * clock;
    int* temp_clock[s.clock_size];
    fprintf(f,"Switching!\n");
    fflush(f);
    strtok(action,"-");
    clock = strtok(NULL,"-");
    
    if (strcmp(action,"on")==0){
      free(s.state);
      s.state = strdup("on");
      fprintf(f,"Turning on\n");
      fflush(f);
    }
    else{
      free(s.state);
      s.state = strdup("off");
      fprintf(f,"Turning off\n");
      fflush(f);
    }

    temp_clock[0] = (atoi(strtok(clock,",")));
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
      
    //Increment Clock and send
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
    
    fprintf(f,"Sending: %s\n",buffer);
    fflush(f);
    
    sendto(m.sock,buffer, strlen(buffer)+1, 0,(struct sockaddr*)&m.addr,m.addrlen);
  }
}


int main(int argc , char *argv[])
{
  int sock;

  //add arguements
  readConfig(argv[1]);

  //create and clear the file
  f = fopen(argv[2],"w+");
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
      if( recv(sock , server_reply , 1024 , 0) < 0)
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
