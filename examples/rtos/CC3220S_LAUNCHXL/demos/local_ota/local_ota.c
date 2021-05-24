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

/*****************************************************************************

  Application Name     - Getting started with LOCAL_OTA
  Application Overview - This application demonstrates the local ota Experience
                         with CC32xx LaunchPad.
                         It highlights an easy access to CC32xx Using internal
                          HTTP server

   Application Details  - Refer to 'Local OTA' README.html

*****************************************************************************/

//****************************************************************************
//
//! \addtogroup local_ota
//! @{
//
//****************************************************************************

/* Example/Board Header files */
#include "local_ota.h"
#include "local_ota_task.h"
#include "link_local_task.h"
#include "ota_report_server_task.h"

/* TI-DRIVERS Header files */
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/SPI.h>
#include <ti_drivers_config.h>
#ifdef CC32XX
/* driverlib Header files */
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/inc/hw_memmap.h>
#include <ti/devices/cc32xx/driverlib/rom.h>
#include <ti/devices/cc32xx/driverlib/rom_map.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>
#endif
/* POSIX Header files */
#include <mqueue.h>

#ifdef __MSP432P401R__
//*****************************************************************************
//
//! Initiates a soft system reset.
//!
//! \return none
//
//*****************************************************************************
#include <ti/devices/msp432p4xx/driverlib/rom_map.h>
extern void MAP_ResetCtl_initiateSoftReset(void);
#endif

/****************************************************************************
                      LOCAL FUNCTION PROTOTYPES
****************************************************************************/
//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void DisplayBanner(char * AppName,
                          char * AppVer);

//*****************************************************************************
//
//! This function initializes the application variables
//!
//! \param    None
//!
//! \return None
//!
//*****************************************************************************
static void InitializeAppVariables();

//*****************************************************************************
//                 GLOBAL VARIABLES
//*****************************************************************************
pthread_t gLocalOtaThread = (pthread_t)NULL;
pthread_t gLinklocalThread = (pthread_t)NULL;
pthread_t gOtaReportServerThread = (pthread_t)NULL;
pthread_t gSpawnThread = (pthread_t)NULL;

LocalOta_CB LocalOta_ControlBlock;

/*****************************************************************************
                  Callback Functions
*****************************************************************************/

