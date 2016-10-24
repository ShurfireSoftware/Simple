/***************************************************************************//**
 * @file que.c
 * @brief Provides a simple, circular queue of characters.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 09/25/2014
 * @date Last updated: 09/25/2014
 *
 *  This Queue only stores bytes.
 *
 *  The calling routine must declare a variable of type S_QUEUE
 *  and also a data storage area of size (quesize +1).  Any data
 *  type may be used as long as it is cast to an unsigned char
 *  when calling these routines.
 *
 *  The data storage area is defined by the caller and must
 *  contain one more cell than the actual size of the queue,
 *  as this algorithm depends on having one empty cell.
 *  The size of the queue used for initialization MUST be a
 *  power of two-1. This allows the routine to use a simple
 *  masking operation rather than a modulo.
 *               
 * IMPORTANT:   NOT INTERRUPT SAFE.  NOT TASK SAFE
 *  One queue should be used by only  one task (and an interrupt).
 *  If the queue is filled by an interrupt routine, then the main routine 
 *  should only remove items from the queue.  QNum, QEmpty, and QFull routines
 *  should be protected by a mutex or interrupt disable  - best not to use them
 *  if queue is filled by an interrupt routine.  
 *
 * Change Log:
 * 09/25/2014
 * - Created.
 *
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdbool.h>
#include "que.h"

/*******************************************************************************
* Procedure:    QInit
* Purpose:      Initialize the queue.
* Passed:       the user passes in a value for the number of
*               items to be stored, a pointer to the queue structure,
*               and a pointer to the data storage area.
*   
* Returned:     nothing
* Globals:      none
* Notes:        The data storage area is defined by the caller and must
*               contain one more cell than the actual size of the queue,
*               as this algorithm depends on having one empty cell.
*               The size of the queue used for initialization MUST be a
*               power of two-1. This allows the routine to use a simple
*               masking operation rather than a modulo.
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
void QInit(unsigned long qsize, S_QUEUE * p_q, unsigned char* p_src)  //initializes the queue
{
	p_q->head=0;
	p_q->tail= 0;
	p_q->mask=qsize;        //use the size as a mask
	p_q->p_data = p_src;
	p_q->num_items=0;
}

/*******************************************************************************
* Procedure:    QInsert
* Purpose:      Insert at byte at the tail of the queue.
* Passed:       byte to insert, pointer to queue structure
*   
* Returned:     true if added successfully, false if queue is full
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
bool QInsert(unsigned char item, S_QUEUE * p_q)
{
	bool item_added;


	if(  ((p_q->tail + 1) & p_q->mask) == p_q->head) //queue is full
	{
		item_added=false;
	}
	else
	{		
		p_q->p_data[p_q->tail]=item;
		p_q->tail = (p_q->tail+1) & p_q->mask;
		p_q->num_items++;
		item_added=true;
	}
	return(item_added);
}

/*******************************************************************************
* Procedure:    QRemove
* Purpose:      remove a byte from head of the queue and copy it to the area 
*               pointed to by passed in parameter
* Passed:       pointer to receive byte, pointer to queue structure
*   
* Returned:     true if retrieved, false if queue is empty
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
bool QRemove(unsigned char * p_object,S_QUEUE * p_q)
{
	bool item_retrieved;

	if (p_q->head == p_q->tail)	//queue is empty
    {
		item_retrieved=false;
    }
	else
	{
		item_retrieved = true;
		*p_object = p_q->p_data[p_q->head];
		p_q->head=(p_q->head +1) & p_q->mask;
		p_q->num_items--;
	}
	return item_retrieved;
}

/*******************************************************************************
* Procedure:    QFull
* Purpose:      Check to see if queue is full.
* Passed:       pointer to the queue structure
*   
* Returned:     true if full, false otherwise
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
bool QFull(S_QUEUE * p_q)
{
	unsigned char queue_is_full;

	if(  ((p_q->tail+1) & p_q->mask) == p_q->head) //queue is full
    {
		queue_is_full =true;
    }
	else
    {
		queue_is_full = false;
    }
	return queue_is_full;
}

/*******************************************************************************
* Procedure:    QEmpty
* Purpose:      Check to see if queue is empty.
* Passed:       pointer to the queue structure
*   
* Returned:     true if queue is empty, false otherwise
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
bool QEmpty(S_QUEUE * p_q)
{
	bool queue_is_empty;
	
	if (p_q->head == p_q->tail)	//queue is empty
    {
		queue_is_empty=true;
    }
	else
    {
		queue_is_empty = false;
    }
	return queue_is_empty;
}

/*******************************************************************************
* Procedure:    QNum
* Purpose:      Determine number of items in queue.
* Passed:       pointer to the queue structure
*   
* Returned:     number of bytes in the queue
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
unsigned long QNum(S_QUEUE * p_q)
{
	unsigned long num;

	num = p_q->num_items;
	return (num);
}

/*******************************************************************************
* Procedure:    QFlush
* Purpose:      Reset queue to the empty condition.
* Passed:       pointer to the queue structure
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-25  Neal Shurmantine    initial revision
*******************************************************************************/
void QFlush(S_QUEUE * p_q)	//reset the queue to empty
{
	p_q->head=0;
	p_q->tail= 0;
	p_q->num_items=0;
}

