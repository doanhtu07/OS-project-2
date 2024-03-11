#include <pthread.h>   
#include <stdio.h>

/* The function that runs when the thread is created  */
void* print_thread(void*);

int main()
{
   pthread_t tid;              /* thread ID structure */
   char message_main[]={"Hello, "};   
   char message_thread[]={"thread"};

   /* print hello */
   printf(message_main);

   /* Create a thread. ID is returned in &tid.
      The last parameter is being passed to  
      the thread function.                            */
   pthread_create(&tid, NULL, print_thread, message_thread);

   /* wait for thread to finish */
   pthread_join(tid, NULL);

   return 0;
}

/* The thread function */
void* print_thread(void* input)
{
   printf("%s\n", (char*)input);
   return NULL;
}
