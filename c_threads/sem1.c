/*
 * Demonstrate use of semaphores for synchronization.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#define NUM_THREADS 10

sem_t semaphore;

/*
 * Thread function to wait on a semaphore.
 */
void *sem_demo (void *arg)
{
   int *pnum = (int *)arg;
   int num = *pnum;
   free(arg);

   printf("Thread %d waiting\n", num);
   if (sem_wait (&semaphore) == -1)
   {
      printf("Wait on semaphore\n");
      exit(1);
   }
   
   printf("Thread %d resuming\n", num);
   return NULL;
}

int main (int argc, char *argv[])
{
   int thread_count;
   pthread_t sem_waiters[NUM_THREADS];
   int status;


   /* initialize semaphore to 0 (3rd parameter) */
   if (sem_init (&semaphore, 0, 0) == -1)
   {
      printf("Init semaphore\n");
      exit(1);
   }

   /*
    * Create n threads to wait concurrently on the semaphore.
    */
   for (thread_count = 0; thread_count < NUM_THREADS; thread_count++) {
      int *pnum = (int*)malloc(sizeof(int));
      *pnum = thread_count;
      status = pthread_create (
          &sem_waiters[thread_count], NULL,
          sem_demo, (void*)pnum);
      if (status != 0)
      {
         printf("Create thread\n");
	 exit(1);
      }
   }

   /* pause before releasing threads */
   sleep (2);

   /*
    * Release the waiting threads by posting to the semaphore 
    * once for each waiting thread. 
    */

   for (thread_count = 0; thread_count < NUM_THREADS; thread_count++) {

      printf("Posting from main\n");
      if (sem_post (&semaphore) == -1)
      {
         printf("Post semaphore\n");
	 exit(1);
      }
   }

   /*
    * Wait for all threads to complete.
    */
   for (thread_count = 0; thread_count < NUM_THREADS; thread_count++) {
      status = pthread_join (sem_waiters[thread_count], NULL);
      if (status != 0)
      {
         printf("Join thread\n");
	 exit(1);
      }
   }
   return 0;
}


