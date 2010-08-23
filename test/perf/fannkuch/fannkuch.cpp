/*
 * The Computer Language Benchmarks Game
 * http://shootout.alioth.debian.org/
 *
 * Contributed by Andrew Moon
 * Based on the C++ code by The Anh Tran
 * Based on the C code by Eckehard Berns
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <algorithm>
#include <pthread.h>

struct worker {
   worker( int ndigits, int pos, worker *head = NULL ) :
      digits(ndigits), pos_right(pos), next(head) {}

   inline int countflips() {
      if ( !p[0] )
         return 0;
      if ( !p[p[0]] )
         return 1;

      int tmp[16], flips = 0, last = p[0];
      memcpy( tmp, p, digits * sizeof( int ) );
      do {
         if ( !tmp[last] )
            return flips + 1;
         for ( int lo = 1, hi = last - 1; lo < hi; lo++, hi-- )
            std::swap( tmp[lo], tmp[hi] );
         std::swap( tmp[last], last );
         flips++;
      } while ( last );
      return flips;
   }

   inline void permute() {
      int tmp = p[0];
      for ( int i = 0; i < pos_left; i++ )
         p[i] = p[i + 1];
      p[pos_left] = tmp;
   }

   bool print( int &left ) {
      if ( left-- > 0 ) {
         for ( int i = 0; i < digits; i++ )
            printf( "%d", p[i] + 1 );
         printf( "\n" );
      }
      return ( left > 0 );
   }

   int fannkuch( int toprint = -1 ) {
      bool printing = ( toprint >= 0 );
      int left_limit = printing ? digits : digits - 1;
      pos_left = printing ? 1 : digits - 2;
      for ( int i = 0; i < digits; i++ ) {
         p[i] = i;
         count[i] = i + 1;
      }
      if ( printing )
         print( toprint );
      p[pos_right] = digits - 1;
      p[digits - 1] = pos_right;

      int maxflips = ( digits > 1 ) ? 1 : 0;
      while ( pos_left < left_limit ) {
         permute();
         if ( --count[pos_left] > 0 ) {
            if ( printing && !print( toprint ) )
               return maxflips;

            for ( ; pos_left != 1; pos_left-- )
               count[pos_left - 1] = pos_left;
            maxflips = std::max( maxflips, countflips() );
         } else {
            pos_left++;
         }
      }
      return maxflips;
   }

   void launch() { pthread_create( &id, NULL, threadrun, this ); }
   int finish() { int t; pthread_join( id, (void **)&t ); return t; }
   static void *threadrun( void *args ) { return (void *)((worker *)args)->fannkuch(); }

protected:
   int p[16], count[16];
   int digits, pos_right, pos_left;
   pthread_t id;

public:
   worker *next;
};


int fannkuch( int n ) {
   // create the workers
   int count = n - 1;
   worker *head = NULL;
   if ( n > 0 ) {
      for ( int i = 0; i < count; i++ ) {
         head = new worker( n, i, head );
         head->launch();
      }

      // print the first 30
      worker(n,n-1).fannkuch( 30 );
   }

   // gather the results
   int maxflips = 0;
   while ( head ) {
      maxflips = std::max( head->finish(), maxflips );
      worker *tmp = head->next;
      delete head;
      head = tmp;
   }

   return maxflips;
}

int main( int argc, const char *argv[] ) {
   int n = ( argc > 1 ) ? atoi( argv[1] ) : 0;
   printf( "Pfannkuchen(%d) = %d\n", n, fannkuch( n ) );
   return 0;
}

