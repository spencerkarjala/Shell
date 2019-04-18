#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "historyArray.h"

#define MAX_HISTORY 10
#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define PWD_BUF_LENGTH 512

//creates a circular array to store user shell command entries
struct historyArray* createHistoryArray() {
    struct historyArray* ha = malloc(sizeof(struct historyArray));
    ha->firstItem = 1;
    ha->lastItem  = 0;
    ha->size = 0;
    ha->items = malloc(MAX_HISTORY * sizeof(char*));
    for (int i = 0; i < MAX_HISTORY; i++) {
        ha->items[i] = malloc(COMMAND_LENGTH * sizeof(char));
        ha->items[i][0] = '\0';
    }
    return ha;
}

//frees all malloc'd data from historyArray initialization
void removeHistoryArray(struct historyArray* ha) {
    for (int i = 0; i < MAX_HISTORY; i++) {
        free(ha->items[i]);
    }
    free(ha->items);
    free(ha);
    return;
}

//adds a new entry to the circular history array
void addToHistoryArray(struct historyArray* ha, char** commandsToAdd, _Bool in_background) {
    ha->lastItem++;
    int totalLen = 0;

    /*
     * if new item is being placed at n>MAX_HISTORY index,
     * re-initialize that memory to avoid data conflicts
     */
    if (ha->lastItem > 9) {
        memset(ha->items[ha->lastItem % MAX_HISTORY], 0, strlen(ha->items[ha->lastItem % MAX_HISTORY]));
    }

    /*
     * for each argument, concatenate it into a new history
     * array index and add a space between it, then null-terminate it.
     */
    for (int i = 0; commandsToAdd[i] != NULL; i++) {
		strcat(ha->items[ha->lastItem % MAX_HISTORY], commandsToAdd[i]);
        totalLen += strlen(commandsToAdd[i]);

        if (commandsToAdd[i + 1] != NULL) {
            strcat(ha->items[ha->lastItem % MAX_HISTORY], " ");
        }
        else {
            ha->items[ha->lastItem % MAX_HISTORY][totalLen+i] = '\0';
        }
    }

    //if new command should be run in the background, add on a '&'
    if (in_background) {
        strcat(ha->items[ha->lastItem % MAX_HISTORY], " ");
        strcat(ha->items[ha->lastItem % MAX_HISTORY], "&");
    }

    //once number of entries > MAX_HISTORY, move up first index too
    if (ha->size == MAX_HISTORY) {
        ha->firstItem++;
    }
    else {
        ha->size++;
    }
    return;
}