#include "headers.h"
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#define MSGKEY 12345

// Process Control Block (PCB) structure
struct PCB {
    int id;
    int arrivalTime;
    int runtime;
    int remainingTime;
    int priority;
    int waitingTime;
    int startTime;
    int endTime;
    pid_t pid;  // Process ID of the forked process
    bool started;  // To track if process has started
};

// Queue for Ready Processes
#define MAX_PROCESSES 100
struct PCB readyQueue[MAX_PROCESSES];
int readyQueueSize = 0;

// Function Prototypes
void scheduleSJF();
void schedulePHPF();
void scheduleRR(int timeQuantum);
void handleProcessCompletion(int signum);
void clearResources();
void logSchedulerPerformance();

// Global Variables
int currentAlgorithm;
int currentProcessIndex = -1;  // Index of the currently running process
pid_t runningProcessPid = -1;
int totalProcesses = 0;
int cpuBusyTime = 0;  // Tracks CPU busy time
int simulationStartTime = 0;
int simulationEndTime = 0;

// Open file pointers for logging
FILE *logFile;
FILE *perfFile;

int main(int argc, char *argv[]) {
    // Handle SIGINT (Ctrl+C) to cleanup resources properly
    signal(SIGINT, clearResources);

    // Handle SIGCHLD to track process completion
    signal(SIGCHLD, handleProcessCompletion);

    // Step 1: Get Scheduling Algorithm from Command-Line Arguments
    if (argc < 2) {
        printf("Missing scheduling algorithm argument\n");
        return -1;
    }
    currentAlgorithm = atoi(argv[1]);

    int timeQuantum = 0;
    if (currentAlgorithm == 3) {
        if (argc < 3) {
            printf("Missing time quantum for Round Robin\n");
            return -1;
        }
        timeQuantum = atoi(argv[2]);
    }

    // Step 2: Initialize clock and setup message queue
    initClk();
    int msgq_id = msgget(MSGKEY, IPC_CREAT | 0644);
    if (msgq_id == -1) {
        perror("Error in creating message queue");
        return -1;
    }

    // Open log files for writing
    logFile = fopen("scheduler.log", "w");
    if (logFile == NULL) {
        perror("Error opening scheduler.log");
        return -1;
    }
    perfFile = fopen("scheduler.perf", "w");
    if (perfFile == NULL) {
        perror("Error opening scheduler.perf");
        return -1;
    }

    // Record the start of the simulation
    simulationStartTime = getClk();

    // Step 3: Scheduler Loop - Receiving and scheduling processes
    while (true) {
        struct msgbuffer msg;
        if (msgrcv(msgq_id, &msg, sizeof(msg.p), 0, IPC_NOWAIT) != -1) {
            // Process received from the generator
            struct PCB newProcess;
            newProcess.id = msg.p.id;
            newProcess.arrivalTime = msg.p.arrivalTime;
            newProcess.runtime = msg.p.runtime;
            newProcess.remainingTime = msg.p.runtime;
            newProcess.priority = msg.p.priority;
            newProcess.waitingTime = 0;
            newProcess.startTime = -1;  // Not started yet
            newProcess.endTime = -1;    // Not finished yet
            newProcess.pid = -1;        // Will be assigned after fork
            newProcess.started = false;

            // Add the process to the ready queue
            readyQueue[readyQueueSize++] = newProcess;
            totalProcesses++;
            fprintf(logFile, "# At time %d process %d added to ready queue\n", getClk(), newProcess.id);
            fflush(logFile);
        }

        // Step 4: Apply Scheduling Algorithm
        switch (currentAlgorithm) {
            case 1:
                scheduleSJF();
                break;
            case 2:
                schedulePHPF();
                break;
            case 3:
                scheduleRR(timeQuantum);
                break;
            default:
                printf("Invalid scheduling algorithm\n");
                return -1;
        }

        // Avoid busy waiting
        sleep(1);
    }

    // Clean up resources and finalize metrics
    destroyClk(true);
    return 0;
}

