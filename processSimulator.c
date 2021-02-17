#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "linkedlist.h"
#include "coursework.h"

struct process * processTable[SIZE_OF_PROCESS_TABLE];
struct element * newQueueHead = NULL;
struct element * newQueueTail = NULL;
struct element * emptyListHead = NULL;
struct element * emptyListTail = NULL;
struct element * termQueueHead = NULL;
struct element * termQueueTail = NULL;
struct element * readyQueueHead[MAX_PRIORITY];
struct element * readyQueueTail[MAX_PRIORITY];

struct timeval  oBaseTime;
int CPU_ID[NUMBER_OF_CPUS];
int pid[SIZE_OF_PROCESS_TABLE];
pthread_t tids[NUMBER_OF_CPUS+ 4];
pthread_mutex_t pcb;
pthread_mutex_t nQueue;
sem_t rQueue;
pthread_mutex_t tQueue;
sem_t delay_gen;
sem_t shortsch;
int processCount = 0;
long int sumofResponseTime = 0.0;
long int sumofTurnaroundTime = 0.0;
long averageResponseTime = 0.0;
long averageTurnaroundTime = 0.0;



void printHeadersSVG()
{
	printf("SVG: <!DOCTYPE html>\n");
	printf("SVG: <html>\n");
	printf("SVG: <body>\n");
	printf("SVG: <svg width=\"10000\" height=\"1100\">\n");
}


void printProcessSVG(int iCPUId, struct process * pProcess, struct timeval oStartTime, struct timeval oEndTime)
{
	int iXOffset = getDifferenceInMilliSeconds(oBaseTime, oStartTime) + 30;
	int iYOffsetPriority = (pProcess->iPriority + 1) * 16 - 12;
	int iYOffsetCPU = (iCPUId - 1 ) * (480 + 50);
	int iWidth = getDifferenceInMilliSeconds(oStartTime, oEndTime);
	printf("SVG: <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"8\" style=\"fill:rgb(%d,0,%d);stroke-width:1;stroke:rgb(255,255,255)\"/>\n", iXOffset /* x */, iYOffsetCPU + iYOffsetPriority /* y */, iWidth, *(pProcess->pPID) - 1 /* rgb */, *(pProcess->pPID) - 1 /* rgb */);
}


void printPrioritiesSVG()
{
	for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS;iCPU++)
	{
		for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
		{
			int iYOffsetPriority = (iPriority + 1) * 16 - 4;
			int iYOffsetCPU = (iCPU - 1) * (480 + 50);
			printf("SVG: <text x=\"0\" y=\"%d\" fill=\"black\">%d</text>", iYOffsetCPU + iYOffsetPriority, iPriority);
		}
	}
}


void printRasterSVG()
{
	for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS;iCPU++)
	{
		for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
		{
			int iYOffsetPriority = (iPriority + 1) * 16 - 8;
			int iYOffsetCPU = (iCPU - 1) * (480 + 50);
			printf("SVG: <line x1=\"%d\" y1=\"%d\" x2=\"10000\" y2=\"%d\" style=\"stroke:rgb(125,125,125);stroke-width:1\" />", 16, iYOffsetCPU + iYOffsetPriority, iYOffsetCPU + iYOffsetPriority);
		}
	}
}


void printFootersSVG()
{
	printf("SVG: Sorry, your browser does not support inline SVG.\n");
	printf("SVG: </svg>\n");
	printf("SVG: </body>\n");
	printf("SVG: </html>\n");
}


