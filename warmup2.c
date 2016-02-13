#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include "cs402.h"
#include "my402list.h"
#include <sys/stat.h>

typedef struct Packets {
   int p_id;
   int p_tokens_needed;
   double p_service_time;
   
}packetelem;

static char gszProgName[MAXPATHLENGTH];

double r = 1.5; //token deposit time
int num =20; //total number of packets
int P=3; //number of tokens needed
int B=10; //token bucket max capacity
double lambda=1; //packet interarrival time in seconds
double mu=0.35; //packet service time in seconds
int deterministic = 1;
int dropped_tokens = 0;
int available_tokens=0;
int packet_counter =1;
int token_counter =1;
int dropped_packets =0;
//sigset_t set;
//int packets_processed = num;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

My402List Q1;
My402List Q2;

// Below function is called if user provides invalid command
static
void Usage()
{
    fprintf(stderr,
            "usage: %s [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile] \n",
            gszProgName);
    exit(-1);
}


packetelem *Create_Packet(int id,int token_num,double servicetime)
{
    char *error = NULL;
    packetelem *newPacket = (packetelem *)malloc(sizeof(packetelem));
    if(newPacket !=NULL) {
        newPacket->p_id = id;
        newPacket->p_tokens_needed = token_num;
        newPacket->p_service_time = servicetime;
        return newPacket;
    } else {
        error = "Error Allocating memory\n";
	exit(1);
   }
   return newPacket;
}

void *packethandler(void *arg) {
    double p_interarrivaltime = 0;
    double p_servicetime;
    struct timeval currenttime;
    int numoftokens;
    int sig=0;

    gettimeofday(&currenttime, NULL);
    printf("Packethandler thread is running\n");
    for(;;) {
	//sigwait(&set, &sig);
 	printf("Packethandler thread for loop\n");
      if(deterministic) {
	p_interarrivaltime = 1000/lambda; //time in millisecond
        p_servicetime = 1000/mu; //time in millisecond
  	numoftokens = P;
      }else {
	p_interarrivaltime = 1000/lambda; //time in millisecond
        p_servicetime = 1000/mu; //time in millisecond
        numoftokens = P;
      }
      double conv_p_interarrivaltime = 1000*p_interarrivaltime;  //time in microseconds
      double conv_p_servicetime = 1000*p_servicetime; //time in microseconds
      printf("interarrival time is: %.6g\n",p_interarrivaltime);
      printf("service time is: %.6g\n",p_servicetime);	
      //usleep(conv_p_interarrivaltime);
      usleep(1000);
      packetelem *packet = NULL; 
      if(packet_counter < num) {//remove this
      packet = Create_Packet(packet_counter,numoftokens,conv_p_servicetime);
      flockfile (stdout);
      printf("p%d arrives,needs %d tokens, inter-arrival time = %.6gms\n",packet_counter,numoftokens,p_interarrivaltime);
      funlockfile(stdout);
      pthread_mutex_lock(&mutex);
      if(!My402ListEmpty(&Q1)){
	if (numoftokens > B) {
	   flockfile (stdout);
	   printf("p%d arrives,needs %d tokens, inter-arrival time = %.6gms,dropped\n",packet_counter,numoftokens,p_interarrivaltime);
	   funlockfile(stdout);
           dropped_packets++;
        }
	else {
	  flockfile (stdout);
	  printf("p%d enters Q1\n",packet_counter);
	  funlockfile(stdout);
	  (void)My402ListAppend(&Q1,packet);
	  printf ("packet_id at packet handler:%d",packet->p_id);
	}
      } else {
        if (numoftokens > B) {
	   flockfile (stdout);
	   printf("p%d arrives,needs %d tokens, inter-arrival time = %.6gms,dropped\n",packet_counter,numoftokens,p_interarrivaltime);
	   funlockfile(stdout);
	   dropped_packets++;
        }
	else if (available_tokens >= numoftokens) {
	  flockfile (stdout);
	  printf("p%d enters Q1\n",packet_counter);
	  funlockfile(stdout);
	  available_tokens = available_tokens - numoftokens;
	  if (!My402ListEmpty(&Q2)) {
	    flockfile (stdout);
	    printf("p%d leaves Q1\n",packet_counter);
	    printf("p%d enters Q2\n",packet_counter);
	    funlockfile(stdout);
            packet = Create_Packet(packet_counter,numoftokens,conv_p_servicetime);
            (void)My402ListAppend(&Q2,packet);
	    printf ("packet_id at packet handler:%d",packet->p_id);
          } else {
	    flockfile (stdout);
	    printf("p%d leaves Q1\n",packet_counter);
	    printf("p%d enters Q2\n",packet_counter);
	    funlockfile(stdout);
	    packet = Create_Packet(packet_counter,numoftokens,conv_p_servicetime);
            (void)My402ListAppend(&Q2,packet);
	    printf ("packet_id at packet handler:%d",packet->p_id);
            pthread_cond_broadcast(&queue_not_empty);
          }
	}
        else if (available_tokens < numoftokens) {
	  flockfile (stdout);
	  printf("p%d enters Q1\n",packet_counter);
	  funlockfile(stdout);
 	  packet = Create_Packet(packet_counter,numoftokens,conv_p_servicetime);
	  (void)My402ListAppend(&Q1,packet);
	}
      }
     }
      packet_counter++;
      pthread_mutex_unlock(&mutex);
    }
    return 0;
}

