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
//
//! \addtogroup local_ota
//! @{
//
//*****************************************************************************

/* standard includes */
#include <stdlib.h>
#include <string.h>
#ifdef CC32XX
/* driverlib Header files */
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/inc/hw_memmap.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>
#endif
/* TI-DRIVERS Header files */
#include <ti_drivers_config.h>
#include <uart_term.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/wlan.h>
#include <ti/drivers/power/PowerCC32XX.h>

/* Example/Board Header files */
#include "local_ota_task.h"
#include "platform.h"
#include "local_ota.h"
#include "link_local_task.h"
#include "ota_archive.h"

/* POSIX Header files */
#include <pthread.h>
#include <time.h>

/* In msecs */
#define PING_INTERVAL           (2000) 
/* In msecs */   
#define PING_TIMEOUT            (2000)
/* In bytes */    
#define PING_PKT_SIZE           (20)      
#define NO_OF_ATTEMPTS          (4)
/* In msecs. Used to detect good/bad sl_start() */
#define ASYNC_EVT_TIMEOUT       (5000) 
/* In msecs */ 
#define LED_TOGGLE_TIMEOUT      (500)   

#define SERVER_CERTIFICATE_FILENAME     "ca_in_cert_store"
#define SERVER_PRIVATE_KEY_FILENAME     "ca-priv-key.der"

/*
 *  \brief  Application state's context
 */
typedef struct  _LocalOta_AppContext_t_
{
    LocalOtaState currentState;        /* Current state of the application */
    uint32_t pendingEvents;            /* Events pending to be processed */
    uint8_t role;                      /* SimpleLink's role - STATION/AP/P2P */
    uint32_t asyncEvtTimeout;          /* Timeout value*/
    uint32_t ledToggleTimeout;         /* Timeout value */
    uint32_t ledIndex;                 /* led index for toggling */
}LocalOta_AppContext;

/*!
 *  \brief  Function pointer to the event handler
 */
typedef int32_t (*fptr_EventHandler)();

/*!
 *  \brief  Entry in the lookup table
 */
typedef struct
{
    fptr_EventHandler p_evtHndl;    /* Pointer to the event handler */
    LocalOtaState nextState;        /* Next state of local ota */
}s_TblEntry;

/******************************************************************************
                      LOCAL FUNCTION PROTOTYPES
******************************************************************************/

static void SetNetAppHttp(int32_t *retVal,
                          const uint8_t Option,
                          const uint8_t OptionLen,
                          const uint8_t *pOptionValue);

//*****************************************************************************
//
//! \brief This function configures the HTTP server
//!
//! \param[in]  none
//!
//! \return NetApp error codes or 0 upon success.
//!
//*****************************************************************************
static int32_t ConfigureHttpServer();

//*****************************************************************************
//
//! \brief Depending upon the triggered events/error, this function traverses
//!         through the application's transition table
//!
//! \param[in]  None
//!
//! \return On success, zero is returned. On error, loops forever
//!
//*****************************************************************************
static int32_t localOtaAppTask();

//*****************************************************************************
//
//! \brief This function starts the SimpleLink in the configured role.
//!         The device notifies the host asynchronously 
//!         when the initialization is completed
//!
//! \param[in]  role    Device shall be configured in this role
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************

static int32_t InitSimplelink(uint8_t const role);

//*****************************************************************************
//! \brief This function handles 'LocalOtaEvent_Started' event and kick off
//!  the procedure
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t HandleStartConnect();

//*****************************************************************************
//! \brief This function initiates a connection request
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t StartConnection();

//*****************************************************************************
//! \brief This function starts timer for IP address acquisition
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t WaitForIpAddr();

//*****************************************************************************
//! \brief This function configures Simplelink to ping the gateway IP and 
//! check the LAN connection
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t CheckLanConnection();

//*****************************************************************************
//! \brief test for commit flag after ping is completed
//!          if commit is pending, try to commit.
//!          if commit is successful or no commit is required, continue
//!          to ota procedure.
//!          Upon unsuccessful commit, platform reboot is required.
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t HandlePingComplete();