/**
 * This function is used to remove pid from empty list and process
 * based on this pid, then put the process into new queue.
**/
void processGenerator()
{
	sem_wait(&delay_gen);
	int i = 0;
	while (i < NUMBER_OF_PROCESSES)
	{	
		// remove the first free pid
		pthread_mutex_lock(&pcb);	
		int * pPID = removeFirst(&emptyListHead, &emptyListTail);
		struct element * temp = emptyListHead;
		processTable[*pPID] = generateProcess(pPID);
		i = i + 1;		
		pthread_mutex_unlock(&pcb);

		// add a process to newQueue
		pthread_mutex_lock(&nQueue);
		addLast(processTable[*pPID], &newQueueHead, &newQueueTail);	
		struct process * p1 = newQueueTail->pData;
		printf("TXT: Generated: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *(int *)p1->pPID, p1->iPriority, p1->iPreviousBurstTime, p1->iRemainingBurstTime);
		pthread_mutex_unlock(&nQueue);

		if (temp == NULL)
		{
			// sleep
			sem_wait(&delay_gen);
		}
		
	}
}


/** This funciton implements long term scheduler, the first process in 
 * new queue will be moved the tail of reach queue with different priority
 * in a time interval.
**/
void longTermScheduler()
{
	while(1)
	{
		if (newQueueHead)
		{
			pthread_mutex_lock(&nQueue);
			struct process * temp = removeFirst(&newQueueHead, &newQueueTail);
			pthread_mutex_unlock(&nQueue);

			sem_wait(&rQueue);
			int priority = temp->iPriority;
			addLast(temp, &readyQueueHead[priority], &readyQueueTail[priority]);
			printf("TXT: Admitted: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *(int *)temp->pPID, priority, temp->iPreviousBurstTime, temp->iRemainingBurstTime);
			sem_post(&rQueue);
			sem_post(&shortsch);
		}
		usleep(LONG_TERM_SCHEDULER_INTERVAL);
	}
}


/** This function implements the short term scheduler, the processes in ready
 * queue with priority 0 to 15 will run in a preemptive FCFS scheduling and
 * processes with  priority 16 to 31 will run in RR scheduling.
**/
void shortTermScheduler(int* cpuID)
{
	while(1)
	{	
		struct timeval oStartTime;
		struct timeval oEndTime;
		sem_wait(&shortsch);
		int i = 0;
		sem_wait(&rQueue);
		for (i = 0; i < MAX_PRIORITY; i++)
		{
			if (readyQueueHead[i])
			{
				break;
			}
		}
		struct process * temp = removeFirst(&readyQueueHead[i], &readyQueueTail[i]);	
		sem_post(&rQueue);
		int isFirst = 0;
		if(temp->iInitialBurstTime == temp->iRemainingBurstTime)
		{
			isFirst = 1;
		}
		
		if (i < MAX_PRIORITY / 2)
   		{   
			for (int j = 0; j < i; j ++)
			{
				if(readyQueueHead[j])
				{
					preemptJob(temp);
					break;
				}
				
			}
			runNonPreemptiveJob(temp, &oStartTime, &oEndTime);	
		} else
		{
			runPreemptiveJob(temp, &oStartTime, &oEndTime);				
		}
		
		long responseTime = getDifferenceInMilliSeconds(temp->oTimeCreated, temp->oFirstTimeRunning);

		if(isFirst == 1)
		{
			sumofResponseTime += responseTime;
		}

		if (temp->iRemainingBurstTime == 0)
		{
			long turnaroundTime = getDifferenceInMilliSeconds(temp->oTimeCreated, temp->oLastTimeRunning);
			sumofTurnaroundTime += turnaroundTime;
			pthread_mutex_lock(&tQueue);
			addLast(temp, &termQueueHead, &termQueueTail);
			pthread_mutex_unlock(&tQueue);
			if (temp->iPriority < MAX_PRIORITY / 2 && isFirst == 1)
			{
				printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld, Turnaround Time = %ld\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime, responseTime, turnaroundTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);
			} else if (temp->iPriority < MAX_PRIORITY / 2 && isFirst == 0)
			{
				printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Turnaround Time = %ld\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime, turnaroundTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);
			} else if (temp->iPriority >= MAX_PRIORITY / 2 && isFirst == 1)
			{
				printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld, Turnaround Time = %ld\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime, responseTime, turnaroundTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);
			} else if (temp->iPriority >= MAX_PRIORITY / 2 && isFirst == 0)
			{
				printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Turnaround Time = %ld\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime, turnaroundTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);
			}	
			printf("TXT: Terminated: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime);
		} else
		{
			sem_wait(&rQueue);
			addLast(temp, &readyQueueHead[temp->iPriority], &readyQueueTail[temp->iPriority]);		
			//wake up
			sem_post(&shortsch);
			sem_post(&rQueue);
			if (temp->iPriority < MAX_PRIORITY / 2 && isFirst == 1)
			{
				printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime, responseTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);
			} else if (temp->iPriority < MAX_PRIORITY / 2 && isFirst == 0)
			{
				printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);		
			} else if (temp->iPriority >= MAX_PRIORITY / 2 && isFirst == 1)
			{
				printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime, responseTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);
			} else if (temp->iPriority >= MAX_PRIORITY / 2 && isFirst == 0)
			{
				printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *cpuID, *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime);
				printProcessSVG(*cpuID, temp, oStartTime, oEndTime);	
			}
			printf("TXT: Admitted: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime);
		}		
	}		
}


