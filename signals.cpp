/*  This file will handle the sending/receiving of process signals. It creates a child process using fork,
 *  and then the child process will send signals using kill to the parent process. The parent process then
 *  handles these signals through handler methods. This simulates hardware interrupts and how the kernel
 *  handles the unpredictability of those events.
 *  Joseph Medina
 */


/*The three signals that I will use in my program:
 *  SIGHUP(1): terminal line hang-up, SIGIO(23): file descriptor ready to send I/O
 *  SIGUSR1(30): User defined signal.
 */

#include <iostream>
#include <assert.h>
#include <signal.h>
#include <unistd.h>

void sighandler(int);

int main()
{
    struct sigaction *action = new (struct sigaction);
    sigemptyset(&(action -> sa_mask));
    action -> sa_handler = sighandler;
    action -> sa_flags = SA_RESTART;

    assert(sigaction(SIGUSR1, action, NULL) == 0);
    assert(sigaction(SIGUSR2, action, NULL) == 0);
    assert(sigaction(SIGIO, action, NULL) == 0);

    pid_t pid = fork(); //Create the new child process.

    if(pid < 0) //Check fork system call, if less than zero. Something wrong.
    {
        perror("Fork() error occurred: ");
        exit(errno);
    }
    if(pid == 0)
    {
        printf("Parent Process: %d\n", getppid()); //Print process ID of parent.
        printf("Child Process: %d\n", getpid()); //Print process ID of child.*/
        int parentProcess = getppid();


        if(kill(parentProcess, SIGUSR2) == 0)
        {
            printf("SIGUSR2 signal sent to the parent process\n");
        }
        else if(kill(parentProcess, SIGUSR2) > 0)
        {
            perror("Kill() error occurred.");
            exit(errno);
        }

        if(kill(parentProcess, SIGIO) == 0)
        {
            printf("SIGIO signal sent to parent process. \n");
        }
        else if(kill(parentProcess, SIGIO) > 0)
        {
            perror("Kill() error occurred.");
            exit(errno);
        }


        if(kill(parentProcess, SIGUSR1) == 0)
        {
            printf("1st SIGUSR1 signal sent to parent process. \n");
            //Kill w/ SIGUSR1 success, send two more.

            if(kill(parentProcess, SIGUSR1) == 0)
            {
                printf("2nd SIGUSR1 signal sent to parent process. \n");

                if(kill(parentProcess, SIGUSR1) == 0)
                {
                    printf("3rd SIGUSR1 signal sent to parent process. \n");
                }
                else if(kill(parentProcess, SIGUSR1) > 0)
                {
                    perror("Kill() w/SIGUSR1 3 error occurred.");
                    exit(errno);
                }
            }
            else if(kill(parentProcess, SIGUSR1) > 0)
            {
                perror("Kill() w/SIGUSR1 2 error occurred.");
                exit(errno);
            }

        }
        else if(kill(parentProcess, SIGUSR1) > 0)
        {
            perror("Kill() w/SIGUSR1 error occurred.");
            exit(errno);
        }
    }
    else
    {
        while (1)
        {
            int status;
            int returnPID = waitpid (pid, &status, 0);
            if (returnPID == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    perror("waitpid caused an error - ");
                    break;
                }
            }
            else
            {
                break;
            }
            assert (returnPID == pid);
        }
    }

    delete(action);

    return 0;
}

void sighandler(int signal)
{
    const char *string;

    switch(signal)
    {
        case(SIGUSR2):
            string = "Signal SIGUSR2 received from child.\n";
            write(1,string,strlen(string));
            break;
        case(SIGIO):
            string = "Signal SIGIO received from child.\n";
            write(1, string, strlen(string));
            break;
        case(SIGUSR1):
            string = "Signal SIGUSR1 received from child.\n";
            write(1, string, strlen(string));
            break;
    }
}
