#include "lqueue.h"

// Define the queue handle
QueueHandle_t stringQueue;
String buffer_queue = "";
void initQueue() {
    stringQueue = xQueueCreate(10, 500);
}

void addStringToQueue(String str) {
    // Try to send to queue with 10ms timeout

    str.remove(0, 1);  // Remove first char from str
   buffer_queue += str;     // Add remaining string to buffer
   
   while (buffer_queue.indexOf('\n') != -1) {
       int newlineIndex = buffer_queue.indexOf('\n');
       String completeLine = buffer_queue.substring(0, newlineIndex);
       
       // Try to send to queue with 10ms timeout
       xQueueSend(stringQueue, completeLine.c_str(), pdMS_TO_TICKS(10));
       
       // Remove processed portion from buffer
       buffer_queue = buffer_queue.substring(newlineIndex + 1);
   }
}
String readStringFromQueue() {
    if (stringQueue == NULL) {
        return "";
    }

    char buffer[500];  // Match the size used in queue creation
    if (xQueueReceive(stringQueue, &buffer, pdMS_TO_TICKS(10)) == pdPASS) {
        return String(buffer);
    }
    return "";  // Return empty string if no data or error
}
int getQueueCount() {
    if (stringQueue == NULL) {
        return 0;
    }
    return uxQueueMessagesWaiting(stringQueue);
}
void emptyQueue() {
    if (stringQueue == NULL) {
        return;
    }
    
    // Read and discard all messages from queue
    char buffer[500];
    while (xQueueReceive(stringQueue, &buffer, 0) == pdPASS) {
        // Keep receiving until queue is empty
    }
    
    // Alternative method using xQueueReset
    // xQueueReset(stringQueue);  // This is faster but less graceful
}