void *tokenhandler(void *arg){

    printf("tokenhandler thread is running\n");
  //int sig=0;
  for(;;) {
    //sigwait(&set, &sig);
    printf("tokenhandler thread for loop\n");
    double t_interarrivaltime;  
    struct timeval currenttime;

    t_interarrivaltime = 1000/r;  //time in millisecond
    double conv_t_interarrivaltime = 1000*t_interarrivaltime;  //time in microseconds
    //usleep(conv_t_interarrivaltime);
    usleep(1000);
    pthread_mutex_lock(&mutex);
    if(available_tokens == B) {
	dropped_tokens++;
	flockfile (stdout);
	printf("token t%d arrives,dropped\n",token_counter);
	funlockfile(stdout);
    } else {
	available_tokens++;
	flockfile (stdout);
	printf("token t%d arrives, token bucket now has %d token\n",token_counter,available_tokens);
	funlockfile(stdout);
    }
    token_counter++;
    while(!My402ListEmpty(&Q1)) {
	packetelem *packet = (packetelem *) malloc(sizeof(packetelem));
	My402ListElem *elem=NULL;
	elem = My402ListFirst(&Q1);
	packet = (packetelem *)(elem->obj);
	int tokens_needed = packet->p_tokens_needed;
        int packet_id = packet->p_id;
        int servicetime = packet->p_service_time;
        if(available_tokens >= tokens_needed) {
	  available_tokens = available_tokens - tokens_needed;
   	  //My402ListUnlink(&Q1,elem);
	  flockfile (stdout);
          printf("p%d leaves Q1, time in Q1=%.6g, token bucket now has %d token\n",packet_id,t_interarrivaltime,available_tokens);
	  funlockfile(stdout);
          if (!My402ListEmpty(&Q2)) {
	    packet = Create_Packet(packet_id,tokens_needed,servicetime);
	    (void)My402ListAppend(&Q2,packet);
	    flockfile (stdout);
            printf("p%d enters Q2\n",packet_id);
	    funlockfile(stdout);
	    packetelem *packet1 = (packetelem *) malloc(sizeof(packetelem));
          My402ListElem *elem1=NULL;
          elem1 = My402ListFirst(&Q2);
          packet1 = (packetelem *)(elem1->obj);
	    printf("object in Q2:\n",packet1->p_id);
	    
          } else {
            (void)My402ListAppend(&Q2,packet);
		packetelem *packet1 = (packetelem *) malloc(sizeof(packetelem));
          My402ListElem *elem1=NULL;
          elem1 = My402ListFirst(&Q2);
          packet1 = (packetelem *)(elem1->obj);
            printf("object in Q2:\n",packet1->p_id);
	    flockfile (stdout);
            printf("p%d enters Q2\n",packet_id);
	    funlockfile(stdout);
            pthread_cond_broadcast(&queue_not_empty);
          }
	  My402ListUnlink(&Q1,elem);
	}
	else 
	  break;
    }
    pthread_mutex_unlock(&mutex);
  }
  return 0;
}

