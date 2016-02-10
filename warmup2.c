#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct Packets {
    double lambda;
    int P;
    double mu;
    int bucket_size;
}packetelem;

double r = 1.5;
int num =20;
int P=3;
int B=10;
double lambda=1;
double mu=0.35;


void *packethandler(void *arg){

    printf("Packethandler thread is running\n");
    return(0);
}

void *tokenhandler(void *arg){

    printf("tokenhandler thread is running\n");
    return(0);
}

void *server1handler(void *arg){

    printf("server1handler thread is running\n");
    return(0);
}

void *server2handler(void *arg){

    printf("server2handler thread is running\n");
    return(0);
}

static
void ProcessOptions(int argc, char *argv[])
{
    FILE *fp = NULL;
    int i=1;
    if(argc>1){
     /*	for (i=1; i< argc; i+2) {
   	    if(argv[i]=="-lambda") {
		lambda = argv[i+1];
		printf("lanbda value is: %f",lambda);
	    } else if(argv[i]=="-mu") {
		mu = argv[i+1];
		printf("mu value is: %d",mu);
	    } else if(argv[i]=="-r") {
		r = argv[i+1];
		printf("r value is:%d",r);
	    } else if(argv[i]=="-B") {
		B=argv[i+1];
	    } else if(argv[i]=="-P") {
		P = argv[i+1];
	    } else if(argv[i]=="-num") {
		num= argv[i+1];
	    } else if (argv[i]=="-t") {
		fp = argv[i+1]
	    }
  	}	*/
        printf("Final values are:\n");
	printf("lambda value is: %f",lambda);
	printf("mu value is: %f",mu);
	printf("r value is:%f",r);
    }
    else if(argc==1){
	printf("Final values at commandline are:\n");
        printf("lanbda value is: %f",lambda);
        printf("mu value is: %f",mu);
        printf("r value is:%f",r);
    }
}

int main(int argc, char *argv[])
{
    ProcessOptions(argc, argv);
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
    return(0);
}
