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

#ifndef __LOCAL_OTA_TASK_H__
#define __LOCAL_OTA_TASK_H__

/* TI-DRIVERS Header files */
#include <ti/drivers/net/wifi/simplelink.h>

/* POSIX Header files */
#include <semaphore.h>
#include <pthread.h>
#include <time.h>

/*!
 *  \brief  Application's states
 */
typedef enum
{
    LocalOtaState_Starting,
    LocalOtaState_WaitForConnection,
    LocalOtaState_WaitForIP,
    LocalOtaState_PingingGW,
    LocalOtaState_OtaRun,
    LocalOtaState_Error,
    LocalOtaState_Max
}LocalOtaState;

/*!
 *  \brief  Application's events
 */
typedef enum
{
    LocalOtaEvent_Started,
    LocalOtaEvent_Connected,
    LocalOtaEvent_IpAcquired,
    LocalOtaEvent_Disconnected,         /* used also for IP lost */
    LocalOtaEvent_PingCompleted,
    LocalOtaEvent_OtaDownloadDone,
    LocalOtaEvent_OtaError,
    LocalOtaEvent_Timeout,
    LocalOtaEvent_Error,
    LocalOtaEvent_Restart,
    LocalOtaEvent_Max
}LocalOtaEvent;

typedef struct LocalOtaTask_ControlBlock_t
{
    sem_t localOtaConnDoneSignal;
    sem_t localOtaConnDoneToOtaServerSignal;
}LocalOtaTask_CB;

/****************************************************************************
                      GLOBAL VARIABLES
****************************************************************************/
extern LocalOtaTask_CB LocalOtaTask_ControlBlock;

//****************************************************************************
//                      FUNCTION PROTOTYPES
//****************************************************************************

//****************************************************************************
//
//! \brief signal event to local ota SM
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
int16_t SignalEvent(LocalOtaEvent event);

//****************************************************************************
//
//! \brief This function starts the led toggling timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
int32_t StartLedEvtTimer(uint32_t timeout);

//****************************************************************************
//
//! \brief This function stops the led toggling timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
int32_t StopLedEvtTimer();

//****************************************************************************
//
//! \brief This is the main local ota task
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
void * localOtaTask(void *pvParameters);

#endif
