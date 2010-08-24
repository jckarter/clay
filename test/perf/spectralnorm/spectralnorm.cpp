#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sched.h>
//#include <omp.h>


// define SIMD data type. 2 doubles are packed in 1 XMM register
typedef double v2dt __attribute__((vector_size(16)));
v2dt const v1 = {1.0, 1.0};


struct Param
{
   union
   {
      double* u;      // source
      v2dt*   xmm_u;
   };

   union
   {
      double* tmp;   // temporary
      v2dt*   xmm_tmp;
   };

   union
   {
      double* v;         // destination
      v2dt*   xmm_v;
   };

   int    length;         // source/desti vec's length
   int    half_length;

   int    r_begin;      // working range of each thread
   int    r_end;

   double   vBv;
   double   vv;
};


// Return:   1.0 / (i + j) * (i + j +1) / 2 + i + 1;
double
eval_A(int i, int j)
{
   int d = (((i+j) * (i+j+1)) >> 1) + i+1;

   return 1.0 / d;
}


// Return: 2 doubles in xmm register [double1, double2]
//      double1 = 1.0 / (i + j) * (i + j +1) / 2 + i + 1;
//      double2 = 1.0 / (i+1 + j) * (i+1 + j +1) / 2 + i+1 + 1;
//   Or:
//      double2 = 1.0 / (i + j+1) * (i + j+1 +1) / 2 + i + 1;
template<bool inc_i>
v2dt
eval_A_xmm(int i, int j)
{
   if (inc_i)
      i <<= 1;
   else
      j <<= 1;

   int d1 = (((i+j) * (i+j+1)) >> 1) + i+1;
   int d2 = (((i+1 +j) * (i+1 +j+1)) >> 1) + i +1;

   if (inc_i)
      d2 += 1;

   v2dt r = {d1, d2};
   return v1 / r;
}


double
hz_add(v2dt x)
{
   double const* val = reinterpret_cast<double const*>(&x);
   return val[0] + val[1];
}


void
eval_A_times_u (Param const &p)
{
   for (int i = p.r_begin, ie = p.r_end; i < ie; ++i)
   {
      v2dt sum = {0, 0};

      // xmm = 2 doubles => index [0..length/2)
      int j = 0;
      for (; j < p.half_length; ++j)
         sum += eval_A_xmm<false>(i, j) * p.xmm_u[j];

      p.tmp[i] = hz_add(sum);

      // If source vector is odd size. This should be called <= 1 time
      for (j = j*2; j < p.length; ++j)
         p.tmp[i] += eval_A(i, j) * p.u[j];
   }
}

void
eval_At_times_u(Param const &p)
{
   for (int i = p.r_begin, ie = p.r_end; i < ie; ++i)
   {
      v2dt sum = {0, 0};

      int j = 0;
      for (; j < p.half_length; ++j)
         sum += eval_A_xmm<true>(j, i) * p.xmm_tmp[j];

      p.v[i] = hz_add(sum);

      for (j = j*2; j < p.length; ++j)
         p.v[i] += eval_A(j, i) * p.tmp[j];
   }
}

// Each thread modifies its portion in destination vector
// -> barrier needed to sync access
void
eval_AtA_times_u(Param const &p)
{
   eval_A_times_u( p );
//   #pragma omp barrier

   eval_At_times_u( p );
//   #pragma omp barrier
}

void
final_sum(Param& p)
{
   v2dt sum_vBv   = {0,0};
   v2dt sum_vv      = {0,0};

   int i = p.r_begin /2;
   int ie = p.r_end /2;

   for (; i < ie; ++i)
   {
      sum_vv   += p.xmm_v[i] * p.xmm_v[i];
      sum_vBv   += p.xmm_u[i] * p.xmm_v[i];
   }

   p.vBv   = hz_add(sum_vBv);
   p.vv   = hz_add(sum_vv);

   for (i = i*2; i < p.r_end; ++i)
   {
      p.vBv   += p.u[i] * p.v[i];
      p.vv   += p.v[i] * p.v[i];
   }
}


void
fill_10(Param const& p)
{
   int i = p.r_begin /2;
   int ie = p.r_end /2;

   for (; i < ie; ++i)
      p.xmm_u[i] = v1;

   for (i = i*2; i < p.r_end; ++i)
      p.u[i] = 1.0;
}


// Search for appropriate number of threads to spawn
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

double
spectral_game(int N)
{
   // Align L2 cache line
   __attribute__((aligned(64))) double u[N];
   __attribute__((aligned(64))) double tmp[N];
   __attribute__((aligned(64))) double v[N];

   double vBv   = 0.0;
   double vv   = 0.0;

//   #pragma omp parallel default(shared) num_threads(GetThreadCount())
   {
      // this block will be executed by NUM_THREADS
      // variable declared in this block is private for each thread
       int threadid   = 0; //omp_get_thread_num();
       int threadcount   = 1; //omp_get_num_threads();
      int chunk      = N / threadcount;

      Param my_param;

      my_param.u             = u;
      my_param.tmp         = tmp;
      my_param.v             = v;

      my_param.length         = N;
      my_param.half_length   = N /2;

      // calculate each thread's working range [r1 .. r2) => static schedule
      my_param.r_begin   = threadid * chunk;
      my_param.r_end      = (threadid < (threadcount -1)) ? (my_param.r_begin + chunk) : N;


      fill_10(my_param);
      // #pragma omp barrier

      // Evaluating
      for (int ite = 0; ite < 10; ++ite)
      {
         my_param.u = u;   // source is u
         my_param.v = v;   // desti is v
         eval_AtA_times_u(my_param);

         my_param.u = v; // source is v
         my_param.v = u; // desti is u
         eval_AtA_times_u(my_param);
      }

      my_param.u = u;
      my_param.v = v;
      final_sum(my_param);

      // #pragma omp critical
      {
         vBv   += my_param.vBv;
         vv   += my_param.vv;
      }
   } // parallel region

   return sqrt( vBv/vv );
}


int
main(int argc, char *argv[])
{
   int N = ((argc >= 2) ? atoi(argv[1]) : 2000);

   printf("%.9f\n", spectral_game(N));
   return 0;
}

