import java.util.concurrent.atomic.AtomicInteger;

public final class fannkuchredux implements Runnable
{
    private static final int TOPRINT = 30;
    private static final int CHUNKSZ = 5040*4;
    private static int n;
    private static int nfact;
    private static int[] maxFlips;
    private static AtomicInteger taskId;

    int[] p, pp, count;

    fannkuchredux()
    {
        p = new int[n];
        pp = new int[n];
        count = new int[n];
    }

    void print()
    {
        for ( int i = 0; i < p.length; i++ ) {
            System.out.print( p[i] + 1 );
        }
        System.out.println();
    }

    int countFlips()
    {
        int last = p[0];
        if ( last == 0 )
            return 0;
        if ( p[last] == 0 )
            return 1;

        int flips = 1;
        int len = pp.length;
        System.arraycopy( p, 0, pp, 0, len );
        do {
             ++flips;
             for ( int lo = 1, hi = last - 1; lo < hi; ++lo, --hi ) {
                int t = pp[lo];
                pp[lo] = pp[hi];
                pp[hi] = t;
             }
             int t = pp[last];
             pp[last] = last;
             last = t;
        } while ( pp[last] != 0 );
        return flips;
    }

    void firstPermutation( int idx )
    {
        for ( int i=0; i<p.length; ++i ) {
           p[i] = i;
        }

        int curFact = nfact;
        for ( int i=count.length; i>0; --i ) {
            curFact /= i;
            int d = idx / curFact;
            count[i-1] = d;
            idx = idx % curFact;

            System.arraycopy( p, 0, pp, 0, i );
            for ( int j=0; j<i; ++j ) {
                p[j] = j+d < i ? pp[j+d] : pp[j+d-i];
            }
        }
    }

    boolean nextPermutation()
    {
        int first = p[1];
        p[1] = p[0];
        p[0] = first;

        int i=1;
        while ( ++count[i] > i ) {
            count[i] = 0;
            if ( ++i == count.length ) {
                return false;
            }
            int next = p[0] = p[1];
            for ( int j=1; j<i; ++j ) {
                p[j] = p[j+1];
            }
            p[i] = first;
            first = next;
        }
        return true;
    }

    public void run()
    {
        
        for (;;) {
            int task = taskId.getAndIncrement();
            if ( task >= maxFlips.length ) {
                break;
            }

            int idxMin = task*CHUNKSZ;
            int idxMax = Math.min( nfact, idxMin+CHUNKSZ );

            firstPermutation( idxMin );

            int mflips = 0;
            while ( idxMin < TOPRINT ) {
                print();
                if ( !nextPermutation() ) {
                    maxFlips[task] = mflips;
                    return;
                }
                mflips = Math.max( mflips, countFlips() );
                idxMin += 1;
            }

            for ( int idx=idxMin; idx<idxMax; ++idx ) {
                if ( !nextPermutation() ) {
                    maxFlips[task] = mflips;
                    return;
                }
                mflips = Math.max( mflips, countFlips() );
            }

            maxFlips[task] = mflips;
        }
    }

    static void printResult( int n, int res )
    {
        System.out.println( "Pfannkuchen("+n+") = "+res );
    }

    public static void main( String[] args )
    {
        n = args.length > 0 ? Integer.parseInt( args[0] ) : 12;
        if ( n <= 1 ) {
            printResult( n, 0 );
            return;
        }

        nfact = 1;
        for ( int i=2; i<=n; ++i ) {
            nfact *= i;
        }

        int nchunks = (nfact + CHUNKSZ - 1) / CHUNKSZ;
        maxFlips = new int[nchunks];
        taskId = new AtomicInteger(0);

        int nthreads = Runtime.getRuntime().availableProcessors();
        Thread[] threads = new Thread[nthreads];
        for ( int i=0; i<nthreads; ++i ) {
            threads[i] = new Thread( new fannkuchredux() );
            threads[i].start();
        }
        for ( Thread t : threads ) {
            try {
                t.join();
            }
            catch ( InterruptedException e ) {}
        }

        int res = 0;
        for ( int v : maxFlips ) {
            res = Math.max( res, v );
        }
        printResult( n, res );
    }
}