//*****************************************************************************
//
//! \brief This function stops the network processor and restarts the mcu
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t ProcessRestartMcu();

//*****************************************************************************
//
//! \brief This function starts the async-event timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t StartAsyncEvtTimer();

//*****************************************************************************
//
//! \brief This function stops the async-event timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t StopAsyncEvtTimer();

//*****************************************************************************
//
//! \brief report SM during local ota process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t ReportSM();

//*****************************************************************************
//
//! \brief internal error detection during local ota process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t ReportError();
#ifdef CC32XX
//*****************************************************************************
//
//! Notify if device return to factory image
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void NotifyReturnToFactoryImage();
#endif

//*****************************************************************************
//
//! \brief The interrupt handler for the LED timer
//!
//! \param[in]  None
//!
//! \return None
//!
//*****************************************************************************
void LedTimerIntHandler(sigval val);

//*****************************************************************************
//
//! \brief This function handles ping report events
//!
//! \param[in]  pPingReport - Pointer to the structure containing ping report
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkPingReport(SlNetAppPingReport_t *pPingReport);

/******************************************************************************
                      GLOBAL VARIABLES
******************************************************************************/
/*!
 *  \brief  Application state's context
 */
LocalOta_AppContext gAppCtx;

timer_t gAsyncEventTimer;
timer_t gLedTimer;

/*!
 *  \brief   Application lookup/transition table
 */
const s_TblEntry gTransitionTable[LocalOtaState_Max][LocalOtaEvent_Max] =
{
    /* LocalOtaState_Starting */
    {
        /* LocalOtaEvent_Started          */ {HandleStartConnect,
                                              LocalOtaState_WaitForConnection},
        /* LocalOtaEvent_Connected        */ {ReportError,
                                              LocalOtaState_Error           },
        /* LocalOtaEvent_IpAcquired       */ {ReportError,
                                              LocalOtaState_Error           },
        /* LocalOtaEvent_Disconnected     */ {ProcessRestartMcu,
                                              LocalOtaState_Starting        },
        /* LocalOtaEvent_PingCompleted    */ {ReportError,
                                              LocalOtaState_Error           },
        /* LocalOtaEvent_OtaDownloadDone  */ {ReportError,
                                              LocalOtaState_Error           },
        /* LocalOtaEvent_Timeout          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting        },
        /* LocalOtaEvent_Error            */ {ReportError,
                                              LocalOtaState_Error           },
        /* LocalOtaEvent_Restart          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting        }
    },

    /* LocalOtaState_WaitForConnection */
    {
        /* LocalOtaEvent_Started          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Connected        */ {WaitForIpAddr,
                                              LocalOtaState_WaitForIP        },
        /* LocalOtaEvent_IpAcquired       */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Disconnected     */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
        /* LocalOtaEvent_PingCompleted    */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaDownloadDone  */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaError         */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Timeout          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
        /* LocalOtaEvent_Error            */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Restart          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         }
    },

    /* LocalOtaState_WaitForIP */
    {
        /* LocalOtaEvent_Started          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Connected        */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_IpAcquired       */ {CheckLanConnection,
                                              LocalOtaState_PingingGW        },
        /* LocalOtaEvent_Disconnected     */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
/* OTA - on disconnectio/ip lost do rollback 
   by reset, no pending commit check */
        /* LocalOtaEvent_PingCompleted    */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaDownloadDone  */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaError         */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Timeout          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
        /* LocalOtaEvent_Error            */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Restart          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         }
    },

    /* LocalOtaState_PingingGW */
    {
        /* LocalOtaEvent_Started          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Connected        */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_IpAcquired       */ {ReportError,
                                              LocalOtaState_Error            },
        /* OTA - on disconnection/ip lost, do rollback by reset, no pending 
        commit check */
        /* LocalOtaEvent_Disconnected     */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
        /* OTA - on ping complete, check for commit criteria */       
       /* LocalOtaEvent_PingCompleted    */ {HandlePingComplete,
                                              LocalOtaState_OtaRun           },
        /* LocalOtaEvent_OtaDownloadDone  */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaError         */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Timeout          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
        /* LocalOtaEvent_Error            */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Restart          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         }
    },

    /* LocalOtaState_OtaRun */
    {
        /* LocalOtaEvent_Started          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Connected        */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_IpAcquired       */ {ReportError,
                                              LocalOtaState_Error            },
        /* on disconnection/ip lost during OTA, stay in state 
           and wait for ERROR event and only then restart */
        /* LocalOtaEvent_Disconnected     */ {ReportSM,
                                              LocalOtaState_OtaRun           },
        /* LocalOtaEvent_PingCompleted    */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaDownloadDone  */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
         /* OTA fails. restart MCU as files 
            are during commit and need to get "free" */
        /* LocalOtaEvent_OtaError         */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         },
        /* LocalOtaEvent_Timeout          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Error            */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Restart          */ {ProcessRestartMcu,
                                              LocalOtaState_Starting         }
    },

    /* LocalOtaState_Error */
    {
        /* LocalOtaEvent_Started          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Connected        */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_IpAcquired       */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Disconnected     */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_PingCompleted    */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaDownloadDone  */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_OtaError         */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Timeout          */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Error            */ {ReportError,
                                              LocalOtaState_Error            },
        /* LocalOtaEvent_Restart          */ {ProcessRestartMcu,
                                              LocalOtaState_Error            }
    }
};

