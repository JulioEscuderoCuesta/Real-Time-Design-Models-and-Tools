#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include "timespec_operations.h"
#include "eat.h"

static struct timespec initial_time;

// Structure containing the parameters of each periodic thread
struct periodic_data {
  struct timespec period;     // period
  struct timespec wcet;       // worst-case execution time
  struct timespec phase;      // initial phase to start the thread
  struct timespec wcrt;       // worst-case response time
  struct timespec deadline;   //deadline
  int id;                     // thread identifier
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
  struct timespec next_deadline;
  struct timespec response_time;
  int err;

  my_data->wcrt.tv_sec=0;
  my_data->wcrt.tv_nsec=0;

  // set initial time and deadline, and wait for the critical instant
  //clock_gettime(CLOCK_MONOTONIC, &next_time);
  next_time=initial_time;
  incr_timespec(&next_time,&(my_data->phase));
  next_deadline=next_time;
  incr_timespec(&next_deadline,&(my_data->deadline));
  if ((err=pthread_setdeadline(pthread_self(),&next_deadline,
                               CLOCK_MONOTONIC,0))!=0) 
    {
      printf("Error in pthead_setdeadline: %s\n",strerror(err));
      pthread_exit(NULL);
    }
  if ((err=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,
                           &next_time,NULL))!=0) 
    {
      printf("Error in clock_nanosleep: %s\n",strerror(err));
      pthread_exit(NULL);
    }
  

  // infinite loop where periodic work is done
  while (1) {
    // periodic work
    report ("Start thread ",my_data->id,NULL);
    eat(&(my_data->wcet)); // this simulates useful work
    clock_gettime(CLOCK_MONOTONIC, &response_time);
    decr_timespec(&response_time,&next_time);
    report ("End   thread ",my_data->id,&response_time);

    //set wcrt
    if smaller_timespec(&(my_data->wcrt),&response_time)
      {
	my_data->wcrt=response_time;
      }

    // set deadline and wait for next period
    incr_timespec(&next_time,&(my_data->period));
    next_deadline = next_time;
    incr_timespec(&next_deadline,&(my_data->deadline));
    if ((err=pthread_setdeadline(pthread_self(),&next_deadline,
                               CLOCK_MONOTONIC,0))!=0) 
      {
        printf("Error in pthead_setdeadline: %s\n",strerror(err));
        pthread_exit(NULL);
      }
    if ((err=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,
                             &next_time,NULL))!=0) 
      {
        printf("Error in clock_nanosleep: %s\n",strerror(err));
        pthread_exit(NULL);
      }
  }
}


// Main program that creates two periodic threads
int main ()
{
  pthread_t t1,t2,t3,t4;
  struct sched_param sch_param;
  pthread_attr_t attr;
  struct periodic_data data1,data2,data3,data4;

  // set data for all threads

  data1.period.tv_sec =3;
  data1.period.tv_nsec=0;
  data1.wcet.tv_sec =1;
  data1.wcet.tv_nsec=0;
  data1.phase.tv_sec =0;
  data1.phase.tv_nsec=0;
  data1.deadline.tv_sec=3;
  data1.deadline.tv_nsec=0;
  data1.id=1;

  data2.period.tv_sec =4;
  data2.period.tv_nsec=0;
  data2.wcet.tv_sec =1;
  data2.wcet.tv_nsec=0;
  data2.phase.tv_sec =0;
  data2.phase.tv_nsec=0;
  data2.deadline.tv_sec=4;
  data2.deadline.tv_nsec=0;
  data2.id=2;

  data3.period.tv_sec =5;
  data3.period.tv_nsec=0;
  data3.wcet.tv_sec =0;
  data3.wcet.tv_nsec=900000000;
  data3.phase.tv_sec =0;
  data3.phase.tv_nsec=0;
  data3.deadline.tv_sec=2;
  data3.deadline.tv_nsec=500000000;
  data3.id=3;

  data4.period.tv_sec =6;
  data4.period.tv_nsec=0;
  data4.wcet.tv_sec =0;
  data4.wcet.tv_nsec=900000000;
  data4.phase.tv_sec =0;
  data4.phase.tv_nsec=0;
  data4.deadline.tv_sec=2;
  data4.deadline.tv_nsec=0;
  data4.id=4;


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

  // The scheduling policy is EDF for threads with
  // the same priority
  if (pthread_attr_setschedpolicy 
      (&attr, SCHED_EDF) != 0) 
    {
      printf("Error in schedpolicy attribute\n");
      exit(1);
    }

  // Set the relative deadline of thread 1 equals to period
  if (pthread_attr_setreldeadline 
      (&attr,&data1.period) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 1 with the attributes specified in attr_used
    
  if (pthread_create (&t1,&attr,periodic,&data1) != 0) {
    printf("Error en creacion de thread 1\n");
  }

  // Set the relative deadline of thread 2 equals to period
  if (pthread_attr_setreldeadline 
      (&attr,&data2.period) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 2 with the attributes specified in attr_used
    
  if (pthread_create (&t2,&attr,periodic,&data2) != 0) {
    printf("Error en creacion de thread 2\n");
  }

  // Set the relative deadline of thread 3 
  if (pthread_attr_setreldeadline 
      (&attr,&data3.deadline) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 3 with the attributes specified in attr_used
    
  if (pthread_create (&t3,&attr,periodic,&data3) != 0) {
    printf("Error en creacion de thread 3\n");
  }

  // Set the relative deadline of thread 4
  if (pthread_attr_setreldeadline 
      (&attr,&data4.deadline) != 0) 
    {
      printf("Error en atributo schedparam\n");
      exit(1);
    }
  
  // create thread 4 with the attributes specified in attr_used
    
  if (pthread_create (&t4,&attr,periodic,&data4) != 0) {
    printf("Error en creacion de thread 4\n");
  }

  // Read the initial time for use by the report function
  clock_gettime(CLOCK_MONOTONIC,&initial_time);

  while (1) {
     report ("Worst-case response time for thread",data1.id,&(data1.wcrt));
     report ("Worst-case response time for thread",data2.id,&(data2.wcrt));
     report ("Worst-case response time for thread",data3.id,&(data3.wcrt));
     report ("Worst-case response time for thread",data4.id,&(data4.wcrt));
     sleep(5);
  }
}
