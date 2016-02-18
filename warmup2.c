#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include "cs402.h"
#include "my402list.h"
#include <sys/stat.h>

typedef struct Packets {
   int p_id;
   int p_tokens_needed;
   double p_service_time;
   struct timeval GenerationTime;
   struct timeval Q1ArrivalTime;
   struct timeval Q1DepartureTime;
   struct timeval Q2ArrivalTime;
   struct timeval Q2DepartureTime;
}packetelem;

static char gszProgName[MAXPATHLENGTH];

double r = 1.5; //token deposit time
double r_print_val = 1.5;
int num =20; //total number of packets
int P=3; //number of tokens needed
int B=10; //token bucket max capacity
double lambda=1; //packet interarrival time in seconds
double lambda_print_val=1;
double mu=0.35; //packet service time in seconds
double mu_print_val=0.35;
char *filename;
int deterministic = 1;
int dropped_tokens = 0;
int available_tokens=0;
int packet_counter =0;
int token_counter =0;
int dropped_packets =0;
int completed_packets =0;
int removed_packets =0;
struct timeval emulationstart;
double emulationtime =0;
double emulationDuration=0;
double timefromstart=0;
int packet_completed = 0;
FILE *fp = NULL;
pthread_t pkthread,ththread,s1thread,s2thread;
pthread_t user_threadID;
sigset_t new;
void *handler(), interrupt();
struct sigaction act;
double total_int_arr_time=0; //time in milliseconds
double total_service_time=0;
double total_system_time=0;
double total_time_Q1=0;
double total_time_Q2=0;
double total_time_S1=0;
double total_time_S2=0;
int terminateprocess=0;
int all_packet_processed=0;

//sigset_t set;
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

static
void Handle_Error(char *error, int line_num){

    fprintf(stderr,"Input data is invalid\n");
    if(error!=NULL) {
        if(line_num!=0) {
            fprintf(stderr,"Error: %s\n",error);
            fprintf(stderr,"Error at line %d\n",line_num);
        } else
            fprintf(stderr,"Error: %s",error);
    }
    exit(1);
}

packetelem *Create_Packet(int id,int token_num,double servicetime,struct timeval generationTime,struct timeval Q1arrivaltime, struct timeval Q1departtime, struct timeval Q2arrivaltime, struct timeval Q2departtime)
{
    packetelem *newPacket = (packetelem *)malloc(sizeof(packetelem));
    if(newPacket !=NULL) {
        newPacket->p_id = id;
        newPacket->p_tokens_needed = token_num;
        newPacket->p_service_time = servicetime;
	newPacket->GenerationTime = generationTime;
        newPacket->Q1ArrivalTime = Q1arrivaltime;
	newPacket->Q1DepartureTime = Q1departtime;
  	newPacket->Q2ArrivalTime = Q2arrivaltime;
	newPacket->Q2DepartureTime = Q2departtime;
        return newPacket;
    } else {
        fprintf(stdout,"Error Allocating memory\n");
	exit(1);
   }
   return newPacket;
}
 
double structtimeinmicrosec(struct timeval structtoconvert) {
    return structtoconvert.tv_sec*1000000.0 + structtoconvert.tv_usec;
}

double structtimeinmillisec(struct timeval structtoconvert) {
    return structtoconvert.tv_sec*1000.0 + (structtoconvert.tv_usec/1000);
}

double valinmicrosec(double timetoconvert) {
    return 1000000.0/timetoconvert;
}
double converttomilliseconds(double timeinmicro) {
    return timeinmicro/1000;
}

double convertmillitosec(double timeinmilli) {
   return timeinmilli/1000;    
}

double calculatedifffromstart(struct timeval temptime){
    double timeinmicrosec = structtimeinmicrosec(temptime);
    double emulationtime = structtimeinmicrosec(emulationstart);
    double difference = timeinmicrosec - emulationtime;
    return difference;
}

double calculatediff(struct timeval temptime1, struct timeval temptime2){
    double time1inmicrosec = structtimeinmicrosec(temptime1);
    double time2inmicrosec = structtimeinmicrosec(temptime2);
    double difference = time1inmicrosec - time2inmicrosec;
    return difference;
}

static
int IsNumber(char *number){
    int i=0;
    for(i=0;i<strlen(number);i++){
        if(number[i] <48 || number[i] >57){
             return FALSE;
        }
    }
    return TRUE;
}

