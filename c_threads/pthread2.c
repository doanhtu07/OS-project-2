#include <pthread.h>
#include <string.h>	/* for strerror() */
#include <stdio.h>
#include <stdlib.h>

#define NTHREADS 4

#define errexit(code,str)                          \
  fprintf(stderr,"%s: %s\n",(str),strerror(code)); \
  exit(1);

/******** this is the thread code */
void *hello(void * arg)
{
   /* cast arg to int pointer and dereference it to get
      its value */
   int myid=*(int *) arg;

   printf("Hello, world, I'm %d\n",myid);

   /* return arg back to the join function */
   return arg;
}

/******** this is the main thread's code */
int main(int argc,char *argv[])
{
   int worker;
   pthread_t threads[NTHREADS];                /* holds thread info */
   int ids[NTHREADS];                          /* holds thread args */
   int errcode;                                /* holds pthread error code */
   int *status;                                /* holds return code */

   /* create the threads */
   for (worker=0; worker<NTHREADS; worker++) 
   {
      /* save thread number for this thread in array */
      ids[worker]=worker;

      /* create thread */
      if (errcode=pthread_create(&threads[worker],/* thread struct             */
			 NULL,                    /* default thread attributes */
			 hello,                   /* start routine             */
			 &ids[worker])) {         /* arg to routine            */
	errexit(errcode,"pthread_create");
      }
   }
   
   /* join the threads as they exit */
   for (worker=0; worker<NTHREADS; worker++) {
      if (errcode=pthread_join(threads[worker],(void **) &status)) { 
         errexit(errcode,"pthread_join");
      }
      /* check thread's exit status, should be the same as the 
	 thread number for this example */
      if (*status != worker) {
         fprintf(stderr,"thread %d terminated abnormally\n",worker);
         exit(1);
      }
   }

   return(0);
}




