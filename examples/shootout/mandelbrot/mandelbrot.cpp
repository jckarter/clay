#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sched.h>
#include <memory.h>

// #include <omp.h>


#define L2_CACHE_LINE   64
#define BYTE_A_TIME      L2_CACHE_LINE
#define COLUMN_FETCH    (BYTE_A_TIME * 8)
#define ALIGN         __attribute__ ((aligned(L2_CACHE_LINE)))


typedef unsigned char byte;
typedef double   v2d   __attribute__ ((vector_size(16))); // vector of two doubles

const v2d v10   = { 1.0, 1.0 };
const v2d v15   = { 1.5, 1.5 };
const v2d v40   = { 4.0, 4.0 };

v2d inv_2n;   // {2.0/N, 2.0/N}


// int
// GetThreadCount()
// {
//    cpu_set_t cs;
//    CPU_ZERO(&cs);
//    sched_getaffinity(0, sizeof(cs), &cs);

//    int count = 0;
//    for (int i = 0; i < CPU_SETSIZE; ++i)
//    {
//       if (CPU_ISSET(i, &cs))
//          ++count;
//    }
//    return count;
// }


void
mandelbrot(int N, byte* data)
{
   // counter of each line, how many columns are processed
   ALIGN int jobs[N];
   memset(jobs, 0, sizeof(jobs));

   // #pragma omp parallel default(shared) firstprivate(data) num_threads(GetThreadCount())
   {
      // foreach line
      for (int y = 0; y < N; ++y, data += (N >> 3))
      {
         // Calculate C.img = y*2/N -1.0
         v2d Civ = {y, y};
         Civ = Civ * inv_2n - v10;

         // Divide task for each thread here:
         // claim that me (this thread) will handle K-not-yet-process columns
         int x;
         while ((x = __sync_fetch_and_add(jobs + y, COLUMN_FETCH)) < N)
         {
            int limit = std::min(x +COLUMN_FETCH, N);
            int byte_acc = 1;

            for (; x < limit; x += 2)
            {
               v2d   Crv = {x+1, x};
               Crv = Crv * inv_2n - v15;
               v2d Zrv = Crv;
               v2d Ziv = Civ;
               v2d Trv = Crv * Crv;
               v2d Tiv = Civ * Civ;

               int result = 3; // assume that 2 elements belong to MB set

               int i = 1;
               while ( result && (i++ < 50) )
               {
                  v2d ZZ = Zrv * Ziv;
                  Zrv = Trv - Tiv + Crv;
                  Ziv = ZZ + ZZ + Civ;
                  Trv = Zrv * Zrv;
                  Tiv = Ziv * Ziv;

                  // delta = (Trv + Tiv) <= 4.0 ? 0xff : 0x00

                  // for g++
                  // v2d delta = (v2d)__builtin_ia32_cmplepd( (Trv + Tiv), v40 );

                  // for clang++
                  v2d delta = (v2d)__builtin_ia32_cmppd( (Trv + Tiv), v40, 2 );

                  // mask-out elements that goes outside MB set
                  result &= __builtin_ia32_movmskpd(delta);
               }

               byte_acc <<= 2;
               byte_acc |= result;

               if (__builtin_expect(byte_acc & ~0xFF, false))
               {
                  data[x >> 3] = static_cast<byte>(byte_acc);
                  byte_acc = 1;
               }
            } // end foreach (column)
         }
      } // end foreach (line)
   } // end parallel region
}


int
main (int argc, char **argv)
{
   int N = (argc == 2) ? atoi(argv[1]) : 200;
   assert((N % 8) == 0);

   printf("P4\n%d %d\n", N, N);
   int width_bytes = N >> 3;

   {
      double t[2];
      t[0] = t[1] = 2.0 / N;
      inv_2n = __builtin_ia32_loadupd(t);
   }

   byte* data = 0;
   assert(   posix_memalign(reinterpret_cast<void**>(&data), L2_CACHE_LINE, width_bytes * N)
         == 0);

   mandelbrot(N, data);

   fwrite( data, width_bytes, N, stdout);
   fflush(stdout);
   free(data);

   return 0;
}

