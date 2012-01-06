#include <stdlib.h>
#include <stdio.h>

typedef off_t off64_t;
#include <apr_pools.h>

const size_t	LINE_SIZE = 64;

struct node
{
  int i;
  struct node *left;
  struct node *right;
};

int
node_check(const struct node *n)
{
  if (n->left)
    {
      int lc = node_check (n->left);
      int rc = node_check (n->right);
      return lc + n->i - rc;
    }

  return n->i;
}

struct node *
node_get_avail (apr_pool_t *pool)
{
  return apr_palloc (pool, sizeof(struct node));
}

struct node *
make (int i, int depth, apr_pool_t *pool)
{
  struct node *curr = node_get_avail (pool);

  curr->i = i;

  if (depth > 0)
    {
      curr->left  = make (2*i-1, depth - 1, pool);
      curr->right = make (2*i  , depth - 1, pool);
    }
  else
    {
      curr->left  = NULL;
      curr->right = NULL;
    }

  return curr;
}

int
main(int argc, char *argv[])
{
  apr_pool_t *long_lived_pool;
  int min_depth = 4;
  int req_depth = (argc == 2 ? atoi(argv[1]) : 10);
  int max_depth = (req_depth > min_depth + 2 ? req_depth : min_depth + 2);
  int stretch_depth = max_depth+1;

  apr_initialize();

  /* Alloc then dealloc stretchdepth tree */
  {
    apr_pool_t *store;
    struct node *curr;

    apr_pool_create (&store, NULL);
    curr = make (0, stretch_depth, store);
    printf ("stretch tree of depth %i\t check: %i\n", stretch_depth,
	    node_check (curr));
    apr_pool_destroy (store);
  }

  apr_pool_create (&long_lived_pool, NULL);

  {
    struct node *long_lived_tree = make(0, max_depth, long_lived_pool);

    /* buffer to store output of each thread */
    char *outputstr = (char*) malloc(LINE_SIZE * (max_depth +1) * sizeof(char));
    int d;

#pragma omp parallel for
    for (d = min_depth; d <= max_depth; d += 2)
      {
        int iterations = 1 << (max_depth - d + min_depth);
	apr_pool_t *store;
        int c = 0, i;

	apr_pool_create (&store, NULL);

        for (i = 1; i <= iterations; ++i)
	  {
	    struct node *a, *b;

	    a = make ( i, d, store);
	    b = make (-i, d, store);
            c += node_check (a) + node_check (b);
	    apr_pool_clear (store);
        }
	apr_pool_destroy (store);

	/* each thread write to separate location */
	sprintf(outputstr + LINE_SIZE * d, "%d\t trees of depth %d\t check: %d\n", (2 * iterations), d, c);
      }

    /* print all results */
    for (d = min_depth; d <= max_depth; d += 2)
      printf("%s", outputstr + (d * LINE_SIZE) );
    free(outputstr);

    printf ("long lived tree of depth %i\t check: %i\n", max_depth,
	    node_check (long_lived_tree));

    return 0;
  }
}

/*
MAKE:
/usr/bin/gcc -pipe -Wall -O3 -fomit-frame-pointer -march=native -fopenmp -D_FILE_OFFSET_BITS=64 -I/usr/include/apr-1.0 -lapr-1 -lgomp binarytrees.gcc-7.c -o binarytrees.gcc-7.gcc_run
rm binarytrees.gcc-7.c
*/