static
void checkifvalid(char *val) {
   char *error;
   if (strlen(val)>10) {
         error="Please provide a valid value";
         Handle_Error(error,0);
    } else {
      if(strstr(val,".")) {
           error="Please provide a valid value";
           Handle_Error(error,0);
      }
      if(!IsNumber(val)) {
           error="Please provide a valid value";
           Handle_Error(error,0);
      }

      int val_num = atoi(val);

      if(val_num<0 || val_num>2147483647) {
            error="Please provide a valid value";
            Handle_Error(error,0);
      }
   }
}

void checkifvalidvalue(char *token, int line_num) {
    char *error =NULL;
    if(token==NULL || (strcmp(token,"\n")==0)) {
        error = "Invalid file format.";
        Handle_Error(error,line_num);
    }
    if (strlen(token)>10) {
	 error = "Invalid file format.";
	 Handle_Error(error,line_num);
    } else {
      if(strstr(token,".")) {
	   error = "Invalid file format.";
	   Handle_Error(error,line_num);
      }
      if(!IsNumber(token)) {
	   error = "Invalid file format.";
	   Handle_Error(error,line_num);
      }

      int val_num = atoi(token);

      if(val_num<0 || val_num>2147483647) {
	    error = "Invalid interarrival time.";
	    Handle_Error(error,line_num);
      }
   }
}


