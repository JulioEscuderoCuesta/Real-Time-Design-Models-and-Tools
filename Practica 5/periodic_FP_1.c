#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include "ethr_access.h"
#include "timespec_operations.h"
#include "eat.h"

static struct timespec initial_time;

// Structure containing the parameters of each periodic thread
struct periodic_data {
  struct timespec period;  // period
  struct timespec wcet1;   // worst-case execution time for the first part
  struct timespec wcet2;   // worst-case execution time for the second part
  struct timespec phase;   // initial phase to start the thread
  struct timespec wcrt;    // worst-case response time
  sem_t *sem;              // semaphore to wait
  int id;                  // thread identifier
  char* message;           // message to be sent with the thread id
};

// Show a message with the relative elapsed time, and response_time
void report (char * message, int id, struct timespec *response_time) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC,&now);
  decr_timespec(&now,&initial_time);
  printf("%3.3f - %s - %d",(double) (now.tv_sec+
	 now.tv_nsec/1.0e9),message,id);
  if (response_time==NULL) {
    printf("\n");
  } else {
    printf(" - %3.3f\n",(double) (response_time->tv_sec+
				  response_time->tv_nsec/1.0e9));
  }
}

// Periodic thread using nanosleep
void * periodic (void *arg)
{
  struct periodic_data *my_data=(struct periodic_data *)arg;
  struct timespec next_time;
  struct timespec response_time;
  int err;

  my_data->wcrt.tv_sec=0;
  my_data->wcrt.tv_nsec=0;

  // set initial time and wait for the critical instant
  clock_gettime(CLOCK_MONOTONIC, &next_time);
  //next_time=initial_time;
  incr_timespec(&next_time,&(my_data->phase));
  if ((err=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,
                           &next_time,NULL))!=0) 
    {
      printf("Error in clock_nanosleep: %s\n",strerror(err));
      pthread_exit(NULL);
    }
  

  // infinite loop where periodic work is done
  while (1) {
    // periodic work
    //report ("Start thread ",my_data->id,NULL);
    eat(&(my_data->wcet1)); // this simulates useful work
    send(STATION_2,my_data->message); // send the message to the remote CPU
    if (sem_wait(my_data->sem)!=0)    // wait for the event
    {
      printf("Error in sem_wait: %s\n",strerror(err));
      pthread_exit(NULL);
    }
    eat(&(my_data->wcet2)); // this simulates useful work
    clock_gettime(CLOCK_MONOTONIC, &response_time);
    decr_timespec(&response_time,&next_time);
    //report ("End   thread ",my_data->id,&response_time);

    //set wcrt and report it if needed
    if smaller_timespec(&(my_data->wcrt),&response_time)
      {
	my_data->wcrt=response_time; 
      }

    // wait for next period
    incr_timespec(&next_time,&(my_data->period));
    if ((err=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,
                             &next_time,NULL))!=0) 
      {
        printf("Error in clock_nanosleep: %s\n",strerror(err));
        pthread_exit(NULL);
      }
  }
}

// Thread for remote execution
void *remote (void *arg)
{
  struct periodic_data *my_data=(struct periodic_data *)arg;
  struct timespec next_time;
  int err;

  // set initial time and wait for the critical instant
  //clock_gettime(CLOCK_MONOTONIC, &next_time);
  next_time=initial_time;
  incr_timespec(&next_time,&(my_data->phase));
  if ((err=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,
                           &next_time,NULL))!=0) 
    {
      printf("Error in clock_nanosleep: %s\n",strerror(err));
      pthread_exit(NULL);
    }

  // infinite loop waiting to process events
  while (1) {
    if (sem_wait(my_data->sem)!=0)    // wait for the event
    {
      printf("Error in sem_wait: %s\n",strerror(err));
      pthread_exit(NULL);
    }
    //report ("Start thread ",my_data->id,NULL);
    eat(&(my_data->wcet1));     // this simulates useful work
    send(STATION_2,my_data->message); // send the return message to the calling CPU
    //report ("End   thread ",my_data->id,NULL);
  }
}