void *server1handler(void *arg){

    printf("server1handler thread is running\n");
    //int sig=0;
    for (;;) {
      //sigwait(&set, &sig);	
      pthread_mutex_lock(&mutex);
      while(My402ListEmpty(&Q2)) {
	pthread_cond_wait(&queue_not_empty, &mutex);
	if(!My402ListEmpty(&Q2)) {
	  packetelem *packet = (packetelem *) malloc(sizeof(packetelem));
          My402ListElem *elem=NULL;
          elem = My402ListFirst(&Q2);
          packet = (packetelem *)(elem->obj);
          int packet_id = packet->p_id;
	  printf("at server1:%d\n",packet_id);
	  flockfile (stdout);
	  printf("p%d leaves Q2, time in Q2=\n",packet_id);
          printf("p%d begins service at S1, requesting %.6g of service\n",packet_id,packet->p_service_time);
	  funlockfile(stdout);
	  double servicetime = packet->p_service_time;
	  free(packet);
          My402ListUnlink(&Q2,elem);
 	  pthread_mutex_unlock(&mutex);
	  //usleep(servicetime);
	  usleep(100);
	  flockfile (stdout);
	  printf("p%d departs from S1, service time =%.6g, time in system =%.6g\n",packet_id,servicetime,servicetime+1000000);
	  funlockfile(stdout);
	}
      }
    }   
}

void *server2handler(void *arg){

    printf("server2handler thread is running\n");
    //int sig=0;
    for (;;) {
      //sigwait(&set, &sig);
      pthread_mutex_lock(&mutex);
      while(My402ListEmpty(&Q2)) { //while or if loop
        pthread_cond_wait(&queue_not_empty, &mutex);
        if(!My402ListEmpty(&Q2)) {
          packetelem *packet = (packetelem *) malloc(sizeof(packetelem));
          My402ListElem *elem=NULL;
          elem = My402ListFirst(&Q2);
          packet = (packetelem *)(elem->obj);
          int packet_id = packet->p_id;
 	  double servicetime = packet->p_service_time;
	  printf("at server1:%d\n",packet_id);
	  flockfile (stdout);
          printf("p%d leaves Q2, time in Q2=\n",packet_id);
          printf("p%d begins service at S2, requesting %.6g of service\n",packet_id,packet->p_service_time);
	  funlockfile(stdout);
	  free(packet);
          My402ListUnlink(&Q2,elem);
          pthread_mutex_unlock(&mutex);
          //double servicetime = packet->p_service_time;
          //usleep(servicetime);
	  usleep(100);
	  flockfile (stdout);
          printf("p%d departs from S2, service time =%.6g, time in system =%.6g\n",packet_id,servicetime,servicetime+1000000);
	  funlockfile(stdout);
        }
      }
    }
}