void *packethandler(void *arg) {
//void packethandler() {
    double p_interarrivaltime = 0;
    double p_servicetime;
    struct timeval beforeSleep,afterpktProc;
    struct timeval bfrQ1arrival,aftQ1departs,bfrQ2arrival,aftQ2departs;
    double bfrsleep =0.0;
    double prevpacket =0.0;
    double meas_interarrivaltime =0.0;
    double sleeptime=0;
    double aftproc = 0;
    int numoftokens=0;
    double temp;
    double diffval;
    int line_num=1;
    int delimiter_count = 0;
    char *file_interarrival=NULL;
    char *file_packets = NULL;
    char *file_service = NULL;
    int val =0;
    char buf[1024];
    char *savestr=NULL;
    char *line_error=NULL;
    char *error=NULL;

    if(deterministic) {
	p_interarrivaltime = valinmicrosec(lambda); //time in microsecond
        p_servicetime = valinmicrosec(mu); //time in microsecond
        numoftokens = P;
    }
    for(;;) {
	//sigwait(&set, &sig);
      if(!deterministic) {
	if(line_num <= num) {
          if((fgets(buf, sizeof(buf), fp))!=NULL) {
	    delimiter_count =0;
	    line_num++;
            file_interarrival = strtok_r(buf," \t",&savestr);	  
	    checkifvalidvalue(file_interarrival,line_num); 
	    val = atoi(file_interarrival);
	    p_interarrivaltime = val*1000;

	    file_packets = strtok_r(NULL," \t",&savestr);              
	    checkifvalidvalue(file_packets,line_num);
	    delimiter_count++;
            val = atoi(file_packets);
            P = val;
	    numoftokens = P;
              
	    file_service = strtok_r(NULL," \t",&savestr);
	    char* temp = strchr(file_service,'\n');
            if (temp)
               *temp = '\0';
	    else {
	       error = "Invalid file format.\n";
               Handle_Error(error,line_num);
	    }
	    checkifvalidvalue(file_service,line_num);
            delimiter_count++;
            val = atoi(file_service);
            p_servicetime = val*1000;

	    line_error = strtok_r(NULL," \t",&savestr);
	    if(line_error!=NULL) {
	      delimiter_count++;
            }

	    if(delimiter_count!=2) {
               error = "Invalid file.\n";
               Handle_Error(error,line_num);
            }
	  } else {
	      error = "Less number of packet information provided in the file." ;
	      Handle_Error(error,line_num);
	  }
	} else {
	    if((fgets(buf, sizeof(buf), fp))!=NULL) {
	      error = "More number of packet information provided in the file.";
              Handle_Error(error,line_num);
	    } 
	}
      }
      //current packet processing starts
      if(packet_counter==num) {
          packet_completed =1;
      }
      if(packet_completed) 
	 pthread_exit(NULL);
      else {
        packet_counter++;
	if(packet_counter==1)
          sleeptime = p_interarrivaltime; //time in microsecond
        else 
          sleeptime = p_interarrivaltime - (bfrsleep-aftproc); //time in microsecond
	
	if(sleeptime >0) 
          usleep(sleeptime);

	gettimeofday(&beforeSleep,NULL);
        bfrsleep = structtimeinmicrosec(beforeSleep);  //time in microsecond

	if(packet_counter==1) {
	  meas_interarrivaltime = converttomilliseconds(calculatedifffromstart(beforeSleep));
          total_int_arr_time+=meas_interarrivaltime;//time in milliseconds
	} else {
          meas_interarrivaltime = bfrsleep - prevpacket; //calculates interarrival time in microseconds
	  meas_interarrivaltime = converttomilliseconds(meas_interarrivaltime); //time in milliseconds
          total_int_arr_time+=meas_interarrivaltime; //time in milliseconds;
	}

	//assign current time to previous to be used by next packet
	prevpacket = bfrsleep;

        if (numoftokens > B) {
	   temp = converttomilliseconds(calculatedifffromstart(beforeSleep));
           flockfile (stdout);
           fprintf(stdout,"%012.3fms: p%d arrives,needs %d tokens, inter-arrival time = %.3fms,dropped\n",temp,packet_counter,numoftokens,meas_interarrivaltime);
           funlockfile(stdout);
           dropped_packets++;
        } else {
      	   packetelem *packet = NULL;
	   temp = converttomilliseconds(calculatedifffromstart(beforeSleep));
      	   flockfile (stdout);
      	   fprintf(stdout,"%012.3fms: p%d arrives,needs %d tokens, inter-arrival time = %.3fms\n",temp,packet_counter,numoftokens,meas_interarrivaltime);
      	   funlockfile(stdout);
      	   pthread_mutex_lock(&mutex);
      	   if(!My402ListEmpty(&Q1)){
	     gettimeofday(&bfrQ1arrival,NULL);
	     temp = converttomilliseconds(calculatedifffromstart(bfrQ1arrival));
	     flockfile (stdout);
	     fprintf(stdout,"%012.3fms: p%d enters Q1\n",temp,packet_counter);
	     funlockfile(stdout);
	     packet = Create_Packet(packet_counter,numoftokens,p_servicetime,beforeSleep,bfrQ1arrival,aftQ1departs,bfrQ2arrival,aftQ2departs);
	     (void)My402ListAppend(&Q1,packet);
           } else {
	     if (available_tokens >= numoftokens) {
            	gettimeofday(&bfrQ1arrival,NULL);
		temp = converttomilliseconds(calculatedifffromstart(bfrQ1arrival));
	    	flockfile (stdout);
	    	fprintf(stdout,"%012.3fms: p%d enters Q1\n",temp,packet_counter);
	    	funlockfile(stdout);
	    	available_tokens = available_tokens - numoftokens;
	    	if (!My402ListEmpty(&Q2)) {
		  gettimeofday(&aftQ1departs,NULL);
		  temp = converttomilliseconds(calculatedifffromstart(aftQ1departs));
		  diffval = converttomilliseconds(calculatediff(aftQ1departs,bfrQ1arrival));
	    	  flockfile (stdout);
	    	  fprintf(stdout,"%012.3fms: p%d leaves Q1, time in Q1=%.3f\n",temp,packet_counter,diffval);
		  gettimeofday(&bfrQ2arrival,NULL);
		  temp = converttomilliseconds(calculatedifffromstart(bfrQ2arrival));
	    	  fprintf(stdout,"%012.3fms: p%d enters Q2\n",temp,packet_counter);
	    	  funlockfile(stdout);
	    	  packet = Create_Packet(packet_counter,numoftokens,p_servicetime,beforeSleep,bfrQ1arrival,aftQ1departs,bfrQ2arrival,aftQ2departs);
            	  (void)My402ListAppend(&Q2,packet);
              	} else {
		  gettimeofday(&aftQ1departs,NULL);
                  temp = converttomilliseconds(calculatedifffromstart(aftQ1departs));
		  diffval = converttomilliseconds(calculatediff(aftQ1departs,bfrQ1arrival));
	    	  flockfile (stdout);
	    	  fprintf(stdout,"%012.3fms: p%d leaves Q1, time in Q1=%.3f\n",temp,packet_counter,diffval);
		  gettimeofday(&bfrQ2arrival,NULL);
                  temp = converttomilliseconds(calculatedifffromstart(bfrQ2arrival));
	    	  fprintf(stdout,"%012.3fms: p%d enters Q2\n",temp,packet_counter);
	    	  funlockfile(stdout);
	    	  packet = Create_Packet(packet_counter,numoftokens,p_servicetime,beforeSleep,bfrQ1arrival,aftQ1departs,bfrQ2arrival,aftQ2departs);
            	  (void)My402ListAppend(&Q2,packet);
            	  pthread_cond_broadcast(&queue_not_empty);
                }
	      } else {
		  gettimeofday(&bfrQ1arrival,NULL);
		  temp = converttomilliseconds(calculatedifffromstart(bfrQ1arrival));
	  	  flockfile (stdout);
	  	  fprintf(stdout,"%012.3fms: p%d enters Q1\n",temp,packet_counter);
	  	  funlockfile(stdout);
	  	  packet = Create_Packet(packet_counter,numoftokens,p_servicetime,beforeSleep,bfrQ1arrival,aftQ1departs,bfrQ2arrival,aftQ2departs);
		  (void)My402ListAppend(&Q1,packet);
              }
	    }
         }
         gettimeofday(&afterpktProc,NULL);
         aftproc = structtimeinmicrosec(afterpktProc);
         pthread_mutex_unlock(&mutex);	
      }
    }
    return 0;
}