// Main program that creates two periodic threads
int main ()
{
  pthread_t t1,t4,t5;
  sem_t sem1,sem4,sem5;
  struct sched_param sch_param;
  pthread_attr_t attr;
  struct periodic_data data1,data4,data5;
  char message[1518];
  char message1[1518];
  char message4[1518];
  char message5[1518];
  int counter;

  // initialize the network

  initialize();

  // initialize all semaphores

  if (sem_init(&sem1,0,0)==-1)
  {
    printf("Error while initializing a semaphore\n");
    exit(1);
  }
  if (sem_init(&sem4,0,0)==-1)
  {
    printf("Error while initializing a semaphore\n");
    exit(1);
  }
  if (sem_init(&sem5,0,0)==-1)
  {
    printf("Error while initializing a semaphore\n");
    exit(1);
  }

  // set data for all threads

  data1.period.tv_sec =1;
  data1.period.tv_nsec=500000000;
  data1.wcet1.tv_sec =0;
  data1.wcet1.tv_nsec=430000000;
  data1.wcet2.tv_sec =0;
  data1.wcet2.tv_nsec=150000000;
  data1.phase.tv_sec =3;
  data1.phase.tv_nsec=0;
  data1.sem=&sem1;
  data1.id=1;
  message1[0]='1';
  data1.message=message1;

  data4.wcet1.tv_sec =1;
  data4.wcet1.tv_nsec=0;
  data4.phase.tv_sec =2;
  data4.phase.tv_nsec=0;
  data4.sem=&sem4;
  data4.id=4;
  message4[0]='4';
  data4.message=message4;

  data5.period.tv_sec =3;
  data5.period.tv_nsec=0;
  data5.wcet1.tv_sec =0;
  data5.wcet1.tv_nsec=070000000;
  data5.wcet2.tv_sec =0;
  data5.wcet2.tv_nsec=070000000;
  data5.phase.tv_sec =3;
  data5.phase.tv_nsec=0;
  data5.sem=&sem5;
  data5.id=5;
  message5[0]='5';
  data5.message=message5;

  // Set the priority of the main program to max_prio-1
  sch_param.sched_priority = 
    (sched_get_priority_max(SCHED_FIFO)-1); 
  if (pthread_setschedparam(pthread_self(),SCHED_FIFO,&sch_param) !=0)
    {
      printf("Error while setting main thread's priority\n");
      exit(1);
    }

  // Create the thread attributes object
  if (pthread_attr_init (&attr) != 0) 
    {
      printf("Error while initializing attributes\n");
      exit(1);
    }

  // Set thread attributes

  // Never forget the inheritsched attribute
  // Otherwise the scheduling attributes are not used
  if (pthread_attr_setinheritsched 
      (&attr,PTHREAD_EXPLICIT_SCHED) != 0) 
    { 
      printf("Error in inheritsched attribute\n");
      exit(1);
    }
  
  // Thread is created dettached
  if (pthread_attr_setdetachstate 
      (&attr,PTHREAD_CREATE_DETACHED) != 0) 
    {
      printf("Error in detachstate attribute\n");
      exit(1);
    }

  // The scheduling policy is fixed-priorities, with
  // FIFO ordering for threads of the same priority
  if (pthread_attr_setschedpolicy 
      (&attr, SCHED_FIFO) != 0) 
    {
      printf("Error in schedpolicy attribute\n");
      exit(1);
    }

  // Set the priority of thread 1 to 6554
  sch_param.sched_priority = 6544); 
  if (pthread_attr_setschedparam 
      (&attr,&sch_param) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 1 with the attributes specified in attr_used
    
  if (pthread_create (&t1,&attr,periodic,&data1) != 0) {
    printf("Error en creacion de thread 1\n");
  }

  // Set the priority of thread 4 to min_prio+x
  sch_param.sched_priority = 1); 
  if (pthread_attr_setschedparam 
      (&attr,&sch_param) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 4 with the attributes specified in attr_used
    
  if (pthread_create (&t4,&attr,remote,&data4) != 0) {
    printf("Error en creacion de thread 2\n");
  }

  // Set the priority of thread 5 to min_prio+x
  sch_param.sched_priority = 13107); 
  if (pthread_attr_setschedparam 
      (&attr,&sch_param) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 5 with the attributes specified in attr_used
    
  if (pthread_create (&t5,&attr,periodic,&data5) != 0) {
    printf("Error en creacion de thread 1\n");
  }

  // Read the initial time for use by the report function
  clock_gettime(CLOCK_MONOTONIC,&initial_time);

  counter=0;
  while (1) {
     receive(message);
     switch (message[0]) {
        case '2': 
           sem_post(&sem1);
           break;
        case '3':
           sem_post(&sem4);
           break;
        case '6':
           sem_post(&sem5);
           break;
     }
     counter++;
     if (counter==3)
     {
       report ("Worst-case response time for thread",data1.id,&(data1.wcrt));
       report ("Worst-case response time for thread",data5.id,&(data5.wcrt));
       counter=0;
     }
  }


}
