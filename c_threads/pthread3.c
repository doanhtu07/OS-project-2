#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MATSIZE 4

/* function prototype */
void* matMult( void* );

/* global matrix data */

int mat2[MATSIZE][MATSIZE] =
   { { 1, 2, 3, 4 },
     { 4, 5, 6, 7 },
     { 7, 8, 9, 10 },
     { 10, 11, 12, 13 } };
int mat1[MATSIZE][MATSIZE] =
   { { 9, 8, 7, 6 },
     { 6, 5, 4, 3 },
     { 3, 2, 1, 0 },
     { 0, -1, -2, -3 } };
int result[MATSIZE][MATSIZE];

int main( void )
{
  pthread_t thr[MATSIZE];
  
  int i, j, *column;

  for( i = 0; i < MATSIZE; ++i ) {
     column = (int*)malloc(sizeof(int));
     *column = i;
     pthread_create( &thr[i], NULL, matMult, column ); 
  }

  for( i = 0; i < MATSIZE; ++i ) {
     pthread_join( thr[i], NULL );
  }

  for( i = 0; i < MATSIZE; ++i ) {
     printf( "| " );
     for( j = 0; j < MATSIZE; ++j ) {
	printf( "%3d ", mat1[i][j] );
     }
     printf( "| %c | ", (i==MATSIZE/2 ? 'x' : ' ') );
     for( j = 0; j < MATSIZE; ++j ) {
	printf( "%3d ", mat2[i][j] );
     }
     printf( "| %c | ", (i==MATSIZE/2 ? '=' : ' ' ) );
     for( j = 0; j < MATSIZE; ++j ) {
       printf( "%3d ", result[i][j] );
     }
     printf( "|\n" );
  }

  return 0;
}


void* matMult( void* arg )
{
   int i, j;
   int val;

   int *pcol = (int *)arg;
   int col = *pcol;
   free(arg);
   
   for( i = 0; i < MATSIZE; ++i ) {
      result[i][(int)col] = 0;
      for( j = 0; j < MATSIZE; ++j ) {
	 result[i][(int)col] += mat1[i][j] * mat2[j][(int)col];
      }
   }
   return NULL;
}
   