// Scheduling Algorithm: Shortest Job First (SJF)
void scheduleSJF() {
    if (runningProcessPid != -1) {
        // A process is already running, SJF does not preempt.
        return;
    }

    // Find the process with the shortest remaining time
    int shortestIndex = -1;
    for (int i = 0; i < readyQueueSize; i++) {
        if (readyQueue[i].remainingTime > 0) {
            if (shortestIndex == -1 || readyQueue[i].remainingTime < readyQueue[shortestIndex].remainingTime) {
                shortestIndex = i;
            }
        }
    }

    if (shortestIndex != -1) {
        // Fork and start the process
        struct PCB *process = &readyQueue[shortestIndex];
        pid_t pid = fork();
        if (pid == 0) {
            char remainingTimeStr[10];
            sprintf(remainingTimeStr, "%d", process->remainingTime);
            execl("./process.out", "process.out", remainingTimeStr, NULL);
            perror("Error executing process");
            exit(-1);
        } else {
            process->pid = pid;
            process->started = true;
            process->startTime = getClk();
            runningProcessPid = pid;
            currentProcessIndex = shortestIndex;
            cpuBusyTime += process->runtime;  // Track CPU busy time
            fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                    getClk(), process->id, process->arrivalTime, process->runtime, process->remainingTime, process->waitingTime);
            fflush(logFile);
        }
    }
}

// Scheduling Algorithm: Preemptive Highest Priority First (PHPF)
void schedulePHPF() {
    // Find the highest priority process in the ready queue
    int highestPriorityIndex = -1;
    for (int i = 0; i < readyQueueSize; i++) {
        if (readyQueue[i].remainingTime > 0) {
            if (highestPriorityIndex == -1 || readyQueue[i].priority < readyQueue[highestPriorityIndex].priority) {
                highestPriorityIndex = i;
            }
        }
    }

    if (highestPriorityIndex == -1) {
        // No process to schedule
        return;
    }

    struct PCB *highestPriorityProcess = &readyQueue[highestPriorityIndex];

    // Check if we need to preempt the current running process
    if (runningProcessPid == -1 || highestPriorityProcess->priority < readyQueue[currentProcessIndex].priority) {
        if (runningProcessPid != -1 && highestPriorityProcess->priority < readyQueue[currentProcessIndex].priority) {
            // Preempt the current running process
            kill(runningProcessPid, SIGSTOP);
            fprintf(logFile, "At time %d process %d stopped\n", getClk(), readyQueue[currentProcessIndex].id);
            fflush(logFile);
        }

        // Start or resume the highest priority process
        if (highestPriorityProcess->pid == -1) {
            // The process hasn't started yet, fork and start it
            pid_t pid = fork();
            if (pid == 0) {
                char remainingTimeStr[10];
                sprintf(remainingTimeStr, "%d", highestPriorityProcess->remainingTime);
                execl("./process.out", "process.out", remainingTimeStr, NULL);
                perror("Error executing process");
                exit(-1);
            } else {
                highestPriorityProcess->pid = pid;
                highestPriorityProcess->startTime = getClk();
                fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        getClk(), highestPriorityProcess->id, highestPriorityProcess->arrivalTime,
                        highestPriorityProcess->runtime, highestPriorityProcess->remainingTime,
                        highestPriorityProcess->waitingTime);
            }
        } else {
            // The process was previously stopped, resume it
            kill(highestPriorityProcess->pid, SIGCONT);
            fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                    getClk(), highestPriorityProcess->id, highestPriorityProcess->arrivalTime,
                    highestPriorityProcess->runtime, highestPriorityProcess->remainingTime,
                    highestPriorityProcess->waitingTime);
        }

        // Update the running process
        runningProcessPid = highestPriorityProcess->pid;
        currentProcessIndex = highestPriorityIndex;
        fflush(logFile);
    }
}