LocalOtaTask_CB LocalOtaTask_ControlBlock;

/******************************************************************************
                  Callback Functions
******************************************************************************/

//*****************************************************************************
//
//! \brief This function handles ping report events
//!
//! \param[in]  pPingReport - Pointer to the structure containing ping report
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkPingReport(SlNetAppPingReport_t *pPingReport)
{
    memset(&LocalOta_ControlBlock.pingReport, 0, sizeof(SlNetAppPingReport_t));
    memcpy(&LocalOta_ControlBlock.pingReport, pPingReport,
           sizeof(SlNetAppPingReport_t));

    if(LocalOta_ControlBlock.pingReport.PacketsReceived > 0 &&
       LocalOta_ControlBlock.pingReport.PacketsReceived <= NO_OF_ATTEMPTS)
    {
        UART_PRINT("[Local ota task] pinging to gateway succeeded \n\r");
        SignalEvent(LocalOtaEvent_PingCompleted);
    }
    else
    {
        UART_PRINT(
            "[Local ota task] pinging to gateway failed, no response \n\r");

        /* Restart MCU doing rollback if needed */
        SignalEvent(LocalOtaEvent_Restart);
    }
}

//*****************************************************************************
//
//! \brief The interrupt handler for the async-evt timer
//!
//! \param[in]  None
//!
//! \return None
//!
//*****************************************************************************
void AsyncEvtTimerIntHandler(sigval val)
{
    /* Clear the timer interrupt. */
    Platform_TimerInterruptClear();

    /* One Shot */
    /* TBD - Check role for One-shot/periodic */
    StopAsyncEvtTimer();
    SignalEvent(LocalOtaEvent_Timeout);
}

//*****************************************************************************
//
//! \brief The interrupt handler for the LED timer
//!
//! \param[in]  None
//!
//! \return None
//!
//*****************************************************************************
void LedTimerIntHandler(sigval val)
{
    LocalOta_AppContext *const pCtx = &gAppCtx;

    /* Clear the timer interrupt. */
    Platform_TimerInterruptClear();

    GPIO_toggle(pCtx->ledIndex);
}

//*****************************************************************************
//                 Local Functions
//*****************************************************************************

static void SetNetAppHttp(int32_t *retVal,
                          const uint8_t Option,
                          const uint8_t OptionLen,
                          const uint8_t *pOptionValue)
{
    if(*retVal >= 0)
    {
        int32_t i;

        *retVal = sl_NetAppSet(SL_NETAPP_HTTP_SERVER_ID, Option, OptionLen,
                               pOptionValue);
        INFO_PRINT("_SetNetAppHttp:: option=%d len=%d (", Option, OptionLen);

        for(i = 0; i < OptionLen; i++)
        {
            INFO_PRINT ("%2d ", pOptionValue[i]);
        }

        INFO_PRINT(")\n\r_SetNetAppHttp:: retval=%d\n\r", *retVal);
    }
}

