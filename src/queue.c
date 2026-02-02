#include "queue.h"

int front = -1;
int rear = -1;

void initQueue()
{
	front = -1;
	rear = -1;
}

void enqueue(uint32_t value)
{
	if (rear == QUEUE_SIZE - 1)
	{
		printf("Queue Overflow\n");
		return;
	}

	if (front == -1)
		front = 0;

	rear++;
	connectedClientsQueue[rear] = value;
}

void dequeue()
{
	if (front == -1 || front > rear)
	{
		printf("Queue Underflow\n");
		return;
	}
	front++;
}
