#ifndef QUEUE_H
#define QUEUE_H

#include "includes.h"

#define QUEUE_SIZE 5

uint32_t queue[QUEUE_SIZE];
extern int front;
extern int rear;

void initQueue();
void enqueue(uint32_t value);
void dequeue();

#endif // QUEUE_H