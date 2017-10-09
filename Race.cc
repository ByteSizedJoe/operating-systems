#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
** Compile and run this program, and make sure you get the 'aargh' error
** message. Fix it using a pthread mutex. The one command-line argument is
** the number of times to loop. Here are some suggested initial values, but
** you might have to tune them to your machine.
** Debian 8: 100000000
** Gouda: 10000000
** OS X: 100000
** You will need to compile your program with a "-lpthread" option.
*/


#define NUM_THREADS 2

int i;
/* Code added by Joe Medina. Implemented a pthread_mutex.
 * Inspired by: https://github.com/angrave/SystemProgramming/wiki/Synchronization,-Part-1:-Mutex-Locks
 */

//This creates a pthread mutex and initializes it for lock critical sections of our code.
pthread_mutex_t pthreadMutex = PTHREAD_MUTEX_INITIALIZER;

void *foo (void *bar)
{
    pthread_t *me = new pthread_t (pthread_self());
    printf("in a foo thread, ID %ld\n", *me);

    //This will lock the mutex as we enter this critical section of the code(the for loop)
    pthread_mutex_lock(&pthreadMutex);
    for (i = 0; i < *((int *) bar); i++)
    {
        int tmp = i;

        if (tmp != i)
        {
            printf ("aargh: %d != %d\n", tmp, i);
        }
    }
    //Now that we are done in the critical section of the code, we are going to unlock it so that other
    //threads may now enter.
    pthread_mutex_unlock(&pthreadMutex);

    pthread_mutex_destroy(&pthreadMutex);
    pthread_exit (me);

}

int main(int argc, char **argv)
{
    int iterations = strtol(argv[1], NULL, 10);
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (pthread_create(&threads[i], NULL, foo, (void *) &iterations))
        {
            perror ("pthread_create");
            return (1);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++)
    {
        void *status;
        if (pthread_join (threads[i], &status))
        {
            perror ("pthread_join");
            return (1);
        }
        printf("joined a foo thread, number %ld\n", *((pthread_t *) status));
    }

    return (0);
}

//Test
