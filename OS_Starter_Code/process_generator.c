#include "headers.h"
#include <string.h>
#include <stdbool.h>
#include <sys/msg.h>

#define MAX_PROCESSES 100
#define MSGKEY 12345

// Structure to store process information
struct process {
    int id;
    int arrivalTime;
    int runtime;
    int priority;
};

// Structure for message queue
struct msgbuffer {
    long mtype;
    struct process p;
};

// Global variables for process storage
struct process processes[MAX_PROCESSES];
int processCount = 0;

// Function to clear IPC resources
void clearResources(int signum) {
    printf("\nClearing all resources before exit.\n");
    destroyClk(true);
    exit(0);
}

int main(int argc, char * argv[]) {
    // Handle SIGINT (Ctrl + C) for cleanup purposes
    signal(SIGINT, clearResources);

    // Step 1: Read input file
    FILE *inputFile = fopen("processes.txt", "r");
    if (inputFile == NULL) {
        perror("Error opening input file");
        return -1;
    }

    char line[256];
    // Ignore comment lines and parse the processes
    while (fgets(line, sizeof(line), inputFile)) {
        if (line[0] != '#') {
            // Parsing non-comment lines to extract process information
            struct process p;
            sscanf(line, "%d\t%d\t%d\t%d", &p.id, &p.arrivalTime, &p.runtime, &p.priority);
            processes[processCount++] = p;
        }
    }
    fclose(inputFile);

    // Step 2: Ask user for the scheduling algorithm
    int algorithmChoice;
    printf("Choose the scheduling algorithm:\n");
    printf("1. Shortest Job First (SJF)\n");
    printf("2. Preemptive Highest Priority First (PHPF)\n");
    printf("3. Round Robin (RR)\n");
    printf("Enter the choice (1-3): ");
    scanf("%d", &algorithmChoice);

    // For Round Robin, get time quantum
    int timeQuantum = 0;
    if (algorithmChoice == 3) {
        printf("Enter time quantum for Round Robin: ");
        scanf("%d", &timeQuantum);
    }

    // Step 3: Initialize and create the clock and scheduler processes
    pid_t clkPid = fork();
    if (clkPid == 0) {
        // Child process for clock
        execl("./clk.out", "clk.out", NULL);
        perror("Failed to start clock process");
        return -1;
    }

    // Start the scheduler process
    pid_t schedulerPid = fork();
    if (schedulerPid == 0) {
        // Child process for scheduler
        char algoStr[3], quantumStr[3];
        sprintf(algoStr, "%d", algorithmChoice);
        sprintf(quantumStr, "%d", timeQuantum);
        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, NULL);
        perror("Failed to start scheduler process");
        return -1;
    }

    // Step 4: Initialize clock to start tracking time
    initClk();

    // Step 5: Create message queue for IPC with scheduler
    int msgq_id = msgget(MSGKEY, IPC_CREAT | 0644);
    if (msgq_id == -1) {
        perror("Error in creating message queue");
        return -1;
    }

    // Step 6: Generation Main Loop - Send processes to the scheduler at the right time
    int currentProcess = 0;
    while (currentProcess < processCount) {
        int currentTime = getClk();

        // Check if any processes have arrived at the current time
        while (currentProcess < processCount && processes[currentProcess].arrivalTime <= currentTime) {
            // Send the process to the scheduler
            struct msgbuffer msg;
            msg.mtype = 1;  // Arbitrary message type, all processes have the same type for simplicity
            msg.p = processes[currentProcess];

            if (msgsnd(msgq_id, &msg, sizeof(msg.p), !IPC_NOWAIT) == -1) {
                perror("Error sending message to scheduler");
            } else {
                printf("Sent process %d to scheduler at time %d\n", processes[currentProcess].id, currentTime);
            }
            currentProcess++;
        }
        // Sleep to avoid busy waiting
        sleep(1);
    }

    // Step 7: Clean up and release clock resources
    destroyClk(true);

    return 0;
}
#include "headers.h"
#include <string.h>
#include <stdbool.h>
#include <sys/msg.h>

