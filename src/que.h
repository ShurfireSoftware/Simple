/***************************************************************************//**
 * @file que.h
 * @brief Include file for que.c
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 09/25/2014
 * @date Last updated: 09/25/2014
 *
 * Change Log:
 * 09/25/2014
 * - Created.
 *
 ******************************************************************************/
#ifndef __QUE_H
#define __QUE_H

typedef struct S_QUEUE
{
	unsigned long head;	//array index of next item to be removed from the queue
	unsigned long tail;	//array index where new item can be added to the queue
	unsigned char * p_data;  //pointer to the queue data area (array)
	unsigned long mask;	//size of the queue, used as a mask for wraparounds
	unsigned long num_items;//the current number of items in the queue.
}S_QUEUE;



//public function prototypes:
void QInit(unsigned long qsize,S_QUEUE * p_q, unsigned char * p_data);  //"constructs" the new queue
bool QInsert(unsigned char item, S_QUEUE * p_q); //returns FALSE if queue is full
bool QRemove(unsigned char * object, S_QUEUE * p_q); //returns FALSE if queue is empty
bool QFull(S_QUEUE * p_q);			//returns TRUE if queue is full
bool QEmpty(S_QUEUE * p_q); //returns true if queue is empty
unsigned long QNum(S_QUEUE * p_q); //returns number of items in queue
void QFlush(S_QUEUE * p_q);	//reset the queue to empty

#endif
