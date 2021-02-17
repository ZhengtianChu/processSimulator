/**************************************************************************************************************************
			- DO NOT CHANGE THE CONTENTS OF THIS FILE FOR YOUR COURSEWORK (APART FROM THE VALUES FOR THE CONSTANTS)
			- ONLY WORK WITH THE OFFICIAL VERSION
**************************************************************************************************************************/

#include <sys/time.h>

// Number of processes to simulate
#define NUMBER_OF_PROCESSES 1024

// maximum number of processes that can be in the system at any point in time (number of PIDs available)
#define SIZE_OF_PROCESS_TABLE 128

// Duration of the time slice for the round robin algorithm
#define TIME_SLICE 5

// Maximum duration of the individual processes, in milli seconds. Note that the times themselves will be chosen at random in ]0,100]
#define MAX_BURST_TIME 50

// Maximum process priority 
#define MAX_PRIORITY 32

// Number of CPUs to simulate
#define NUMBER_OF_CPUS 2

// interval at which the booster runs
#define BOOST_INTERVAL SIZE_OF_PROCESS_TABLE * MAX_BURST_TIME * 100

// termination daemon interval
#define LONG_TERM_SCHEDULER_INTERVAL 50

// termination daemon interval
#define TERMINATION_INTERVAL 50

/* 
 * Definition of the structure containing the process characteristics. These should be sufficient for the full implementation of all tasks.
 */
struct process
{
	int iPreempt; 
	int * pPID;
	struct timeval oTimeCreated;
	struct timeval oFirstTimeRunning;
	struct timeval oLastTimeRunning;
	struct timeval oMostRecentTime; // most recent time the job finished running
	
	int iInitialBurstTime;
	int iPreviousBurstTime;
	int iRemainingBurstTime;
	int iPriority;
};

void preemptJob(struct process * pTemp);
struct process * generateProcess();
long int getDifferenceInMilliSeconds(struct timeval start, struct timeval end);
void runProcess(struct process * oTemp, int iBurstTime, struct timeval * oStartTime, struct timeval * oEndTime);
void runNonPreemptiveJob(struct process * oTemp, struct timeval * oStartTime, struct timeval * oEndTime);
void runPreemptiveJob(struct process * oTemp, struct timeval * oStartTime, struct timeval * oEndTime);