//*****************************************************************************
//
//! \brief This function configures the HTTP server
//!
//! \param[in]  none
//!
//! \return NetApp error codes or 0 upon success.
//!
//*****************************************************************************
static int32_t ConfigureHttpServer()
{
    int32_t retVal = 0;
    uint8_t serverCACertificateFileName[] = "\0";
    uint8_t httpsPort[] = {0xBB, 0x01};  /* 0x1BB = 443 */
    uint8_t secondaryPort[] = {0x50, 0x00}; /* 0x050 = 80 */
    uint8_t secondaryPortEnable[] = {0x1};
    uint8_t securityMode = 1;

    UART_PRINT("[Local ota task] ConfigureHttpServer for secured mode...\n\r");

    /* Set the file names used for SSL key exchange */
    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_DEVICE_CERTIFICATE_FILENAME,
                  strlen((char *)SSL_SERVER_CERT),
                  (const uint8_t *)SSL_SERVER_CERT);

    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_PRIVATE_KEY_FILENAME,
                  strlen((char *)SSL_SERVER_KEY),
                  (const uint8_t *)SSL_SERVER_KEY);

    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_CA_CERTIFICATE_FILE_NAME,
                  sizeof(serverCACertificateFileName),
                  serverCACertificateFileName);

    /* Activate SSL security on primary HTTP port and change it to
       443 (standard HTTPS port) */
    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_PRIMARY_PORT_SECURITY_MODE,
                  sizeof(securityMode),
                  &securityMode);

    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_PRIMARY_PORT_NUMBER,
                  sizeof(httpsPort),
                  httpsPort);

    /* Enable secondary HTTP port (can only be used for redirecting
       connections to the secure primary port) */
    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_SECONDARY_PORT_NUMBER,
                  sizeof(secondaryPort),
                  secondaryPort);

    SetNetAppHttp(&retVal,
                  SL_NETAPP_HTTP_SECONDARY_PORT_ENABLE,
                  sizeof(secondaryPortEnable),
                  secondaryPortEnable);

    if(retVal >= 0)
    {
        retVal = sl_NetAppStop(SL_NETAPP_HTTP_SERVER_ID);
        UART_PRINT("[Local ota task] HTTP Server Stopped\n\r");
        if(retVal >= 0)
        {
            retVal = sl_NetAppStart(SL_NETAPP_HTTP_SERVER_ID);
            UART_PRINT("[Local ota task] HTTP Server Re-started\n\r");
        }
    }
    return(retVal);
}

//*****************************************************************************
//
//! \brief This function starts the SimpleLink in the configured role.
//!         The device notifies the host asynchronously when the 
//!         initialization is completed
//!
//! \param[in]  role    Device shall be configured in this role
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t InitSimplelink(uint8_t const role)
{
    LocalOta_AppContext *const pCtx = &gAppCtx;
    int32_t retVal = -1;
#ifdef __MSP432P401R__
    int32_t ret = -1;
#endif

    pCtx->role = role;
    pCtx->currentState = LocalOtaState_Starting;
    pCtx->pendingEvents = 0;

    retVal = sl_Start(0, 0, 0); /* without using SimpleLinkInitCallback */
    if (retVal < 0)
    {
        UART_PRINT("[Local ota task] sl_Start Failed %d \n\r", retVal);
        LOOP_FOREVER();
    }
#ifdef CC32XX
    UART_PRINT("[Local ota task] Device is configured in default state \n\r");
#endif
#ifdef __MSP432P401R__
    ret = ConfigureHttpServer();
    ASSERT_ON_ERROR(ret);
#endif
    if(pCtx->role == retVal)
    {
        UART_PRINT(
            "[Local ota task] SimpleLinkInitCallback: started in role %d\n\r",
            pCtx->role);
        SignalEvent(LocalOtaEvent_Started);
    }
    else
    {
        UART_PRINT(
            "[Local ota task] SimpleLinkInitCallback: started in role %d,"
            " set the requested role %d\n\r",
            retVal, pCtx->role);
        retVal = sl_WlanSetMode(pCtx->role);
        ASSERT_ON_ERROR(retVal);
        retVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(retVal);
        retVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(retVal);
        if(pCtx->role != retVal)
        {
            UART_PRINT(
                "[Local ota task] SimpleLinkInitCallback: error setting"
                " role %d, status=%d\n\r",
                pCtx->role, retVal);
            SignalEvent(LocalOtaEvent_Error);
        }
        UART_PRINT(
           "[Local ota task] SimpleLinkInitCallback: restarted in role %d\n\r",
            pCtx->role);
        pCtx->pendingEvents = 0;
        SignalEvent(LocalOtaEvent_Started);
    }

    /* Start timer */
    pCtx->asyncEvtTimeout = ASYNC_EVT_TIMEOUT;
    retVal = StartAsyncEvtTimer(pCtx->asyncEvtTimeout);
    ASSERT_ON_ERROR(retVal);

    return(retVal);
}