// Scheduling Algorithm: Round Robin (RR)
void scheduleRR(int timeQuantum) {
    static int lastExecutionTime = -1;

    if (runningProcessPid != -1) {
        // Check if the current process has exhausted its time slice
        int currentTime = getClk();
        if ((currentTime - lastExecutionTime) >= timeQuantum) {
            // Time slice expired, preempt current process
            kill(runningProcessPid, SIGSTOP);
            fprintf(logFile, "At time %d process %d stopped\n", currentTime, readyQueue[currentProcessIndex].id);
            fflush(logFile);

            // Move the current process to the end of the ready queue
            struct PCB temp = readyQueue[currentProcessIndex];
            for (int i = currentProcessIndex; i < readyQueueSize - 1; i++) {
                readyQueue[i] = readyQueue[i + 1];
            }
            readyQueue[readyQueueSize - 1] = temp;

            runningProcessPid = -1;
            currentProcessIndex = -1;
        }
    }

    // If no process is running, start the next process in the queue
    if (runningProcessPid == -1 && readyQueueSize > 0) {
        struct PCB *nextProcess = &readyQueue[0];
        if (nextProcess->pid == -1) {
            // Fork and start a new process
            pid_t pid = fork();
            if (pid == 0) {
                char remainingTimeStr[10];
                sprintf(remainingTimeStr, "%d", nextProcess->remainingTime);
                execl("./process.out", "process.out", remainingTimeStr, NULL);
                perror("Error executing process");
                exit(-1);
            } else {
                nextProcess->pid = pid;
                nextProcess->startTime = (nextProcess->startTime == -1) ? getClk() : nextProcess->startTime;
                fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        getClk(), nextProcess->id, nextProcess->arrivalTime,
                        nextProcess->runtime, nextProcess->remainingTime,
                        nextProcess->waitingTime);
            }
        } else {
            // Resume a stopped process
            kill(nextProcess->pid, SIGCONT);
            fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                    getClk(), nextProcess->id, nextProcess->arrivalTime,
                    nextProcess->runtime, nextProcess->remainingTime,
                    nextProcess->waitingTime);
        }

        runningProcessPid = nextProcess->pid;
        currentProcessIndex = 0;  // The first process in the queue is now running
        lastExecutionTime = getClk();  // Track when this process was last started/resumed
        fflush(logFile);
    }
}

// Clean up resources when terminating
void clearResources() {
    printf("\nClearing scheduler resources before exit.\n");
    fclose(logFile);
    logSchedulerPerformance();
    fclose(perfFile);
    destroyClk(true);
    exit(0);
}

// Handle process completion and remove from ready queue
void handleProcessCompletion(int signum) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0) {
        if (runningProcessPid == pid) {
            struct PCB *process = &readyQueue[currentProcessIndex];
            process->endTime = getClk();
            runningProcessPid = -1;
            currentProcessIndex = -1;

            // Calculate metrics for the finished process
            int TA = process->endTime - process->arrivalTime;
            double WTA = (double)TA / process->runtime;
            process->waitingTime = TA - process->runtime;

            // Log process completion
            fprintf(logFile, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                    getClk(), process->id, process->arrivalTime, process->runtime, 0, process->waitingTime, TA, WTA);
            fflush(logFile);
        }
    }
}

// Log final performance metrics
void logSchedulerPerformance() {
    simulationEndTime = getClk();
    int totalSimulationTime = simulationEndTime - simulationStartTime;
    double cpuUtilization = ((double)cpuBusyTime / totalSimulationTime) * 100;

    // Calculate average waiting time and average weighted turnaround time
    double avgWaitingTime = 0.0;
    double avgWTA = 0.0;
    for (int i = 0; i < readyQueueSize; i++) {
        avgWaitingTime += readyQueue[i].waitingTime;
        int TA = readyQueue[i].endTime - readyQueue[i].arrivalTime;
        avgWTA += (double)TA / readyQueue[i].runtime;
    }
    avgWaitingTime /= totalProcesses;
    avgWTA /= totalProcesses;

    // Log the performance
    fprintf(perfFile, "CPU utilization = %.2f%%\n", cpuUtilization);
    fprintf(perfFile, "Avg WTA = %.2f\n", avgWTA);
    fprintf(perfFile, "Avg Waiting = %.2f\n", avgWaitingTime);
    // Note: For simplicity, the standard deviation is omitted here but can be added similarly.
}
