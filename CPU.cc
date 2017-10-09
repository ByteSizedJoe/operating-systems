#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

//Code added from Dr.Beatys main.cc
#define READ_END 0
#define WRITE_END 1
#define NUM_PIPES processes.size()*2

#define WRITE(a) { const char *foo = a; write (1, foo, strlen (foo)); }

/*
This program does the following.
1) Create handlers for two signals.
2) Create an idle process which will be executed when there is nothing
   else to do.
3) Create a send_signals process that sends a SIGALRM every so often

When run, it should produce the following output (approximately):

$ ./a.out
in CPU.cc at 247 main pid = 26428
state:    1
name:     IDLE
pid:      26430
ppid:     0
slices:   0
switches: 0
started:  0
in CPU.cc at 100 at beginning of send_signals getpid () = 26429
in CPU.cc at 216 idle getpid () = 26430
in CPU.cc at 222 going to sleep
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 148 stopped running->pid = 26430
in CPU.cc at 155 continuing tocont->pid = 26430
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 148 stopped running->pid = 26430
in CPU.cc at 155 continuing tocont->pid = 26430
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 115 at end of send_signals
Terminated
---------------------------------------------------------------------------
Add the following functionality.
1) Change the NUM_SECONDS to 20.                                                -- DONE

2) Take any number of arguments for executables, and place each on new_list.
    The executable will not require arguments themselves.                       -- DONE

3) When a SIGALRM arrives, scheduler() will be called. It calls
    choose_process which currently always returns the idle process. Do the
    following.

    a) Update the PCB for the process that was interrupted including the
        number of context switches and interrupts it had, and changing its
        state from RUNNING to READY.                                            -- DONE

    b) If there are any processes on the new_list, do the following.
        i) Take the one off the new_list and put it on the processes list.
        ii) Change its state to RUNNING, and fork() and execl() it.             -- DONE

    c) Modify choose_process to round robin the processes in the processes
        queue that are READY. If no process is READY in the queue, execute
        the idle process.                                                       -- DONE

4) When a SIGCHLD arrives notifying that a child has exited, process_done() is
    called. process_done() currently only prints out the PID and the status.
    a) Add the printing of the information in the PCB including the number
        of times it was interrupted, the number of times it was context
        switched (this may be fewer than the interrupts if a process
        becomes the only non-idle process in the ready queue), and the total
        system time the process took.
    b) Change the state to TERMINATED.
    c) Start the idle process to use the rest of the time slice.                -- DONE
*/

#define NUM_SECONDS 20

// make sure the asserts work
#undef NDEBUG
#include <assert.h>

#define EBUG
#ifdef EBUG
#   define dmess(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << endl;

#   define dprint(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << (#a) << " = " << a << endl;

#   define dprintt(a,b) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << " " << (#b) << " = " \
    << b << endl
#else
#   define dprint(a)
#endif /* EBUG */

using namespace std;

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };

/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab (int signum) { dprint (signum); }