//*****************************************************************************
//! \brief This function handles 'LocalOtaEvent_Started' event and kick off the
//!        procedure
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t HandleStartConnect()
{
    SlDeviceVersion_t firmwareVersion = {0};

    int32_t retVal = -1;
    uint8_t ucConfigOpt = 0;
    uint16_t ucConfigLen = 0;

    /* Get the device's version-information */
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(firmwareVersion);
    retVal =
        sl_DeviceGet(SL_DEVICE_GENERAL, &ucConfigOpt, &ucConfigLen,
                     (uint8_t *)(&firmwareVersion));
    ASSERT_ON_ERROR(retVal);

    UART_PRINT("[Local ota task] Host Driver Version: %s\n\r",
               SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r", \
               firmwareVersion.NwpVersion[0], firmwareVersion.NwpVersion[1], \
               firmwareVersion.NwpVersion[2], firmwareVersion.NwpVersion[3], \
               firmwareVersion.FwVersion[0], firmwareVersion.FwVersion[1], \
               firmwareVersion.FwVersion[2], firmwareVersion.FwVersion[3], \
               firmwareVersion.PhyVersion[0], firmwareVersion.PhyVersion[1], \
               firmwareVersion.PhyVersion[2], firmwareVersion.PhyVersion[3]);

    retVal = StartConnection();
    ASSERT_ON_ERROR(retVal);

    return(retVal);
}

//*****************************************************************************
//! \brief This function initiates a connection request
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t StartConnection()
{
    SlWlanSecParams_t secParams = {0};
    int32_t retVal = 0;

    secParams.Key = (signed char*)SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    UART_PRINT("[Local ota task] WlanConnect SSID=%s\n\r", SSID_NAME);
    retVal = sl_WlanConnect((signed char*)SSID_NAME, \
                            strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(retVal);

    retVal = StartAsyncEvtTimer(ASYNC_EVT_TIMEOUT);
    ASSERT_ON_ERROR(retVal);

    return(retVal);
}

//*****************************************************************************
//! \brief This function starts timer for IP address acquisition
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t WaitForIpAddr()
{
    int32_t retVal = 0;

    retVal = StartAsyncEvtTimer(ASYNC_EVT_TIMEOUT);
    ASSERT_ON_ERROR(retVal);

    return(retVal);
}

//*****************************************************************************
//! \brief This function configures Simplelink to ping the gateway IP and check
//!        the LAN connection
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t CheckLanConnection()
{
    LocalOta_AppContext *const pCtx = &gAppCtx;

    /* LAN connection acquired */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    pCtx->ledIndex = CONFIG_GPIO_LED_2;

    SlNetAppPingCommand_t pingParams = {0};
    SlNetAppPingReport_t pingReport = {0};

    int32_t retVal = -1;

    /* Set the ping parameters */
    pingParams.PingIntervalTime = PING_INTERVAL;
    pingParams.PingSize = PING_PKT_SIZE;
    pingParams.PingRequestTimeout = PING_TIMEOUT;
    pingParams.TotalNumberOfAttempts = NO_OF_ATTEMPTS;
    pingParams.Flags = 0;

    pingParams.Ip = LocalOta_ControlBlock.gatewayIP;

    /* Ping the GW */
    retVal = sl_NetAppPing((SlNetAppPingCommand_t*)&pingParams, \
                           SL_AF_INET, (SlNetAppPingReport_t*)&pingReport, \
                           SimpleLinkPingReport);
    ASSERT_ON_ERROR(retVal);

    /* Compute worst case ping timeout */
    pCtx->asyncEvtTimeout = ((NO_OF_ATTEMPTS * PING_TIMEOUT) + \
                             (NO_OF_ATTEMPTS * PING_INTERVAL) + \
                             (2000)); /* Additional time */

    retVal = StartAsyncEvtTimer(pCtx->asyncEvtTimeout);
    ASSERT_ON_ERROR(retVal);

    UART_PRINT("[Local ota task] Pinging gateway, please wait...\n\r");

    return(retVal);
}

//*****************************************************************************
//
//! \brief This function starts the async-event timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t StartAsyncEvtTimer(uint32_t timeout)
{
    LocalOta_AppContext *const pCtx = &gAppCtx;

    pCtx->asyncEvtTimeout = timeout;
    Platform_TimerStart(pCtx->asyncEvtTimeout, gAsyncEventTimer, 0);
    return(0);
}

//*****************************************************************************
//
//! \brief This function stops the async-event timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t StopAsyncEvtTimer()
{
    LocalOta_AppContext *const pCtx = &gAppCtx;

    if(0 != pCtx->asyncEvtTimeout)
    {
        Platform_TimerStop(gAsyncEventTimer);
        pCtx->asyncEvtTimeout = 0;
    }

    return(0);
}

//*****************************************************************************
//
//! \brief report SM during local ota process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t ReportSM()
{
    LocalOta_AppContext *const pCtx = &gAppCtx;
    uint16_t eventIdx = 0;

    for(eventIdx = 0; eventIdx < LocalOtaEvent_Max; eventIdx++)
    {
        if(0 != (pCtx->pendingEvents & (1 << eventIdx)))
        {
            break;
        }
    }

    UART_PRINT("[Local ota task] SM: State = %d, Event = %d\n\r", \
               pCtx->currentState, eventIdx);
    return(0);
}

//*****************************************************************************
//
//! \brief internal error detection during local ota process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t ReportError()
{
    LocalOta_AppContext *const pCtx = &gAppCtx;
    uint16_t eventIdx = 0;

    for(eventIdx = 0; eventIdx < LocalOtaEvent_Max; eventIdx++)
    {
        if(0 != (pCtx->pendingEvents & (1 << eventIdx)))
        {
            break;
        }
    }

    UART_PRINT("[Local ota task] Unexpected SM: State = %d, Event = %d\n\r", \
               pCtx->currentState, eventIdx);
    return(-1);
}

//*****************************************************************************
//! \brief test for commit flag after ping is completed
//!          if commit is pending, try to commit.
//!          if commit is successful or no commit is required, continue to ota 
//!          procedure.
//!          Upon unsuccessful commit, platform reboot is required.
//!
//! \param   none
//!
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t HandlePingComplete()
{
    int16_t isPendingCommit;

    StopLedEvtTimer();

    /* Turn on GREEN LED when device gets PING response from AP */
    GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_ON);

    isPendingCommit = OtaArchive_GetPendingCommit();

    if(isPendingCommit)         /* validation of new ota bundle succeeded,
    commit the new ota bundle */
    {
        UART_PRINT("[Local ota task] committing new ota download... \n\r");
        if(OtaArchive_Commit() < 0)
        {
            UART_PRINT(
                "[Local ota task] failed to commit new download, reverting to"
                " previous copy by reseting the device \n\r");
            SignalEvent(LocalOtaEvent_Restart);

            return(-1);
        }
        UART_PRINT("[Local ota task] commit succeeded \n\r");

        /* need to stop the WDT so MCU is not reset */
#ifdef CC32XX
        PowerCC32XX_reset(PowerCC32XX_PERIPH_WDT);
#endif
    }

    UART_PRINT("[Local ota task] waiting for new ota upload... \n\r");

    /* signal to linklocal task */
    sem_post(&LocalOtaTask_ControlBlock.localOtaConnDoneSignal);

    /* signal to report server task */
    sem_post(&LocalOtaTask_ControlBlock.localOtaConnDoneToOtaServerSignal);

    return(0);
}

//*****************************************************************************
//
//! \brief This function stops the network processor and restarts the mcu
//!
//! \param[in]  None
//!
//! \return 0 on success
//!
//*****************************************************************************
static int32_t ProcessRestartMcu()
{
    mcuReboot();

    return(0);
}

//*****************************************************************************
//
//! \brief main local ota loop
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
static int32_t localOtaAppTask()
{
    LocalOta_AppContext *const pCtx = &gAppCtx;
    s_TblEntry   *pEntry = NULL;
    uint16_t eventIdx = 0;

    for(eventIdx = 0; eventIdx < LocalOtaEvent_Max; eventIdx++)
    {
        if(0 != (pCtx->pendingEvents & (1 << eventIdx)))
        {
            if(0 != eventIdx)
            {
                /** Events received - Stop the respective timer if its still
                 * running
                 */
                StopAsyncEvtTimer();
            }

            pEntry =
                (s_TblEntry *)&gTransitionTable[pCtx->currentState][eventIdx];
            if(eventIdx == LocalOtaEvent_Timeout)
            {
                UART_PRINT(
                   "[Local ota task] Handle LocalOtaEvent_Timeout in state=%d,"
                   " next=%d\n\r",
                    pCtx->currentState, pEntry->nextState);
            }
            if(eventIdx == LocalOtaEvent_Error)
            {
                UART_PRINT(
                    "[Local ota task] Handle LocalOtaEvent_Error in state=%d,"
                    " next=%d\n\r",
                    pCtx->currentState, pEntry->nextState);
            }
            if(eventIdx == LocalOtaEvent_Restart)
            {
                UART_PRINT(
                    "[Local ota task] Handle LocalOtaEvent_Restart in state=%d,"
                    " next=%d\n\r",
                    pCtx->currentState, pEntry->nextState);
            }
            pCtx->pendingEvents &= ~(1 << eventIdx);
            if(NULL != pEntry->p_evtHndl)
            {
                if(pEntry->p_evtHndl() < 0)
                {
                    UART_PRINT(
                        "[Local ota task] Event handler failed..!! halt\n\r");
                    LOOP_FOREVER();
                }
            }

            if(pEntry->nextState != pCtx->currentState)
            {
                pCtx->currentState = pEntry->nextState;
            }

            pCtx->pendingEvents &= ~(1 << eventIdx);
        }

        /* No more events to handle. Break.! */
        if(0 == pCtx->pendingEvents)
        {
            break;
        }
    }

    usleep(1000);

    return(0);
}

//*****************************************************************************
//
//! Notify if device return to factory image
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
#ifdef CC32XX
static void NotifyReturnToFactoryImage()
{
    if(((HWREG(HIB3P3_BASE + 0x00000418) & (1 << 7)) != 0) &&
       ((HWREG(0x4402F0C8) & 0x01) != 0))
    {
        UART_PRINT("Return To Factory Image successful, Do a power cycle(POR)"
                   " of the device using switch SW1-Reset\n\r");
        while(1)
        {
            ;
        }
    }
}

#endif
//*****************************************************************************
//                            MAIN FUNCTION
//*****************************************************************************

//*****************************************************************************
//
//! \brief The interrupt handler for the async-evt timer
//!
//! \param[in]  None
//!
//! \return None
//!
//*****************************************************************************
int16_t SignalEvent(LocalOtaEvent event)
{
    LocalOta_AppContext *const pCtx = &gAppCtx;
    pCtx->pendingEvents |= (1 << event);
    return(0);
}

//*****************************************************************************
//
//! \brief This function starts the led toggling timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
int32_t StartLedEvtTimer(uint32_t timeout)
{
    LocalOta_AppContext *const pCtx = &gAppCtx;

    pCtx->ledToggleTimeout = timeout;
    Platform_TimerStart(pCtx->ledToggleTimeout, gLedTimer, 1);

    return(0);
}

//*****************************************************************************
//
//! \brief This function stops the led toggling timer
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//*****************************************************************************
int32_t StopLedEvtTimer()
{
    LocalOta_AppContext *const pCtx = &gAppCtx;

    if(0 != pCtx->ledToggleTimeout)
    {
        Platform_TimerStop(gLedTimer);
        pCtx->ledToggleTimeout = 0;
    }

    return(0);
}

//*****************************************************************************
//
//! \brief This is the main local ota task
//!
//! \param[in]  None
//!
//! \return None
//!
//*****************************************************************************
void * localOtaTask(void *pvParameters)
{
    int32_t retVal = -1;
    LocalOta_AppContext     *const pCtx = &gAppCtx;

#ifdef CC32XX
    /* Check the wakeup source. If first time entry or wakeup from HIB */
    if(MAP_PRCMSysResetCauseGet() == 0)
    {
        UART_PRINT("[Local ota task] Wake up on Power ON\n\r");
    }
    else if(MAP_PRCMSysResetCauseGet() == PRCM_HIB_EXIT)
    {
        UART_PRINT("[Local ota task] Woken up from Hibernate\n\r");
    }
#endif
    /* Configure toggle LEDs  */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_OFF);

    Platform_TimerInit(AsyncEvtTimerIntHandler, &gAsyncEventTimer);
    Platform_TimerInit(LedTimerIntHandler, &gLedTimer);

    /* Set the LED toggling timeout and index before starting the timer */
    pCtx->ledIndex = CONFIG_GPIO_LED_0;
    pCtx->ledToggleTimeout = LED_TOGGLE_TIMEOUT;

    StartLedEvtTimer(pCtx->ledToggleTimeout);
#ifdef CC32XX
    /* check whether return-to-default occured */
    NotifyReturnToFactoryImage();

    /*
         IMPORTANT NOTE - This is an example reset function, user must update
                          this function to match the application settings.
     */
    retVal = sl_WifiConfig();
    if(retVal < 0)
    {
        UART_PRINT(
            "[Local ota task] Failed to configure the device in its default"
            " state.., halt\n\r");
        LOOP_FOREVER();
    }
    /* Turn NWP on */
    retVal = sl_Start(NULL, NULL, NULL);
    if (retVal < 0)
    {
        UART_PRINT("[Local ota task] sl_Start Failed %d \n\r", retVal);
        LOOP_FOREVER();
    }

    /* Unregister mDNS services */
    retVal = sl_NetAppMDNSUnRegisterService(0, 0, 0);
    if (retVal < 0)
    {
        UART_PRINT("[Local ota task] Unregister mDNS services Failed %d \n\r", retVal);
        LOOP_FOREVER();
    }

    /* Re-activate HTTP server as secured/non-secured */
    retVal = ConfigureHttpServer();
    if(retVal < 0)
    {
        UART_PRINT("[Local ota task] Failed to configure Http server\n\r");
        LOOP_FOREVER();
    }
    /* For changes to take affect, we restart the NWP */
    retVal = sl_Stop(200);
    if (retVal < 0)
    {
        UART_PRINT("[Local ota task] sl_Stop Failed %d \n\r", retVal);
        LOOP_FOREVER();
    }

#endif
    /* Configure SimpleLink in STATION role */
    retVal = InitSimplelink(ROLE_STA);
    if(retVal < 0)
    {
        UART_PRINT(
            "[Local ota task] Failed to initialize the device!!, halt\n\r");
        LOOP_FOREVER();
    }
    getDeviceType();
    do
    {
        retVal = localOtaAppTask();
    }
    while(!retVal);  /* Exit on failure */

    return(0);
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
