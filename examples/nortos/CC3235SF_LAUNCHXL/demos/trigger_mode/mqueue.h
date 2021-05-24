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

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
// Includes
//*****************************************************************************
#include <stddef.h>
#include <stdint.h>

//*****************************************************************************
// Defines
//*****************************************************************************
/* For mq_open() */
#define O_CREAT         0x200

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME  1
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 2
#endif

//*****************************************************************************
// Typedefs
//*****************************************************************************

/* Message queue descriptor */
typedef void *mqd_t;

/* Message queue attributes. */
typedef struct mq_attr
{
    long mq_flags;       /* Message queue description flags: 0 or O_NONBLOCK.
                            Initialized from oflag argument of mq_open(). */
    long mq_maxmsg;      /* Maximum number of messages on queue.  */
    long mq_msgsize;     /* Maximum message size. */
    long mq_curmsgs;     /* Number of messages currently queued. */
} mq_attr;

typedef struct mq_timespec
{
    unsigned int tv_sec;                 /* Seconds      */
    unsigned int tv_nsec;                /* Microseconds */
} mq_timespec_t;

//*****************************************************************************
// Function prototypes
//*****************************************************************************
extern mqd_t mq_open(const char *name,
                     int oflags,
                     int a,
                     mq_attr *pAttr);
extern int mq_receive(mqd_t mqdes,
                      char *msg_ptr,
                      int msg_len,
                      unsigned int *msg_prio);
extern int mq_send(mqd_t mqdes,
                   const char *msg_ptr,
                   size_t msg_len,
                   unsigned int msg_prio);
extern int mq_timedsend(mqd_t mqdes,
                        const char *msg_ptr,
                        size_t msg_len,
                        unsigned int msg_prio,
                        const struct mq_timespec *abstime);
extern void clock_gettime(int a,
                          struct mq_timespec *ptm);

#ifdef __cplusplus
}
#endif