void *tokenhandler(void *arg){
   
    struct timeval beforeSleep,afterpktProc;
    struct timeval Q1time,afrQ1remo,Q2time,afttoken,afrQ2remo,generationtime;
    double bfrsleep =0.0;
    double prevpacket =0.0;
    double meas_interarrivaltime =0.0;
    double sleeptime=0;
    double aftproc = 0;
    double temp;
    double t_interarrivaltime;
    int tokens_needed;
    int packet_id;
    double servicetime;
    double timeinQ1;

  //int sig=0;
    for(;;) {
    //sigwait(&set, &sig);

    t_interarrivaltime = valinmicrosec(r); //time in microsecond
    if(token_counter==0)
       sleeptime = t_interarrivaltime; //time in microsecond
    else
       sleeptime = t_interarrivaltime - (bfrsleep-aftproc); //time in microsecond

    if(sleeptime >0)
        usleep(sleeptime); 	

    gettimeofday(&beforeSleep,NULL);
    bfrsleep = structtimeinmicrosec(beforeSleep);  //time in microsecond

    if(token_counter==0)
      meas_interarrivaltime = converttomilliseconds(calculatedifffromstart(beforeSleep));
    else {
      meas_interarrivaltime = bfrsleep - prevpacket; //calculates interarrival time in microseconds
      meas_interarrivaltime = converttomilliseconds(meas_interarrivaltime); //time in milliseconds
    }

    //assign current time to previous to be used by next token 
    prevpacket = bfrsleep;

    pthread_mutex_lock(&mutex);
    
    if (packet_completed && My402ListEmpty(&Q1)) {
	pthread_cond_broadcast(&queue_not_empty);
	pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }

    token_counter++;
    if(available_tokens == B) {
	dropped_tokens++;
	gettimeofday(&afttoken,NULL);
        temp = converttomilliseconds(calculatedifffromstart(afttoken));
	flockfile (stdout);
	fprintf(stdout,"%012.3fms: token t%d arrives,dropped\n",temp,token_counter);
	funlockfile(stdout);
    } else {
	available_tokens++;
	gettimeofday(&afttoken,NULL);
        temp = converttomilliseconds(calculatedifffromstart(afttoken));
	flockfile (stdout);
	fprintf(stdout,"%012.3fms: token t%d arrives, token bucket now has %d token\n",temp,token_counter,available_tokens);
	funlockfile(stdout);
    }
    while(!My402ListEmpty(&Q1)) {
	packetelem *packet = (packetelem *) malloc(sizeof(packetelem));
	My402ListElem *elem=NULL;
	elem = My402ListFirst(&Q1);
	packet = (packetelem *)(elem->obj);
	tokens_needed = packet->p_tokens_needed;
        packet_id = packet->p_id;
        servicetime = packet->p_service_time;
	Q1time = packet->Q1ArrivalTime;
        generationtime = packet->GenerationTime;
        if(available_tokens >= tokens_needed) {
	  available_tokens = available_tokens - tokens_needed;
	  flockfile (stdout);
	  gettimeofday(&afrQ1remo,NULL);
          packet->Q1DepartureTime=afrQ1remo;
          temp = converttomilliseconds(calculatedifffromstart(afrQ1remo));
	  timeinQ1 = converttomilliseconds(calculatediff(afrQ1remo,Q1time));
          fprintf(stdout,"%012.3fms: p%d leaves Q1, time in Q1=%.3f, token bucket now has %d token\n",temp,packet_id,timeinQ1,available_tokens);
	  funlockfile(stdout);
          if (!My402ListEmpty(&Q2)) {
	    gettimeofday(&Q2time,NULL);
	    temp = converttomilliseconds(calculatedifffromstart(Q2time));
	    flockfile (stdout);
            fprintf(stdout,"%012.3fms: p%d enters Q2\n",temp,packet_id);
	    funlockfile(stdout);
	    packet = Create_Packet(packet_id,tokens_needed,servicetime,generationtime,Q1time,afrQ1remo,Q2time,afrQ2remo);
            (void)My402ListAppend(&Q2,packet);
          } else {
	    gettimeofday(&Q2time,NULL);
	    temp = converttomilliseconds(calculatedifffromstart(Q2time));
	    flockfile (stdout);
            fprintf(stdout,"%012.3fms: p%d enters Q2\n",temp,packet_id);
	    funlockfile(stdout);
            pthread_cond_broadcast(&queue_not_empty);
	    packet = Create_Packet(packet_id,tokens_needed,servicetime,generationtime,Q1time,afrQ1remo,Q2time,afrQ2remo);
            (void)My402ListAppend(&Q2,packet);
          }
	  My402ListUnlink(&Q1,elem);
	}
	else 
	  break;
    }
    gettimeofday(&afterpktProc,NULL);
    aftproc = structtimeinmicrosec(afterpktProc);
    pthread_mutex_unlock(&mutex);
  }
  return 0;
}