//*****************************************************************************
//
//! The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    switch(pWlanEvent->Id)
    {
    case SL_WLAN_EVENT_CONNECT:
    {
        SET_STATUS_BIT(LocalOta_ControlBlock.status, AppStatusBits_Connection);
        CLR_STATUS_BIT(LocalOta_ControlBlock.status, AppStatusBits_IpAcquired);
        CLR_STATUS_BIT(LocalOta_ControlBlock.status,
                       AppStatusBits_Ipv6lAcquired);
        CLR_STATUS_BIT(LocalOta_ControlBlock.status,
                       AppStatusBits_Ipv6gAcquired);

        /*
           Information about the connected AP (like name, MAC etc) will be
           available in 'slWlanConnectAsyncResponse_t'-Applications
           can use it if required:

           slWlanConnectAsyncResponse_t *pEventData = NULL;
           pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
         */

        /* Copy new connection SSID and BSSID to global parameters */
        memcpy(LocalOta_ControlBlock.connectionSSID,
               pWlanEvent->Data.Connect.SsidName,
               pWlanEvent->Data.Connect.SsidLen);

        LocalOta_ControlBlock.ssidLen = pWlanEvent->Data.Connect.SsidLen;

        memcpy(LocalOta_ControlBlock.connectionBSSID,
               pWlanEvent->Data.Connect.Bssid,
               SL_WLAN_BSSID_LENGTH);

        UART_PRINT(
            "[WLAN EVENT] STA Connected to the AP: %s ,"
            "BSSID: %x:%x:%x:%x:%x:%x\n\r",
            LocalOta_ControlBlock.connectionSSID,
            LocalOta_ControlBlock.connectionBSSID[0],
            LocalOta_ControlBlock.connectionBSSID[1],
            LocalOta_ControlBlock.connectionBSSID[2],
            LocalOta_ControlBlock.connectionBSSID[3],
            LocalOta_ControlBlock.connectionBSSID[4],
            LocalOta_ControlBlock.connectionBSSID[5]);

        SignalEvent(LocalOtaEvent_Connected);
    }
    break;

    case SL_WLAN_EVENT_DISCONNECT:
    {
        SlWlanEventDisconnect_t*    pEventData = NULL;

        CLR_STATUS_BIT(LocalOta_ControlBlock.status, AppStatusBits_Connection);
        CLR_STATUS_BIT(LocalOta_ControlBlock.status, AppStatusBits_IpAcquired);
        CLR_STATUS_BIT(LocalOta_ControlBlock.status,
                       AppStatusBits_Ipv6lAcquired);
        CLR_STATUS_BIT(LocalOta_ControlBlock.status,
                       AppStatusBits_Ipv6gAcquired);

        pEventData = &pWlanEvent->Data.Disconnect;

        /*  If the user has initiated 'Disconnect' request,
            'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED.
         */
        if(SL_WLAN_DISCONNECT_USER_INITIATED == pEventData->ReasonCode)
        {
            UART_PRINT(
                "[WLAN EVENT]Device disconnected from the "
                "AP: %s, BSSID: %x:%x:%x:%x:%x:%x "
                "on application's request \n\r",
                LocalOta_ControlBlock.connectionSSID,
                LocalOta_ControlBlock.connectionBSSID[0],
                LocalOta_ControlBlock.connectionBSSID[1],
                LocalOta_ControlBlock.connectionBSSID[2],
                LocalOta_ControlBlock.connectionBSSID[3],
                LocalOta_ControlBlock.connectionBSSID[4],
                LocalOta_ControlBlock.connectionBSSID[5]);
        }
        else
        {
            UART_PRINT(
                "[WLAN ERROR]Device disconnected from the AP AP: %s,"
                "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                LocalOta_ControlBlock.connectionSSID,
                LocalOta_ControlBlock.connectionBSSID[0],
                LocalOta_ControlBlock.connectionBSSID[1],
                LocalOta_ControlBlock.connectionBSSID[2],
                LocalOta_ControlBlock.connectionBSSID[3],
                LocalOta_ControlBlock.connectionBSSID[4],
                LocalOta_ControlBlock.connectionBSSID[5]);
        }
        memset(LocalOta_ControlBlock.connectionSSID, 0,
               sizeof(LocalOta_ControlBlock.connectionSSID));
        memset(LocalOta_ControlBlock.connectionBSSID, 0,
               sizeof(LocalOta_ControlBlock.connectionBSSID));

        SignalEvent(LocalOtaEvent_Disconnected);
    }
    break;

    case SL_WLAN_EVENT_PROVISIONING_STATUS:
    {
        UART_PRINT(
            "[WLAN EVENT] Unexpected WLAN event with Id [0x%x], Ignored\n\r",
            pWlanEvent->Id);
    }
    break;

    default:
    {
        UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                   pWlanEvent->Id);

        SignalEvent(LocalOtaEvent_Error);
    }
    break;
    }
}

//*****************************************************************************
//
//! The Function Handles the Fatal errors
//!
//! \param[in]  slFatalErrorEvent - Pointer to Fatal Error Event info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
    switch(slFatalErrorEvent->Id)
    {
    case SL_DEVICE_EVENT_FATAL_DEVICE_ABORT:
    {
        UART_PRINT("[ERROR] - FATAL ERROR: Abort NWP event detected: "
                   "AbortType=%d, AbortData=0x%x\n\r",
                   slFatalErrorEvent->Data.DeviceAssert.Code,
                   slFatalErrorEvent->Data.DeviceAssert.Value);
    }
    break;

    case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
    {
        UART_PRINT("[ERROR] - FATAL ERROR: Driver Abort detected. \n\r");
    }
    break;

    case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
    {
        UART_PRINT("[ERROR] - FATAL ERROR: No Cmd Ack detected "
                   "[cmd opcode = 0x%x] \n\r",
                   slFatalErrorEvent->Data.NoCmdAck.Code);
    }
    break;

    case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
    {
        UART_PRINT("[ERROR] - FATAL ERROR: Sync loss detected n\r");
    }
    break;

    case SL_DEVICE_EVENT_FATAL_CMD_TIMEOUT:
    {
        UART_PRINT("[ERROR] - FATAL ERROR: Async event timeout detected "
                   "[event opcode =0x%x]  \n\r",
                   slFatalErrorEvent->Data.CmdTimeout.Code);
    }
    break;

    default:
    {
        UART_PRINT("[ERROR] - FATAL ERROR: Unspecified error detected \n\r");

        break;
    }
    }

    SignalEvent(LocalOtaEvent_Restart);
}