/**
 * This function is used to boost the process with low priority
 * to high in order to avoid starvation. The first process in priority
 * from 17 to 31 will be move to the tail of priority in each time interval.
**/
void boosterDaemon()
{
	while(1)
	{
		sem_wait(&rQueue);
		for (int i = MAX_PRIORITY / 2 + 1; i < MAX_PRIORITY; i ++)
		{
			if (readyQueueHead[i])
			{
				struct process * temp = removeFirst(&readyQueueHead[i], &readyQueueTail[i]);
				addLast(temp, &readyQueueHead[MAX_PRIORITY / 2], &readyQueueTail[MAX_PRIORITY / 2]);
				printf("TXT: Boost: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *(int *)temp->pPID, temp->iPriority, temp->iPreviousBurstTime, temp->iRemainingBurstTime);
			}
		}
		sem_post(&rQueue);
		usleep(BOOST_INTERVAL);
	}
	
}


/**
 * This function is used to be the termination of the process
 * and calculate average response time and turnaround time. 
 * The completed process will be removed and free its pid in 
 * empty list.
**/
void terminationDaemon()
{
	while(1)
	{
		if (termQueueHead)
		{
			usleep(TERMINATION_INTERVAL);

			// remove from termQueue
			pthread_mutex_lock(&tQueue);
			struct process * temp = removeFirst(&termQueueHead, &termQueueTail);
			int PID = *(int *)temp->pPID;
			processTable[pid[PID]] = NULL;
			pthread_mutex_unlock(&tQueue);

			processCount += 1;
	
			// add to emptylist
			pthread_mutex_lock(&pcb);
			addLast((void*)&pid[PID], &emptyListHead, &emptyListTail);
			if (emptyListHead->pNext == NULL)
			{
				//wake up generator
				sem_post(&delay_gen);
			}
			pthread_mutex_unlock(&pcb);	
		}
		if (processCount == NUMBER_OF_PROCESSES)
		{
			printf("TXT: Average response time = %lf, Average turnaround time = %lf\n", (double)sumofResponseTime / NUMBER_OF_PROCESSES, (double)sumofTurnaroundTime / NUMBER_OF_PROCESSES);
			printFootersSVG();
			exit(1);
		}
	}
}


int main(int argc, char * argv[])
{
	printHeadersSVG();
	printPrioritiesSVG();
	printRasterSVG();
	printf("\n");
	pthread_mutex_init(&pcb, NULL);
	pthread_mutex_init(&nQueue, NULL);
	sem_init(&rQueue, 0, 1);
	pthread_mutex_init(&tQueue, NULL);
	sem_init(&delay_gen, 0, 1);
	sem_init(&shortsch, 0, 0);

	gettimeofday(&oBaseTime, NULL);
	int ret;

	for (int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){pid[i] = i;}
	for (int i = 0; i < SIZE_OF_PROCESS_TABLE; i++)
	{
		if (processTable[pid[i]] == NULL)
		{
			addLast((void*)&pid[i], &emptyListHead, &emptyListTail);
		}
	}	

	for  (int i = 0; i < MAX_PRIORITY; i++)
	{
		readyQueueHead[i] = NULL;
		readyQueueTail[i] = NULL;
	}


	for  (int i = 0; i < NUMBER_OF_CPUS; i++)
	{
		CPU_ID[i] = i + 1;
		ret = pthread_create(&tids[i], NULL, (void*)&shortTermScheduler, &CPU_ID[i]);
		if (ret != 0) 
		{ 
			printf("pthread_create error: error_code = %d\n", ret);
		}
	}
	
	ret = pthread_create(&tids[NUMBER_OF_CPUS], NULL, (void*)&processGenerator, NULL);
	if (ret != 0) { printf("pthread_create error: error_code = %d\n", ret);}

	ret = pthread_create(&tids[NUMBER_OF_CPUS + 1], NULL, (void*)&longTermScheduler, NULL);
	if (ret != 0) { printf("pthread_create error: error_code = %d\n", ret);}

	ret = pthread_create(&tids[NUMBER_OF_CPUS + 2], NULL, (void*)&terminationDaemon, NULL);
	if (ret != 0) { printf("pthread_create error: error_code = %d\n", ret);}

	ret = pthread_create(&tids[NUMBER_OF_CPUS + 3], NULL, (void*)&boosterDaemon, NULL);
	if (ret != 0) { printf("pthread_create error: error_code = %d\n", ret);}

	for (int i = 0; i < NUMBER_OF_CPUS + 4; i ++){
		pthread_join(tids[i], NULL);
	}
}

// gcc -std=gnu99 processSimulator.c linkedlist.c coursework.c -pthread