void *server1handler(void *arg){

    int packet_id;
    double totaltimeinsystem;
    struct timeval pktgeneration;
    struct timeval Q2DepartTime;
    double timeinQ2=0;
    double timeinQ1=0;
    struct timeval Q2arrivaltime,Q1arrivaltime,Q1departtime;
    struct timeval bfrservice,aftservice;
    double timevalue;
    //int sig=0;
    for (;;) {
      //sigwait(&set, &sig);	
      pthread_mutex_lock(&mutex);
      while(My402ListEmpty(&Q2)) {
	if ((packet_completed && My402ListEmpty(&Q1)) || (terminateprocess)) {
          pthread_mutex_unlock(&mutex);
	  all_packet_processed =1;
          pthread_exit(NULL);
        }
	pthread_cond_wait(&queue_not_empty, &mutex);
      }
    
      if(!My402ListEmpty(&Q2)) {
	  packetelem *packet = (packetelem *) malloc(sizeof(packetelem));
          My402ListElem *elem=NULL;
          elem = My402ListFirst(&Q2);
          packet = (packetelem *)(elem->obj);
          packet_id = packet->p_id;
	  pktgeneration = packet->GenerationTime; 
	  Q2arrivaltime = packet->Q2ArrivalTime;
	  Q1departtime = packet->Q1DepartureTime;
          Q1arrivaltime = packet->Q1ArrivalTime;
	  double servicetime = packet->p_service_time;
 	  completed_packets++;
	  gettimeofday(&Q2DepartTime,NULL);
          timeinQ2 = converttomilliseconds(calculatediff(Q2DepartTime,Q2arrivaltime));
          total_time_Q2+=timeinQ2;
          timeinQ1 = converttomilliseconds(calculatediff(Q1departtime,Q1arrivaltime));
          total_time_Q1+=timeinQ1;
          timevalue = converttomilliseconds(calculatedifffromstart(Q2DepartTime));
          flockfile (stdout);
          fprintf(stdout,"%012.3fms: p%d leaves Q2, time in Q2=%.3fms\n",timevalue,packet_id,timeinQ2);
          gettimeofday(&bfrservice,NULL);
          timevalue = converttomilliseconds(calculatedifffromstart(bfrservice));
          fprintf(stdout,"%012.3fms: p%d begins service at S1, requesting %.3fms of service\n",timevalue,packet_id,converttomilliseconds(packet->p_service_time));
          funlockfile(stdout);
	  free(packet);
          My402ListUnlink(&Q2,elem);
 	  pthread_mutex_unlock(&mutex);
	  usleep(servicetime);
	  gettimeofday(&aftservice,NULL);
          timevalue = converttomilliseconds(calculatedifffromstart(aftservice));
          double service_time = converttomilliseconds(calculatediff(aftservice,bfrservice));
          total_service_time+=service_time;
          total_time_S1+=service_time;
	  totaltimeinsystem = converttomilliseconds(calculatediff(aftservice,pktgeneration));
          total_system_time+=totaltimeinsystem;
	  flockfile (stdout);
	  fprintf(stdout,"%012.3fms: p%d departs from S1, service time =%.3fms, time in system =%.3fms\n",timevalue,packet_id,service_time,totaltimeinsystem);
	  funlockfile(stdout);
	  if ((packet_completed && My402ListEmpty(&Q1) && My402ListEmpty(&Q2)) || (terminateprocess)) {
	    all_packet_processed =1;
            pthread_exit(NULL);
          }
      }
    }   
}