#define MAX_PROCESSES 100
#define MSGKEY 12345

// Structure to store process information
struct process {
    int id;
    int arrivalTime;
    int runtime;
    int priority;
};

// Structure for message queue
struct msgbuffer {
    long mtype;
    struct process p;
};

// Global variables for process storage
struct process processes[MAX_PROCESSES];
int processCount = 0;

// Function to clear IPC resources
void clearResources(int signum) {
    printf("\nClearing all resources before exit.\n");
    destroyClk(true);
    exit(0);
}

int main(int argc, char * argv[]) {
    // Handle SIGINT (Ctrl + C) for cleanup purposes
    signal(SIGINT, clearResources);

    // Step 1: Read input file
    FILE *inputFile = fopen("processes.txt", "r");
    if (inputFile == NULL) {
        perror("Error opening input file");
        return -1;
    }

    char line[256];
    // Ignore comment lines and parse the processes
    while (fgets(line, sizeof(line), inputFile)) {
        if (line[0] != '#') {
            // Parsing non-comment lines to extract process information
            struct process p;
            sscanf(line, "%d\t%d\t%d\t%d", &p.id, &p.arrivalTime, &p.runtime, &p.priority);
            processes[processCount++] = p;
        }
    }
    fclose(inputFile);

    // Step 2: Ask user for the scheduling algorithm
    int algorithmChoice;
    printf("Choose the scheduling algorithm:\n");
    printf("1. Shortest Job First (SJF)\n");
    printf("2. Preemptive Highest Priority First (PHPF)\n");
    printf("3. Round Robin (RR)\n");
    printf("Enter the choice (1-3): ");
    scanf("%d", &algorithmChoice);

    // For Round Robin, get time quantum
    int timeQuantum = 0;
    if (algorithmChoice == 3) {
        printf("Enter time quantum for Round Robin: ");
        scanf("%d", &timeQuantum);
    }

    // Step 3: Initialize and create the clock and scheduler processes
    pid_t clkPid = fork();
    if (clkPid == 0) {
        // Child process for clock
        execl("./clk.out", "clk.out", NULL);
        perror("Failed to start clock process");
        return -1;
    }

    // Start the scheduler process
    pid_t schedulerPid = fork();
    if (schedulerPid == 0) {
        // Child process for scheduler
        char algoStr[3], quantumStr[3];
        sprintf(algoStr, "%d", algorithmChoice);
        sprintf(quantumStr, "%d", timeQuantum);
        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, NULL);
        perror("Failed to start scheduler process");
        return -1;
    }

    // Step 4: Initialize clock to start tracking time
    initClk();

    // Step 5: Create message queue for IPC with scheduler
    int msgq_id = msgget(MSGKEY, IPC_CREAT | 0644);
    if (msgq_id == -1) {
        perror("Error in creating message queue");
        return -1;
    }

    // Step 6: Generation Main Loop - Send processes to the scheduler at the right time
    int currentProcess = 0;
    while (currentProcess < processCount) {
        int currentTime = getClk();

        // Check if any processes have arrived at the current time
        while (currentProcess < processCount && processes[currentProcess].arrivalTime <= currentTime) {
            // Send the process to the scheduler
            struct msgbuffer msg;
            msg.mtype = 1;  // Arbitrary message type, all processes have the same type for simplicity
            msg.p = processes[currentProcess];

            if (msgsnd(msgq_id, &msg, sizeof(msg.p), !IPC_NOWAIT) == -1) {
                perror("Error sending message to scheduler");
            } else {
                printf("Sent process %d to scheduler at time %d\n", processes[currentProcess].id, currentTime);
            }
            currentProcess++;
        }
        // Sleep to avoid busy waiting
        sleep(1);
    }

    // Step 7: Clean up and release clock resources
    destroyClk(true);

    return 0;
}
