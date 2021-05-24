/*
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//*****************************************************************************
// !!!! This file is only for nortos !!!!
//*****************************************************************************

//*****************************************************************************
// Includes
//*****************************************************************************
#include "mqueue.h"

//*****************************************************************************
// Defines
//*****************************************************************************
/* event (one byte) queue with fixed size, for example state machine */
/* keep mqueue api calls */
#define EVENT_BUFFER_SIZE       20

//*****************************************************************************
// Typedefs
//*****************************************************************************
typedef struct
{   /* FIFO that holds the events in the order of reception */
    uint8_t eventsBuffer[EVENT_BUFFER_SIZE]; 
    /* FIFO Pointer from where to read  */
    uint8_t buffRead;  
    /* FIFO Pointer of write Location   */
    uint8_t buffWrite;        
/* FIFO counter to indicate how many events pending */  
    uint8_t buffSize;                                                           
}EventQueue;

//*****************************************************************************
// Globals
//*****************************************************************************
EventQueue gEventQueue;

//*****************************************************************************
// mqueue routines
//*****************************************************************************

//*****************************************************************************
//
//! \brief
//
//*****************************************************************************
mqd_t mq_open(const char *name,
              int oflags,
              int a,
              mq_attr *pAttr)
{
    EventQueue *pMqueue = (void *)&gEventQueue;

    pMqueue->buffRead = 0;
    pMqueue->buffSize = 0;
    pMqueue->buffWrite = 0;

    return((mqd_t)pMqueue);
}

//*****************************************************************************
//
//! \brief
//
//*****************************************************************************
int mq_receive(mqd_t mqdes,
               char *msg_ptr,
               int msg_len,
               unsigned int *msg_prio)
{
    EventQueue *pMqueue = (EventQueue *)mqdes;
    uint8_t eventIdx;

    if(pMqueue->buffSize <= 0)
    {
        msg_ptr = NULL;
        return(-1);
    }

    /* Get first event to read location in FIFO */
    eventIdx = pMqueue->buffRead;

    /* Increment read pointer */
    pMqueue->buffRead++;

    /* Check read pointer wrap around */
    if(pMqueue->buffRead >= EVENT_BUFFER_SIZE)
    {
        pMqueue->buffRead = 0;
    }

    /* Clear event from FIFO Buffer */
    pMqueue->buffSize--;

    *msg_ptr = pMqueue->eventsBuffer[eventIdx];
    return(0);
}

//*****************************************************************************
//
//! \brief
//
//*****************************************************************************
int mq_send(mqd_t mqdes,
            const char *msg_ptr,
            size_t msg_len,
            unsigned int msg_prio)
{
    EventQueue *pMqueue = (EventQueue *)mqdes;

    /* Check if FIFO is full, if so, drop event */
    if(pMqueue->buffSize >= EVENT_BUFFER_SIZE)
    {
        return(-1);
    }

    /* Add event to FIFO */
    pMqueue->eventsBuffer[pMqueue->buffWrite] = (uint8_t)*msg_ptr;

    /* Increment write pointer */
    pMqueue->buffWrite++;

    /* Check write pointer wrap around */
    if(pMqueue->buffWrite >= EVENT_BUFFER_SIZE)
    {
        pMqueue->buffWrite = 0;
    }

    /* Update FIFO pending information */
    pMqueue->buffSize++;

    //UART_PRINT("Adding event %d, buffWrite: %d, buffSize: %d\n\n",
    //    *msg_ptr, pMqueue->buffWrite, pMqueue->buffSize);
    return(0);
}

//*****************************************************************************
//
//! \brief
//
//*****************************************************************************
int mq_timedsend(mqd_t mqdes,
                 const char *msg_ptr,
                 size_t msg_len,
                 unsigned int msg_prio,
                 const struct mq_timespec *abstime)
{
    /* "timed" not implemented */
    return(mq_send(mqdes, msg_ptr, msg_len, msg_prio));
}

//*****************************************************************************
//
//! \brief
//
//*****************************************************************************
void clock_gettime(int a,
                   struct mq_timespec *ptm)
{
}