void *server2handler(void *arg){

    int packet_id;
    double totaltimeinsystem;
    struct timeval pktgeneration;
    struct timeval Q2DepartTime;
    double timeinQ2=0;
    double timeinQ1=0;
    struct timeval Q2arrivaltime,Q1arrivaltime,Q1departtime;
    struct timeval bfrservice,aftservice;
    double timevalue;

    //int sig=0;
    for (;;) {
      //sigwait(&set, &sig);
      
      pthread_mutex_lock(&mutex);
      while(My402ListEmpty(&Q2)) { //while or if loop
	if ((packet_completed && My402ListEmpty(&Q1)) || (terminateprocess)) {
          pthread_mutex_unlock(&mutex);
	  all_packet_processed =1;
          pthread_exit(NULL);
    	}
        pthread_cond_wait(&queue_not_empty, &mutex);
      }
      if(!My402ListEmpty(&Q2)) {
          packetelem *packet = (packetelem *) malloc(sizeof(packetelem));
          My402ListElem *elem=NULL;
          elem = My402ListFirst(&Q2);
          packet = (packetelem *)(elem->obj);
	  completed_packets++;
          packet_id = packet->p_id;
          pktgeneration = packet->GenerationTime;
          Q1arrivaltime = packet->Q1ArrivalTime;
          Q1departtime = packet->Q1DepartureTime;
          Q2arrivaltime = packet->Q2ArrivalTime;
          double servicetime = packet->p_service_time;
	  gettimeofday(&Q2DepartTime,NULL);
          packet->Q2DepartureTime = Q2DepartTime;
          timeinQ2 = converttomilliseconds(calculatediff(Q2DepartTime,Q2arrivaltime));
          total_time_Q2+=timeinQ2;
          timeinQ1 = converttomilliseconds(calculatediff(Q1departtime,Q1arrivaltime));
          total_time_Q1+=timeinQ1;
          timevalue = converttomilliseconds(calculatedifffromstart(Q2DepartTime));
          flockfile (stdout);
          fprintf(stdout,"%012.3fms: p%d leaves Q2, time in Q2=%.3fms\n",timevalue,packet_id,timeinQ2);
          gettimeofday(&bfrservice,NULL);
          timevalue = converttomilliseconds(calculatedifffromstart(bfrservice));
          fprintf(stdout,"%012.3fms: p%d begins service at S2, requesting %.3fms of service\n",timevalue,packet_id,converttomilliseconds(packet->p_service_time));
          funlockfile(stdout);
          free(packet);
          My402ListUnlink(&Q2,elem);
          pthread_mutex_unlock(&mutex);
          usleep(servicetime);
          gettimeofday(&aftservice,NULL);
          timevalue = converttomilliseconds(calculatedifffromstart(aftservice));
	  double service_time = converttomilliseconds(calculatediff(aftservice,bfrservice));
          total_service_time+=service_time;
	  total_time_S2+=service_time;
          totaltimeinsystem = converttomilliseconds(calculatediff(aftservice,pktgeneration));
	  total_system_time+=totaltimeinsystem;
          flockfile (stdout);
          fprintf(stdout,"%012.3fms: p%d departs from S2, service time =%.3fms, time in system =%.3fms\n",timevalue,packet_id,service_time,totaltimeinsystem);
          funlockfile(stdout);
	  if ((packet_completed && My402ListEmpty(&Q1)) && My402ListEmpty(&Q2) || (terminateprocess)) {
	    all_packet_processed =1;
            pthread_exit(NULL);
          }
      }
    }
}

