#include "headers.h"

int remainingTime;

int main(int argc, char *argv[]) {
    // Initialize the clock connection
    initClk();

    // Check that we have the remaining time as argument
    if (argc < 2) {
        printf("Missing remaining time argument\n");
        destroyClk(false);
        return -1;
    }

    // Get the remaining time from the command line argument
    remainingTime = atoi(argv[1]);

    // Loop to simulate execution of the process
    int previousTime = getClk();  // The last time we checked the clock
    while (remainingTime > 0) {
        int currentTime = getClk();
        if (currentTime > previousTime) {
            remainingTime -= (currentTime - previousTime);
            previousTime = currentTime;
        }
    }

    // When finished, clean up the clock connection
    printf("Process with remaining time %d finished at time %d\n", remainingTime, getClk());
    destroyClk(false);

    return 0;
}
