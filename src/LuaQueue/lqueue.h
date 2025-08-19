#ifndef L_QUEUE_H
#define L_QUEUE_H

#include <queue>
#include <Arduino.h>
#include "Global/global.h"

// Declare the queue handle as extern
extern QueueHandle_t stringQueue;

// Declare the functions
void initQueue();
void addStringToQueue(String str);
String readStringFromQueue();
int getQueueCount();
void emptyQueue();

#endif