// c++decl> declare ISV as array 32 of pointer to function (int) returning
// void
void (*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

struct PCB
{
    STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
    //Added code by Joe Medina, creating the pipes in the PCB of a process.
    int P2K[2];//Process to kernel pipe with one position for each file descriptor for each pipe end
    int K2P[2];//Kernel to process pipe with one position for each file descriptor for each pipe end
};

/*
** an overloaded output operator that prints a PCB
*/
ostream& operator << (ostream &os, struct PCB *pcb)
{
    os << "state:        " << pcb->state << endl;
    os << "name:         " << pcb->name << endl;
    os << "pid:          " << pcb->pid << endl;
    os << "ppid:         " << pcb->ppid << endl;
    os << "interrupts:   " << pcb->interrupts << endl;
    os << "switches:     " << pcb->switches << endl;
    os << "started:      " << pcb->started << endl;
    return (os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator << (ostream &os, list<PCB *> which)
{
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++)
    {
        os << (*PCB_iter);
    }
    return (os);
}

PCB *running;
PCB *idle;

// http://www.cplusplus.com/reference/list/list/
list<PCB *> new_list;
list<PCB *> processes;

int sys_time;
char systemTime[1024];
char ps[1024] = "Process List: ";

//Function from Dr. Beatys main.cc
int eye2eh (int i, char *buf, int bufsize, int base)
{
    if (bufsize < 1) return (-1);
    buf[bufsize-1] = '\0';
    if (bufsize == 1) return (0);
    if (base < 2 || base > 16)
    {
        for (int j = bufsize-2; j >= 0; j--)
        {
            buf[j] = ' ';
        }
        return (-1);
    }

    int count = 0;
    const char *digits = "0123456789ABCDEF";
    for (int j = bufsize-2; j >= 0; j--)
    {
        if (i == 0)
        {
            buf[j] = ' ';
        }
        else
        {
            buf[j] = digits[i%base];
            i = i/base;
            count++;
        }
    }
    if (i != 0) return (-1);
    return (count);
}

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals (int signal, int pid, int interval, int number)
{
    dprintt ("at beginning of send_signals", getpid ());

    for (int i = 1; i <= number; i++)
    {
        sleep (interval);

        dprintt ("sending", signal);
        dprintt ("to", pid);

        if (kill (pid, signal) == -1)
        {
            perror ("kill");
            return;
        }
    }
    dmess ("at end of send_signals");
}

struct sigaction *create_handler (int signum, void (*handler)(int))
{
    struct sigaction *action = new (struct sigaction);

    action->sa_handler = handler;
/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop (i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
    if (signum == SIGCHLD)
    {
        action->sa_flags = SA_NOCLDSTOP;
    }
    else
    {
        action->sa_flags = 0;
    }

    sigemptyset (&(action->sa_mask));

    assert (sigaction (signum, action, NULL) == 0);
    return (action);
}

PCB* choose_process ()
{
    /* This code added by JM. Collaboration was done with Vicky Lym. This code is part 3b of the assignment. The following
    *  will save the PCB of the currently running process before round robin execution.
    */
    running -> interrupts += 1;

    //Check if the processes list has processes that are ready.
    if(!new_list.empty())
    {
        //Change the state of the currently running process to READY
        running -> state = READY;

        //Fork to create child process
        pid_t pid = fork();

        //Case where the fork has failed
        if(pid < 0)
        {
            perror("Fork() error has occurred: ");
            exit(errno);
        }
        //Case where the fork was successful
        if(pid == 0)
        {
            close(new_list.front() -> P2K[READ_END]);
            close(new_list.front() -> K2P[WRITE_END]);

            dup2(new_list.front() -> P2K[WRITE_END], 3);
            dup2(new_list.front() -> K2P[READ_END], 4);

            //Execute the process as an executable.
            execl(new_list.front() -> name, new_list.front() -> name, (char*) NULL);
            perror("Execl() error has occurred.");
            exit(errno);
        }
        else
        {
            //Get the PID of the parent and child processes for the PCB.
            pid_t childPID = pid;
            pid_t parentPID = getpid();

            //Before adding the new process to the processes list, update PCB.
            //Change state to running, assign the start time
            new_list.front() -> state = RUNNING;
            new_list.front() -> pid = childPID;
            new_list.front() -> ppid = parentPID;
            new_list.front() -> started = sys_time;

            //Push the new process to the back of the processes list.
            processes.push_back(new_list.front());
            strncat(ps, new_list.front() -> name, sizeof(new_list.front() -> name));
            strncat(ps, " ", 1);
            //Remove the process from the new processes list.
            new_list.pop_front();
            //The currently running process is the new process which was added to the processes list.
            running = processes.back();
            //Return this process to the virtual CPU.
            return running;
        }
    }
    else
    {
        //Create a list PCB iterator that will go through the PCBs in the processes list.
        list<PCB*>::iterator iterateProcesses;
        for(iterateProcesses = processes.begin(); iterateProcesses != processes.end(); iterateProcesses++)
        {
            //Check if the current process iterations state is running
            if((*iterateProcesses) -> state == RUNNING)
            {
                //Add the running process to the back of the processes list/
                processes.push_back(running);
                //Change the state from running to ready.
                processes.back() -> state = READY;
                //Remove that iteration from the processes list(as it's now on the back of the process list)
                processes.erase(iterateProcesses);
                //Break from this loop to begin round robin.
                break;
            }
        }
        for(iterateProcesses = processes.begin(); iterateProcesses != processes.end(); iterateProcesses++)
        {
            //Check if the current process iteration is ready for CPU time
            if((*iterateProcesses) -> state == READY)
            {
                if((*iterateProcesses) -> pid != running -> pid)
                {
                    //If so, increment the switch as a context switch is about to occur.
                    running -> switches += 1;
                }
                //The new running process will be the next ready process.
                running = *iterateProcesses;
                running -> state = RUNNING;
                return running;
            }
        }
    }


    return idle;
}

void scheduler (int signum)
{
    assert (signum == SIGALRM);
    sys_time++;

    PCB* tocont = choose_process();

    dprintt ("continuing", tocont->pid);
    if (kill (tocont->pid, SIGCONT) == -1)
    {
        perror ("kill");
        return;
    }
}

void process_done (int signum)
{
    assert (signum == SIGCHLD);

    int status, cpid;

    cpid = waitpid (-1, &status, WNOHANG);

    dprintt ("in process_done", cpid);

    if  (cpid == -1)
    {
        perror ("waitpid");
    }
    else if (cpid == 0)
    {
        if (errno == EINTR) { return; }
        perror ("no children");
    }
    else
    {
        dprint (WEXITSTATUS (status));
    }

    //Print out the number of interrupts for the most recently completed process.
    cout << "# of interrupts: " << running -> interrupts << endl;
    //Print out the number of context switches from the most recently completed process.
    cout << "# of context switches: " << running -> switches << endl;
    //Print out the total time it took for the process to complete.
    cout << "Total process time: " << sys_time - running -> started << endl;
    //Set the state of the most recently finished process to terminated so it is not given
    //any more CPU time
    running -> state = TERMINATED;
    //Finish the rest of the time slice with IDLE.
    running = idle;
}

/*Code added by Joe Medina. The handler function designed to process a trap.
 * This code will read the request from the child and generate a proper response.
 * Code based on code from Dr. Beatys main.cc
 */
void process_trap(int signum)
{
    running -> state = WAITING; //change the state to waiting so process doesn't get scheduled.
    assert(signum == SIGTRAP); //Make sure the signum is indeed SIGTRAP.
    WRITE("Entering process_trap\n");

    //Create a buffer to hold the request information.
    char buffer[1024];
    const char* processRequest = "ps";
    const char* sys_timeRequest = "systime";
    //Figure out how many we are reading, and also read from the pipe into the buffer.
    int num_read = read(running -> P2K[READ_END], buffer, 1023);

    if(num_read > 0)
    {
        buffer[num_read] = '\0';
        WRITE("Kernel read: ");
        WRITE(buffer);
        WRITE("\n");

        int result = strcmp(buffer, processRequest);
        if(result == 0)
        {
            write(running -> K2P[WRITE_END], ps, sizeof(ps));
        }

        result = strcmp(buffer, sys_timeRequest);
        if(result == 0)
        {
            eye2eh(sys_time,systemTime, 1024, 10);
            write(running -> K2P[WRITE_END], systemTime, sizeof(systemTime));
        }
    }

        WRITE("Leaving process_trap\n");
        running -> state = READY;
}

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR (int signum)
{
    if (kill (running->pid, SIGSTOP) == -1)
    {
        perror ("kill");
        return;
    }
    dprintt ("stopped", running->pid);

    ISV[signum](signum);
}

/*
** set up the "hardware"
*/
void boot (int pid)
{
    ISV[SIGALRM] = scheduler;       create_handler (SIGALRM, ISR);
    ISV[SIGCHLD] = process_done;    create_handler (SIGCHLD, ISR);
    ISV[SIGTRAP] = process_trap;    create_handler (SIGTRAP, ISR);

    // start up clock interrupt
    int ret;
    if ((ret = fork ()) == 0)
    {
        // signal this process once a second for three times
        send_signals (SIGALRM, pid, 1, NUM_SECONDS);

        // once that's done, really kill everything...
        kill (0, SIGTERM);
    }

    if (ret < 0)
    {
        perror ("fork");
    }
}

void create_idle ()
{
    int idlepid;

    if ((idlepid = fork ()) == 0)
    {
        dprintt ("idle", getpid ());

        // the pause might be interrupted, so we need to
        // repeat it forever.
        for (;;)
        {
            dmess ("going to sleep");
            pause ();
            if (errno == EINTR)
            {
                dmess ("waking up");
                continue;
            }
            perror ("pause");
        }
    }
    idle = new (PCB);
    idle->state = RUNNING;
    idle->name = "IDLE";
    idle->pid = idlepid;
    idle->ppid = 0;
    idle->interrupts = 0;
    idle->switches = 0;
    idle->started = sys_time;
}

int main (int argc, char **argv)
{
    /* This functionality was added by Joe Medina, this piece of code should
     * take in a variable number of arguments into our virtual CPU which
     * are executables that will be processed by the vCPU. It's a for loop
     * which is bound by argc which represents the total number of arguments
     * (with argv[0] being the current program name, argv[i] and beyond
     * represents the executables provided by the command line.*/
    if(argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            //Code credit: klamb from Slack
            printf("Adding an argument to the new list\n");
            PCB* newProcess = new (PCB); //Creating a new PCB for the new process
            newProcess -> state = NEW; //Set the state of the new process to NEW
            newProcess -> name = argv[i]; // The name of the process is the string from the commandline
            newProcess -> ppid = 0; //Set ppid initially to 0 as it is determined in later code.
            newProcess -> interrupts = 0; //There are no interrupts for a new process.
            newProcess -> switches = 0; //There are also no context switches for a new process.
            assert(pipe(newProcess -> P2K) == 0);
            assert(pipe(newProcess -> K2P) == 0);

            //cout << newProcess; //Debugging, remove at later point
            new_list.push_back(newProcess); //Push the newly created PCB for the new process to the new_list
        }
    }
    /* End block of added code*/

    int pid = getpid();
    dprintt ("main", pid);

    sys_time = 0;
    boot (pid);

    // create a process to soak up cycles
    create_idle ();
    running = idle;

    cout << running;

    // we keep this process around so that the children don't die and
    // to keep the IRQs in place.
    for (;;)
    {
        pause();
        if (errno == EINTR) { continue; }
        perror ("pause");
    }
}