static
void ProcessOptions(int argc, char *argv[])
{
    FILE *fp = NULL;
    struct stat dirbuf;
    double lambda = 0;
    double r =0;
    double mu=0;
    long num=0;
    long B=0;
    long P=0;
    int i=1;
    char *filename;
    if(argc>=1){
     	for (i=1; i< argc; i+2) {
		printf("increment value of i %d\n",i+2);
		printf("argv value is: %s\n",argv[i]);
   	    if((strcmp(argv[i],"-lambda")) && (argc > i+1)) {
		if (sscanf(argv[i+1], "%lf", &lambda) != 1) {
		   error="Please provide a valid value";
		   Handle_error(error); 
		}
		//lambda = *dval;
		printf("lanbda value is: %lf",lambda);
	    } else if(strcmp(argv[i],"-mu") && (argc > i+1)) {
		if (sscanf(argv[i+1], "%lf", &mu) != 1) {
		   error="Please provide a valid value";
		   Handle_error(error);
                }
		//mu = *dval;
		printf("mu value is: %lf",mu);
	    } else if(strcmp(argv[i],"-r") && (argc > i+1)) {
		if (sscanf(argv[i+1], "%lf", &r) != 1) {
		   error="Please provide a valid value";
		   Handle_error(error);
                }
		//r = *dval;
		printf("r value is:%lf",r);
	    } else if(strcmp(argv[i],"-B") && (argc > i+1)) {
		if (sscanf(argv[i+1], "%d", &B) != 1) {
		   error="Please provide a valid value";
		   Handle_error(error);
	        }
		//B=*dval;
		printf("B value is:%d",B);
	    } else if(strcmp(argv[i],"-P") && (argc > i+1)) {
		if (sscanf(argv[i+1], "%d", &P) != 1) {
		   error="Please provide a valid value";
		   Handle_error(error);
		} 
		//P = *dval;
		printf("P value is %d",P);
	    } else if(strcmp(argv[i],"-num") && (argc > i+1)) {
		if (sscanf(argv[i+1], "%d", &num) != 1) {
		   error="Please provide a valid value";
		   Handle_error(error);
		}
		//num= *dval;
		printf("num value is %d",num);
	    } else if (strcmp(argv[i],"-t") && (argc > i+1)) {
		if (sscanf(argv[i+1], "%s", filename) != 1) {
	            error="Please provide a valid filename";
                    Handle_error(error);
		}
		fp = fopen(argv[i+1],"r");
	        if(stat(argv[i+1],&dirbuf) ==0){
                  if((S_ISDIR(dirbuf.st_mode))) {
                     fprintf(stderr,"Error: Please provide file name and not the directory name\n");
                     exit(1);
            	  }
                }
		//fp = *dval;
		deterministic=0;
		//printf("fp value is %s",fp);
	    } else {
		Usage();
	    }
  	}	
        printf("Final values are:\n");
	printf("lambda value is: %f",lambda);
	printf("mu value is: %f",mu);
	printf("r value is:%f",r);
    }
}
 
void InitializeQueue() {
    memset(&Q1, 0, sizeof(My402List));
    (void)My402ListInit(&Q1);
    memset(&Q2, 0, sizeof(My402List));
    (void)My402ListInit(&Q2);
 
}
 
static
void SetProgramName(char *s)
{
    char *c_ptr=strrchr(s, DIR_SEP);

    if (c_ptr == NULL) {
        strcpy(gszProgName, s);
    } else {
        strcpy(gszProgName, ++c_ptr);
    }
}

int main(int argc, char *argv[])
{
    SetProgramName(*argv);
    ProcessOptions(argc, argv);
    //Intialize Q1 and Q2
    InitializeQueue();
    //sigemptyset(&set);
    //sigaddset(&set, SIGINT);
    //sigprocmask(SIG_BLOCK, &set, 0);
    /*
    pthread_t pkthread,ththread,s1thread,s2thread;
    int error=0;
    void *pkresult,*thresult,*s1result,*s2result;

    error = pthread_create(&pkthread, NULL, packethandler, NULL);
    if (error != 0) {
        printf("\ncan't create thread :[%s]", strerror(error));
	exit(1);
    }
    error = pthread_create(&ththread, NULL, tokenhandler, NULL);
    if (error != 0) {
	printf("\ncan't create thread :[%s]", strerror(error));
        exit(1);
    }
    error = pthread_create(&s1thread, NULL, server1handler, NULL);
    if (error != 0) {
        printf("\ncan't create thread :[%s]", strerror(error));
        exit(1);
    }
    error = pthread_create(&s2thread, NULL, server2handler, NULL);
    if (error != 0) {
        printf("\ncan't create thread :[%s]", strerror(error));
        exit(1);
    }

    pthread_join(pkthread,(void **)&pkresult);
    pthread_join(ththread,(void **)&thresult);
    pthread_join(s1thread,(void **)&s1result);
    pthread_join(s2thread,(void **)&s2result);
	*/
    return(0);
}
