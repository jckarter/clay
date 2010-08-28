#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef double v2df __attribute__ ((vector_size(16))); /* vector of two doubles */
typedef int v4si __attribute__ ((vector_size(16))); /* vector of four ints */

// 4 works best on a quiet machine at nice -20
// 8 on a noisy machine at default priority
//#define NWORKERS 8
#define NWORKERS 1

int w, h;
v2df zero = { 0.0, 0.0 };
v2df four = { 4.0, 4.0 };
v2df nzero;
double inverse_w;
double inverse_h;

char *whole_data;
int y_pick;

static void * worker(void *_args) {
    char *data;
    double x, y;
    int bit_num;
    char byte_acc = 0;

    for (;;) {
        y = __sync_fetch_and_add(&y_pick, 1);
        if (y >= h)
            return NULL;
        data = &whole_data[(w >> 3) * (int)y];

        for(bit_num=0,x=0;x<w;x+=2)
        {
            v2df Crv = { (x+1.0)*inverse_w-1.5, (x)*inverse_w-1.5 };
            v2df Civ = { y*inverse_h-1.0, y*inverse_h-1.0 };
            v2df Zrv = { 0.0, 0.0 };
            v2df Ziv = { 0.0, 0.0 };
            v2df Trv = { 0.0, 0.0 };
            v2df Tiv = { 0.0, 0.0 };

            int i = 0;
            int mask;
            do {
                Ziv = (Zrv*Ziv) + (Zrv*Ziv) + Civ;
                Zrv = Trv - Tiv + Crv;
                Trv = Zrv * Zrv;
                Tiv = Ziv * Ziv;

                /* from mandelbrot C++ GNU g++ #5 program  */

                // for gcc
                //v2df delta = (v2df)__builtin_ia32_cmplepd( (Trv + Tiv), four );

                // for clang
                v2df delta = (v2df)__builtin_ia32_cmppd( (Trv + Tiv), four, 2 );

                mask = __builtin_ia32_movmskpd(delta);

            } while (++i < 50 && (mask));

            byte_acc <<= 2;
            byte_acc |= mask;
            bit_num+=2;

            if(!(bit_num&7)) {
                data[(bit_num>>3) - 1] = byte_acc;
                byte_acc = 0;
            }
        }

        if(bit_num&7) {
            byte_acc <<= (8-w%8);
            bit_num += 8;
            data[bit_num>>3] = byte_acc;
            byte_acc = 0;
        }
    }
}


int main (int argc, char **argv)
{
    pthread_t ids[NWORKERS];
    int i;

    nzero = -zero;

    w = h = atoi(argv[1]);

    inverse_w = 2.0 / w;
    inverse_h = 2.0 / h;

    y_pick = 0;
    whole_data = malloc(w * (w >> 3));

    for (i = 0; i < NWORKERS; i++)
        pthread_create(&ids[i], NULL, worker, NULL);
    for (i = 0; i < NWORKERS; i++)
        pthread_join(ids[i], NULL);

    printf("P4\n%d %d\n",w,h);
    fwrite(whole_data, h, w >> 3, stdout);

    free(whole_data);
    
    return 0;
}