static
void ProcessOptions(int argc, char *argv[])
{
    struct stat dirbuf;
    int i=1;
    char *error;
    char *val;
    char buf[1024];

    if(argc>1){
     	for (i=1; i< argc; i=i+2) {
   	    if((strcmp(argv[i],"-lambda")==0) && (argc > i+1)) {
		if (sscanf(argv[i+1], "%lf", &lambda) != 1) {
		   error="Please provide a valid value";
		   Handle_Error(error,0); 
		}
		if(lambda<0) {
		   error="Please provide a valid value";
		   Handle_Error(error,0);
		}
		double temp_lambda = 1/lambda; //in seconds
		if(temp_lambda > 10) {
		  lambda_print_val = lambda;
		  lambda = 0.1; //figure this out
		}
	    } else if((strcmp(argv[i],"-mu")==0) && (argc > i+1)) {
		if (sscanf(argv[i+1], "%lf", &mu) != 1) {
		   error="Please provide a valid value";
		   Handle_Error(error,0);
                }
		if (mu<0) {
		   error="Please provide a valid value";
		   Handle_Error(error,0);
		}
		double temp_mu = 1/mu; //in seconds
		if(temp_mu >10) {
		   mu_print_val = mu;
		   mu =0.1;
	        }
	    } else if((strcmp(argv[i],"-r")==0) && (argc > i+1)) {
		if (sscanf(argv[i+1], "%lf", &r) != 1) {
		   error="Please provide a valid value";
		   Handle_Error(error,0);
                }
		if(r<0) {
		   error="Please provide a valid value";
		   Handle_Error(error,0);
		}
		double temp_r = 1/r; //in seconds
		if(temp_r >10) {
		   r_print_val = r;
		   r=0.1;
		}
	    } else if((strcmp(argv[i],"-B")==0) && (argc > i+1)) {
		val = argv[i+1];
		checkifvalid(val);
		B=atoi(val);
	    } else if((strcmp(argv[i],"-P")==0) && (argc > i+1)) {
		val = argv[i+1];
		checkifvalid(val);
		P=atoi(val);
	    } else if((strcmp(argv[i],"-num")==0) && (argc > i+1)) {
		val = argv[i+1];
                checkifvalid(val);
                num=atoi(val);
	    } else if ((strcmp(argv[i],"-t")==0) && (argc > i+1)) {
		if(stat(argv[i+1],&dirbuf) ==0){
                  if((S_ISDIR(dirbuf.st_mode))) {
                     fprintf(stderr,"Error: Please provide file name and not the directory name\n");
                     exit(1);
                  }
                }

		filename = argv[i+1];
		fp = fopen(argv[i+1],"r");
		if(fp==NULL){
        	   perror("Error");
            	   exit(1);
        	}
		if((fgets(buf, sizeof(buf), fp))!=NULL) {
		   char* temp = strchr(buf,'\n');
    		   if (temp)
      		     *temp = '\0';
		   checkifvalidvalue(buf,1);
		   num=atoi(buf);
		}
		deterministic=0;
	    } else {
		Usage();
	    }
  	}	
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

void PrintValues() {
    fprintf(stdout,"Emulation Parameters:\n");
    fprintf(stdout,"\tnumber to arrive = %d\n",num);
    if(deterministic){
	fprintf(stdout,"\tlambda = %f\n",lambda_print_val);
	fprintf(stdout,"\tmu = %f\n",mu_print_val);
    }
    fprintf(stdout,"\tr = %f\n",r_print_val);
    fprintf(stdout,"\tB = %d\n",B);
    if(deterministic)
	fprintf(stdout,"\tP = %d\n",P);
    if(!deterministic)
	fprintf(stdout,"\ttsfile = %s\n",filename);

}

void PrintStatistics() {

    //convert all the time to seconds
    double avg_service_time,avg_total_time_Q1,avg_total_time_Q2,avg_total_time_S1,avg_total_time_S2;
    double avg_int_arr_time,avg_system_time,std_dev_system_time,token_drop_prob,packet_drop_prob;

    fprintf(stdout,"\nStatistics:\n");
    if(packet_counter == 0) {
	fprintf(stdout,"\t average packet inter-arrival time = Not Available (No packets arrived into the system)\n");
    } else {
        avg_int_arr_time = total_int_arr_time/(double)packet_counter;
	avg_int_arr_time = convertmillitosec(avg_int_arr_time);
        fprintf(stdout,"\t average packet inter-arrival time = %.6g\n",avg_int_arr_time);
    }
    if(completed_packets == 0) {
	fprintf(stdout,"\t average packet service time = Not Available (No packets were serviced during the emulation)\n");
    } else {
   	avg_service_time = total_service_time/(double)completed_packets;
        avg_service_time = convertmillitosec(avg_service_time);
	fprintf(stdout,"\t average packet service time = %.6g\n",avg_service_time);
    }

    fprintf(stdout,"\n");

    if(total_time_Q1 == 0) {
        fprintf(stdout,"\t average number of packets at Q1 = Not Available (No packets entered Q2)\n");
    }
    else {
	avg_total_time_Q1 = total_time_Q1/emulationDuration;
        avg_total_time_Q1 = convertmillitosec(avg_total_time_Q1);
        fprintf(stdout,"\t average number of packets at Q1 = %.6g\n",avg_total_time_Q1);
    }

    if(total_time_Q2 == 0) {
        fprintf(stdout,"\t average number of packets at Q2 = Not Available (No packets entered Q2)\n");
    }
    else {
	avg_total_time_Q2 = total_time_Q2/emulationDuration;
        avg_total_time_Q2 = convertmillitosec(avg_total_time_Q2);
        fprintf(stdout,"\t average number of packets at Q2 = %.6g\n",avg_total_time_Q2);
    }

    if(total_time_S1 == 0) {
	fprintf(stdout,"\t average number of packets at S1 = Not Available (No packets entered S1 for service)\n");
    } else {
	avg_total_time_S1 = total_time_S1/emulationDuration;
	avg_total_time_S1 = convertmillitosec(avg_total_time_S1);
	fprintf(stdout,"\t average number of packets at S1 = %.6g\n",avg_total_time_S1);
    }
  
    if(total_time_S2 == 0) {
        fprintf(stdout,"\t average number of packets at S2 = Not Available (No packets entered S1 for service)\n");
    } else {
	avg_total_time_S2 = total_time_S2/emulationDuration;
        avg_total_time_S2 = convertmillitosec(avg_total_time_S2);
        fprintf(stdout,"\t average number of packets at S2 = %.6g\n",avg_total_time_S2);
    }
 
    fprintf(stdout,"\n");

    if(completed_packets == 0) {
        fprintf(stdout,"\t average time a packet spent in system = Not Available (No packets were serviced during the emulation)\n");
    } else {
	avg_system_time = total_system_time/(double)completed_packets;
        avg_system_time = convertmillitosec(avg_system_time);
	fprintf(stdout,"\t average time a packet spent in system = %.6g\n",avg_system_time);
    }
 
    if(completed_packets == 0) {
	fprintf(stdout,"\t standard deviation for time spent in system = Not Available (No packets were serviced during the emulation)\n");
    } else {
	double val1 = total_system_time * total_system_time;
        double val2 = val1/(double)completed_packets;
        double val3 = val2 * val2;
        double val4 = val2-val3;
        //std_dev_system_time = sqrt(val4);
	fprintf(stdout,"\t standard deviation for time spent in system = %.6g\n",val4);
    }
    fprintf(stdout,"\n");
    if(token_counter ==0) {
	fprintf(stdout,"\t token drop probability = Not Available (No tokens were generated)\n");
    } else {
   	token_drop_prob = (double)dropped_tokens/(double)token_counter;
    	fprintf(stdout,"\t token drop probability = %.6g\n",token_drop_prob);
    }

    if(packet_counter==0) {
	fprintf(stdout,"\t packet drop probability = Not Available (No packets were generated)\n");
    } else {
 	packet_drop_prob = (double)dropped_packets/(double)packet_counter;
    	fprintf(stdout,"\t packet drop probability = %.6g\n",packet_drop_prob);
    }

}

void *
handler()
{
    while(1) {
      act.sa_handler = interrupt;
      sigaction(SIGINT, &act, NULL);
      pthread_sigmask(SIG_UNBLOCK, &new, NULL);
    }
}

void
interrupt(int sig) {
    terminateprocess = 1;
    pthread_cancel(pkthread);
    pthread_cancel(ththread);
    pthread_cond_broadcast(&queue_not_empty);
    all_packet_processed =1;
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
    struct timeval emulationend;
    SetProgramName(*argv);
    ProcessOptions(argc, argv);
    //Intialize Q1 and Q2
    InitializeQueue();
    PrintValues();
    fprintf(stdout,"\n00000000.000ms: emulation begins\n");  
    gettimeofday(&emulationstart,NULL);

    sigemptyset(&new);
    sigaddset(&new, SIGINT);
    pthread_sigmask(SIG_BLOCK, &new, NULL);
    
    //pthread_t pkthread,ththread,s1thread,s2thread;
    int error=0;
    void *pkresult,*thresult,*s1result,*s2result;
    error = pthread_create(&pkthread, NULL, packethandler, NULL);
    if (error != 0) {
        fprintf(stdout,"\ncan't create thread :[%s]", strerror(error));
	exit(1);
    }
    error = pthread_create(&ththread, NULL, tokenhandler, NULL);
    if (error != 0) {
	fprintf(stdout,"\ncan't create thread :[%s]", strerror(error));
        exit(1);
    }
    error = pthread_create(&s1thread, NULL, server1handler, NULL);
    if (error != 0) {
        fprintf(stdout,"\ncan't create thread :[%s]", strerror(error));
        exit(1);
    }
    error = pthread_create(&s2thread, NULL, server2handler, NULL);
    if (error != 0) {
        fprintf(stdout,"\ncan't create thread :[%s]", strerror(error));
        exit(1);
    }

    pthread_create(&user_threadID, NULL, handler, NULL);
    if(all_packet_processed == 1) {
      pthread_join(user_threadID, NULL);
    }
    pthread_join(pkthread,(void **)&pkresult);
    pthread_join(ththread,(void **)&thresult);
    pthread_join(s1thread,(void **)&s1result);
    pthread_join(s2thread,(void **)&s2result);
    gettimeofday(&emulationend,NULL);
    emulationDuration = converttomilliseconds(calculatedifffromstart(emulationend));
    fprintf(stdout,"%012.3fms: emulation ends\n",emulationDuration);
    PrintStatistics();
    return(0);
}