//*****************************************************************************
//
//! This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    switch(pNetAppEvent->Id)
    {
    case SL_NETAPP_EVENT_IPV4_ACQUIRED:
    {
        SlIpV4AcquiredAsync_t   *pEventData = NULL;

        SET_STATUS_BIT(LocalOta_ControlBlock.status, AppStatusBits_IpAcquired);

        /* Ip Acquired Event Data */
        pEventData = &pNetAppEvent->Data.IpAcquiredV4;

        /* Gateway IP address */
        LocalOta_ControlBlock.gatewayIP = pEventData->Gateway;

        UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                   "Gateway=%d.%d.%d.%d\n\r",
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,3),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,2),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,1),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,0),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,3),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,2),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,1),
                   SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,0));

        SignalEvent(LocalOtaEvent_IpAcquired);
    }
    break;

    case SL_NETAPP_EVENT_DHCP_IPV4_ACQUIRE_TIMEOUT:
    {
        UART_PRINT("[NETAPP EVENT] DHCP IPv4 Acquire timeout\n\r");
        /* use existing disconnect event, no need for another event */
        SignalEvent(LocalOtaEvent_Disconnected);     
    }
    break;

    default:
    {
        UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                   pNetAppEvent->Id);

        SignalEvent(LocalOtaEvent_Error);
    }
    break;
    }
}

//*****************************************************************************
//
//! This function handles HTTP server events
//!
//! \param[in]  pServerEvent    - Contains the relevant event information
//! \param[in]  pServerResponse - Should be filled by the user with the
//!                               relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerEventHandler(SlNetAppHttpServerEvent_t *pHttpEvent,
    SlNetAppHttpServerResponse_t *
    pHttpResponse)
{
    /* Unused in this application */
    UART_PRINT("[HTTP SERVER EVENT] Unexpected HTTP server event \n\r");

    SignalEvent(LocalOtaEvent_Error);
}

//*****************************************************************************
//
//! This function handles General Events
//!
//! \param[in]  pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /*
        Most of the general errors are not FATAL are are to be handled
        appropriately by the application.
     */
    if(NULL == pDevEvent)
    {
        return;
    }
    switch(pDevEvent->Id)
    {
    case SL_DEVICE_EVENT_RESET_REQUEST:
    {
        UART_PRINT("[GENERAL EVENT] Reset Request Event\r\n");
    }
    break;

    default:
    {
        UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
                   pDevEvent->Data.Error.Code,
                   pDevEvent->Data.Error.Source);

        SignalEvent(LocalOtaEvent_Restart);
    }
    break;
    }
}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]  pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    if(SL_SOCKET_ASYNC_EVENT == pSock->Event)
    {
        UART_PRINT("[SOCK ERROR] an event received on socket %d\r\n",
                   pSock->SocketAsyncEvent.SockAsyncData.Sd);
        switch(pSock->SocketAsyncEvent.SockAsyncData.Type)
        {
        case SL_SSL_NOTIFICATION_CONNECTED_SECURED:
            UART_PRINT("[SOCK ERROR] SSL handshake done");
            break;
        case SL_SSL_NOTIFICATION_HANDSHAKE_FAILED:
            UART_PRINT("[SOCK ERROR] SSL handshake failed with error %d\r\n",
                       pSock->SocketAsyncEvent.SockAsyncData.Val);
            break;
        case SL_SSL_ACCEPT:
            UART_PRINT(
                "[SOCK ERROR] Recoverable error occurred during the"
                " handshake %d\r\n",
                pSock->SocketAsyncEvent.SockAsyncData.Val);
            break;
        case SL_OTHER_SIDE_CLOSE_SSL_DATA_NOT_ENCRYPTED:
            UART_PRINT("[SOCK ERROR] Other peer terminated the SSL layer.\r\n");
            break;
        case SL_SSL_NOTIFICATION_WRONG_ROOT_CA:
            UART_PRINT("[SOCK ERROR] Used wrong CA to verify the peer.\r\n");

            break;
        default:
            break;
        }
    }

    /* This application doesn't work w/ socket - Events are not expected */
    switch(pSock->Event)
    {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch(pSock->SocketAsyncEvent.SockTxFailData.Status)
        {
        case SL_ERROR_BSD_ECLOSE:
            UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                       "failed to transmit all queued packets\n\r",
                       pSock->SocketAsyncEvent.SockTxFailData.Sd);
            break;
        default:
            UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , "
                       "reason (%d) \n\n",
                       pSock->SocketAsyncEvent.SockTxFailData.Sd,
                       pSock->SocketAsyncEvent.SockTxFailData.Status);
            break;
        }

        break;

    default:
        UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
        break;
    }
}

void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    /* Unused in this application */
}

/*****************************************************************************
                 Local Functions
*****************************************************************************/

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void DisplayBanner(char * AppName,
                          char * AppVer)
{
    UART_PRINT("\n\n\n\r");
    UART_PRINT("\t\t =================================================\n\r");
    UART_PRINT("\t\t           %s Example Ver. %s      \n\r", AppName, AppVer);
    UART_PRINT("\t\t =================================================\n\r");
    UART_PRINT("\n\n\n\r");
}

//*****************************************************************************
//
//! This function initializes the application variables
//!
//! \param    None
//!
//! \return None
//!
//*****************************************************************************
static void InitializeAppVariables()
{
    LocalOta_ControlBlock.status = 0;
    LocalOta_ControlBlock.gatewayIP = 0;

    memset(LocalOta_ControlBlock.connectionSSID, 0,
           sizeof(LocalOta_ControlBlock.connectionSSID));
    memset(LocalOta_ControlBlock.connectionBSSID, 0,
           sizeof(LocalOta_ControlBlock.connectionBSSID));
}

/*****************************************************************************
                 Main Functions
*****************************************************************************/

//*****************************************************************************
//
//! \brief This function reboot the M4 host processor
//!
//! \param[in]  none
//!
//! \return none
//!
//****************************************************************************
void mcuReboot(void)
{
    /* stop network processor activities before reseting the MCU */
    sl_Stop(SL_STOP_TIMEOUT);

#ifdef __MSP432P401R__
    MAP_ResetCtl_initiateSoftReset();
#elif  CC32XX
    UART_PRINT("[Common] CC32xx MCU reset request\r\n");

    /* Reset the MCU in order to test the bundle */
    PRCMHibernateCycleTrigger();
#endif
}

void * mainThread(void *arg)
{
    uint32_t RetVal;
    pthread_attr_t pAttrs;
    pthread_attr_t pAttrs_spawn;
    struct sched_param priParam;
    struct timespec ts = {0};

    GPIO_init();
    SPI_init();

    /* init Terminal, and print App name */
    InitTerm();
    /* initilize the realtime clock */
    clock_settime(CLOCK_REALTIME, &ts);

    DisplayBanner(APPLICATION_NAME, APPLICATION_VERSION);

    InitializeAppVariables();

    /* Switch off all LEDs on boards */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_OFF);

    /* initializes signals for all tasks */
    sem_init(&LocalOtaTask_ControlBlock.localOtaConnDoneSignal, 0, 0);
    sem_init(&LocalOtaTask_ControlBlock.localOtaConnDoneToOtaServerSignal,
                                                                        0,
                                                                        0);
    sem_init(&LinkLocal_ControlBlock.otaReportServerStartSignal, 0, 0);
    sem_init(&LinkLocal_ControlBlock.otaReportServerStopSignal, 0, 0);

    /* create the sl_Task */
    pthread_attr_init(&pAttrs_spawn);
    priParam.sched_priority = SPAWN_TASK_PRIORITY;
    RetVal = pthread_attr_setschedparam(&pAttrs_spawn, &priParam);
    RetVal |= pthread_attr_setstacksize(&pAttrs_spawn, TASK_STACK_SIZE);

    RetVal = pthread_create(&gSpawnThread, &pAttrs_spawn, sl_Task, NULL);

    if(RetVal)
    {
        while(1)
        {
            ;
        }
    }

    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 1;
    RetVal = pthread_attr_setschedparam(&pAttrs, &priParam);
    RetVal |= pthread_attr_setstacksize(&pAttrs, TASK_STACK_SIZE);

    if(RetVal)
    {
        while(1)
        {
            ;        /* error handling */
        }
    }

    RetVal = pthread_create(&gLocalOtaThread, &pAttrs, localOtaTask, NULL);

    if(RetVal)
    {
        while(1)
        {
            ;
        }
    }

    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 1;
    RetVal = pthread_attr_setschedparam(&pAttrs, &priParam);
    RetVal |= pthread_attr_setstacksize(&pAttrs, LINKLOCAL_STACK_SIZE);

    if(RetVal)
    {
        while(1)
        {
            ;        /* error handling */
        }
    }

    RetVal = pthread_create(&gLinklocalThread, &pAttrs, linkLocalTask, NULL);

    if(RetVal)
    {
        while(1)
        {
            ;
        }
    }

    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 5;
    RetVal = pthread_attr_setschedparam(&pAttrs, &priParam);
    RetVal |= pthread_attr_setstacksize(&pAttrs, TASK_STACK_SIZE);

    if(RetVal)
    {
        while(1)
        {
            ;        /* error handling */
        }
    }

    RetVal =
        pthread_create(&gOtaReportServerThread, &pAttrs, otaReportServerTask,
                       NULL);

    if(RetVal)
    {
        while(1)
        {
            ;
        }
    }

    return(0);
}
