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

   Application Name     - Trigger Mode application
   Application Overview - This application demonstrates how implement 
                          trigger mode functionality
                        In this mode, the MCU can enter LP mode 
                        and be woken up upon a reception
                        of data on an open socket.

   Application Details  - Refer to 'Trigger Mode' README.html

*****************************************************************************/
//****************************************************************************
//
//! \addtogroup
//! @{
//
//****************************************************************************

/* Standard Include */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* TI-DRIVERS Header files */
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/GPIO.h>
#include "uart_term.h"
#include "mqueue.h"

/* Application Version and Naming*/
#define APPLICATION_NAME         "Trigger Mode"
#define APPLICATION_VERSION      "01.00.00.04"

/* USER's defines */
#define SL_STOP_TIMEOUT                         (200)

/* Provisioning inactivity timeout (20 min)*/
#define PROVISIONING_INACTIVITY_TIMEOUT         (1200)

/* 15 seconds */         
#define RECONNECTION_ESTABLISHED_TIMEOUT_SEC    (15) 

/* 2 seconds */          
#define CONNECTION_PHASE_TIMEOUT_SEC            (2)       

/* 1 second */     
#define PING_TIMEOUT_SEC                        (1)  

/* Maximum size of the event queue (nonOS) */          
#define EVENT_QUEUE_SIZE                        (13)        

/* Enable UART Log */
#define LOG_MESSAGE_ENABLE
#define LOG_MESSAGE UART_PRINT
#define SC_KEY_ENABLE (0)
#define SECURED_AP_ENABLE (0)

#define AP_SEC_TYPE (SL_WLAN_SEC_TYPE_WPA_WPA2)
#define AP_SEC_PASSWORD "1234567890"
#define SC_KEY          "1234567890123456"

/* For Debug - Start Provisioning for each reset, or stay connected */
/* Used mainly in case of provisioning robustness tests             */
#define FORCE_PROVISIONING   (0)

#define ASSERT_ON_ERROR(error_code) \
    { \
        /* Handling the error-codes is specific to the application */ \
        if(error_code < 0) {LOG_MESSAGE("ERROR! %d \n\r",error_code); \
                            gLedDisplayState = LedState_ERROR; } \
        /* else, continue w/ execution */ \
    } \

#define ROLE_STA 0
#define ROLE_AP  2

/* Application's states */
typedef enum
{
    AppState_STARTING = 0,
    AppState_WAIT_FOR_CONNECTION,
    AppState_WAIT_FOR_IP,

    AppState_PINGING_GW,

    AppState_PROVISIONING_IN_PROGRESS,
    AppState_PROVISIONING_WAIT_COMPLETE,

    /* New state for Trigger Mode! */
    AppState_TRIGGER_MODE,

    AppState_ERROR,
    AppState_MAX
}AppState;

/* Application's events */
typedef enum
{
    AppEvent_STARTED = 0,
    AppEvent_CONNECTED,
    AppEvent_IP_ACQUIRED,
    AppEvent_DISCONNECT,

    AppEvent_PING_COMPLETE,

    AppEvent_PROVISIONING_STARTED,
    AppEvent_PROVISIONING_SUCCESS,
    AppEvent_PROVISIONING_STOPPED,
    AppEvent_PROVISIONING_WAIT_CONN,

    AppEvent_TIMEOUT,
    AppEvent_ERROR,
    AppEvent_RESTART,

    AppEvent_MAX,
}AppEvent;

/* Function pointer to the event handler */
typedef int32_t (*fptr_EventHandler)();

/* Entry in the lookup table */
typedef struct _Provisioning_TableEntry_t_
{
    fptr_EventHandler p_evtHndl;    /* Pointer to the event handler */
    AppState nextState;             /* Next state of the application */
}Provisioning_TableEntry_t;

/* LED State */
typedef enum
{
    LedState_CONNECTION = 0,
    LedState_FACTORY_DEFAULT,
    LedState_ERROR
}LedState;

/****************************************************************************
                      COMMON LOCAL FUNCTION PROTOTYPES
****************************************************************************/
int32_t ReportError();
int32_t HandleConnection();
int32_t HandleDiscnctEvt();
int32_t DoNothing();
int32_t HandleProvisioningComplete();
int32_t HandleProvisioningTimeOut();
int32_t CheckLanConnection();
int32_t CheckInternetConnection();
int32_t HandleUserApplication();
int32_t HandleWaitForIp();
int32_t ProcessRestartRequest();
int32_t ProvisioningStart();
int32_t SendPingToGW();
int32_t StartConnection();
int16_t SignalEvent(AppEvent event);
int32_t returnToFactoryDefault();
int32_t ProcessEvents();

/****************************************************************************
                      TRIGGER MODE VARIABLES/PROTOTYPES/DEFINES
****************************************************************************/

/* Trigger Mode Function Prototypes */
void triggerModeDoNothing();
int32_t TriggerModeTimerExpire();
int32_t EnterLowPowerMode(unsigned long long timerVal);
int32_t NotifyTriggerEvent();
int32_t RunSelectTraffic();
int32_t BsdTcpRxOpen();

/* Trigger Mode defines */
/* Max LPDS Time will be 5 minutes */
#define LPDS_MAX_TIME                           (300)   
/* 1.5 seconds without traffic will allow LPDS Mode */     
#define LPDS_NO_TRAFFIC_TIMER                   (1.5)          
#define MAX_DATA_BUFFER_SIZE                    (2500)

/* Trigger Mode Global Variables */
uint8_t gTimerStarted = FALSE;
int16_t gLocalSocketDescriptor;
uint8_t gTriggerModeEventArrived = FALSE;
UART_Handle gUartHandle;
uint8_t gNfds;
uint8_t gMyBuffer[MAX_DATA_BUFFER_SIZE] = {0};
int32_t gSockID;

/****************************************************************************
                      Provisioning GLOBAL VARIABLES
****************************************************************************/
uint8_t gRole = ROLE_STA;

uint8_t gWaitForConn = FALSE;
uint8_t gIsWlanConnected = FALSE;
uint8_t gStopInProgress = FALSE;
uint32_t gPingSent = FALSE;
uint32_t gPingSuccess = FALSE;

ClockP_Handle gAsyncEvtTimerHandle;
ClockP_Handle gDisplayHandle;
ClockP_Handle gLPDSHandle;
mqd_t gSMEventQueue;

uint16_t gLedCount = 0;
uint8_t gLedState = 0;
uint32_t gErrledCount = 0;
uint8_t gErrLedState = 0;
uint8_t gDisplayClockActive = 0;

LedState gLedDisplayState = LedState_CONNECTION;

const char *Roles[] = {"STA","STA","AP","P2P"};
const char *WlanStatus[] = {"DISCONNECTED","SCANING","CONNECTING","CONNECTED"};

/* Application lookup/transition table */
const Provisioning_TableEntry_t gTransitionTable[AppState_MAX][AppEvent_MAX] =
{
    /* AppState_STARTING */
    {
        /* Event: AppEvent_STARTED */ 
        {StartConnection, AppState_WAIT_FOR_CONNECTION       },
        /* Event: AppEvent_CONNECTED */
         {HandleWaitForIp, AppState_WAIT_FOR_IP               },
        /* Event: AppEvent_IP_ACQUIRED */
         {ReportError, AppState_ERROR                         },
        /* Event: AppEvent_DISCONNECT */ 
        {ReportError, AppState_ERROR                         },
        /* Event: AppEvent_PING_COMPLETE */ 
        {ReportError, AppState_ERROR                            },
        /* AppEvent_PROVISIONING_STARTED */
         {ReportError, AppState_ERROR                         },
        /* AppEvent_PROVISIONING_SUCCESS */ 
        {ReportError, AppState_ERROR                         },
        /* AppEvent_PROVISIONING_STOPPED */
         {ReportError, AppState_ERROR                         },
        /* AppEvent_PROVISIONING_WAIT_CONN */        
         {ReportError, AppState_ERROR                         },
        /* Event: AppEvent_TIMEOUT */ 
        {ReportError, AppState_ERROR                          },
        /* Event: AppEvent_ERROR */ 
        {ReportError, AppState_ERROR                         },
        /* Event: AppEvent_RESTART */ 
        {ProvisioningStart, AppState_PROVISIONING_IN_PROGRESS}
    },

    /* AppState_WAIT_FOR_CONNECTION */
    {
        /* Event: AppEvent_STARTED */ 
        {StartConnection,AppState_WAIT_FOR_CONNECTION          },
        /** Event: AppEvent_CONNECTED */ 
        {HandleWaitForIp, AppState_WAIT_FOR_IP                 },
        /* Event: AppEvent_IP_ACQUIRED */  
        {ReportError, AppState_ERROR                           },
        /* Event: AppEvent_DISCONNECT */
         {StartConnection,  AppState_WAIT_FOR_CONNECTION      },
        /* Event: AppEvent_PING_COMPLETE */
         {HandleProvisioningComplete,AppState_PINGING_GW      },
        /* AppEvent_PROVISIONING_STARTED */ 
        {ProvisioningStart,AppState_PROVISIONING_IN_PROGRESS   },
        /* AppEvent_PROVISIONING_SUCCESS */ 
        {DoNothing, AppState_WAIT_FOR_CONNECTION               },
  
        /* AppEvent_PROVISIONING_STOPPED */
         {CheckLanConnection, AppState_WAIT_FOR_CONNECTION    },
        /* AppEvent_PROVISIONING_WAIT_CONN */ 
        {DoNothing, AppState_WAIT_FOR_CONNECTION               },
        /* Event: AppEvent_TIMEOUT */
         {ProvisioningStart, AppState_PROVISIONING_IN_PROGRESS},
        /* Event: AppEvent_ERROR */ 
        {ProvisioningStart, AppState_PROVISIONING_IN_PROGRESS  },
        /* Event: AppEvent_RESTART */ 
        {ProcessRestartRequest,AppState_WAIT_FOR_CONNECTION    }
    },

    /* AppState_WAIT_FOR_IP */
    {
        /* Event: AppEvent_STARTED */ 
        {ProvisioningStart, AppState_PROVISIONING_IN_PROGRESS  },
        /* Event: AppEvent_CONNECTED */ 
        {ReportError, AppState_ERROR                          },
        /* Event: AppEvent_IP_ACQUIRED */  
        {CheckLanConnection, AppState_PINGING_GW               },
        
        /* Event: AppEvent_DISCONNECT */ 
        {StartConnection, AppState_WAIT_FOR_CONNECTION        },
        /* Event: AppEvent_PING_COMPLETE */ 
        {DoNothing, AppState_WAIT_FOR_IP                       },
        /* AppEvent_PROVISIONING_STARTED */ 
        {ProvisioningStart, AppState_PROVISIONING_IN_PROGRESS  },
        /* AppEvent_PROVISIONING_SUCCESS */
         {DoNothing, AppState_WAIT_FOR_IP                     },
        /* AppEvent_PROVISIONING_STOPPED */ 
        {HandleProvisioningComplete, AppState_PINGING_GW       },
        /* AppEvent_PROVISIONING_WAIT_CONN */ 
        {DoNothing, AppState_WAIT_FOR_IP                       },
        /* Event: AppEvent_TIMEOUT */
         {ReportError, AppState_ERROR                         },
        /* Event: AppEvent_ERROR */ 
        {ReportError, AppState_ERROR                          },
        /* Event: AppEvent_RESTART */ 
        {ProcessRestartRequest, AppState_WAIT_FOR_CONNECTION   }
    },

    /* AppState_PINGING_GW */
    {
        /* Event: AppEvent_STARTED */ 
        {DoNothing, AppState_PINGING_GW                          },
        /* Event: AppEvent_CONNECTED */ 
        {HandleWaitForIp, AppState_WAIT_FOR_IP                   },
        /* Event: AppEvent_IP_ACQUIRED */ 
        {CheckInternetConnection, AppState_PINGING_GW            },
        /* Event: AppEvent_DISCONNECT */ 
        {HandleDiscnctEvt, AppState_WAIT_FOR_CONNECTION          },
        /* Event: AppEvent_PING_COMPLETE */ 
        {DoNothing, AppState_PINGING_GW                          },
        /* AppEvent_PROVISIONING_STARTED */ 
        {ProvisioningStart, AppState_PROVISIONING_IN_PROGRESS  },
        /* AppEvent_PROVISIONING_SUCCESS */
         {DoNothing, AppState_PINGING_GW                       },
        /* AppEvent_PROVISIONING_STOPPED */ 
        {HandleUserApplication, AppState_PINGING_GW              },
        /* AppEvent_PROVISIONING_WAIT_CONN */
         {DoNothing, AppState_PINGING_GW                       },
        /*  Event: AppEvent_TIMEOUT */ 
    // Send a ping to check connection, and transition to trigger mode
        {SendPingToGW, AppState_TRIGGER_MODE                   },
        /* Event: AppEvent_ERROR */ 
        {ReportError, AppState_ERROR                           },
        /* Event: AppEvent_RESTART */ 
        {ProcessRestartRequest, AppState_WAIT_FOR_CONNECTION     }
    },
    /* AppState_PROVISIONING_IN_PROGRESS  */
    {
        /* Event: AppEvent_STARTED */ 
        {DoNothing, AppState_PROVISIONING_IN_PROGRESS          },
        /* Event: AppEvent_CONNECTED */ 
        {HandleConnection, AppState_PROVISIONING_IN_PROGRESS      },
        /* Event: AppEvent_IP_ACQUIRED */ 
        {CheckInternetConnection, AppState_PROVISIONING_IN_PROGRESS },
        /* Event: AppEvent_DISCONNECT */ 
        {HandleDiscnctEvt, AppState_PROVISIONING_IN_PROGRESS      },
        /* Event: AppEvent_PING_COMPLETE */
         {DoNothing, AppState_PROVISIONING_IN_PROGRESS         },
        /* AppEvent_PROVISIONING_STARTED */ 
        {DoNothing, AppState_PROVISIONING_IN_PROGRESS          },
        /* AppEvent_PROVISIONING_SUCCESS */ 
        {HandleProvisioningComplete, AppState_PROVISIONING_WAIT_COMPLETE},
        /* AppEvent_PROVISIONING_STOPPED */ 
        {HandleProvisioningComplete, AppState_PINGING_GW          },
        /* AppEvent_PROVISIONING_WAIT_CONN */ 
        {DoNothing, AppState_WAIT_FOR_CONNECTION                },
        /* Event: AppEvent_TIMEOUT */ 
        {ProcessRestartRequest, AppState_WAIT_FOR_CONNECTION      },
        /* Event: AppEvent_ERROR */ 
        {ReportError, AppState_ERROR                            },
        /* Event: AppEvent_RESTART */ 
        {ProcessRestartRequest, AppState_WAIT_FOR_CONNECTION      }
    },
    /* AppState_PROVISIONING_WAIT_COMPLETE */
    {
        /* Event: AppEvent_STARTED */ 
        {DoNothing, AppState_PROVISIONING_WAIT_COMPLETE            },
        /* Event: AppEvent_CONNECTED */ 
        {DoNothing, AppState_PROVISIONING_WAIT_COMPLETE            },
        /* Event: AppEvent_IP_ACQUIRED */ 
        {DoNothing, AppState_PROVISIONING_WAIT_COMPLETE            },
        /* Event: AppEvent_DISCONNECT */ 
        {HandleDiscnctEvt,AppState_PROVISIONING_WAIT_COMPLETE   },
        /* Event: AppEvent_PING_COMPLETE */ 
        {DoNothing,AppState_PROVISIONING_WAIT_COMPLETE          },
        /* AppEvent_PROVISIONING_STARTED */ 
        {DoNothing,AppState_PROVISIONING_WAIT_COMPLETE          },
        /* AppEvent_PROVISIONING_SUCCESS */ 
        {DoNothing,AppState_PROVISIONING_WAIT_COMPLETE          },
        /* AppEvent_PROVISIONING_STOPPED */ 
        {HandleUserApplication,AppState_PINGING_GW              },
        /* AppEvent_PROVISIONING_WAIT_CONN */ 
        {DoNothing,AppState_WAIT_FOR_CONNECTION                 },
        /* Event: AppEvent_TIMEOUT */ 
        {HandleProvisioningTimeOut,AppState_PROVISIONING_WAIT_COMPLETE  },
        /* Event: AppEvent_ERROR */ 
        {ReportError, AppState_ERROR                            },
        /* Event: AppEvent_RESTART */
         {ProcessRestartRequest,AppState_PROVISIONING_WAIT_COMPLETE    }
    },

    /* AppState_TRIGGER_MODE */
    {
        /* Event: APP_EVENT_STARTED */ 
        {DoNothing, AppState_TRIGGER_MODE                    },
        /* Event: APP_EVENT_CONNECTED */
         {DoNothing, AppState_TRIGGER_MODE                     },
        /* Event: APP_EVENT_IP_ACQUIRED */ 
        {DoNothing, AppState_TRIGGER_MODE                    },
        /* Event: APP_EVENT_DISCONNECT */
         {HandleDiscnctEvt,AppState_WAIT_FOR_CONNECTION        },
        /* Event: APP_EVENT_PING_COMPLETE */ 
        {BsdTcpRxOpen,AppState_TRIGGER_MODE                  },
        /* APP_EVENT_PROVISIONING_STARTED */ 
        {DoNothing, AppState_TRIGGER_MODE                    },
        /* APP_EVENT_PROVISIONING_SUCCESS */
         {DoNothing, AppState_TRIGGER_MODE                     },
        /* APP_EVENT_PROVISIONING_STOPPED */ 
        {DoNothing, AppState_TRIGGER_MODE                    },
        /* APP_EVENT_TRIGGER_RECEIVE */ 
         {DoNothing, AppState_TRIGGER_MODE                     },
        /* Event: APP_EVENT_TIMEOUT */ 
        {TriggerModeTimerExpire,AppState_TRIGGER_MODE        },
        /* Event: APP_EVENT_ERROR */
         {ReportError, AppState_ERROR                          },
        /* Event: APP_EVENT_RESTART */
         {DoNothing, AppState_TRIGGER_MODE                     }
    },

    /* AppState_ERROR */
    /* we will restart connection for all errors */
    {
        /* Event: AppEvent_STARTED */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* Event: AppEvent_CONNECTED */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* Event: AppEvent_IP_ACQUIRED */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* Event: AppEvent_DISCONNECT */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* Event: AppEvent_PING_COMPLETE */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* AppEvent_PROVISIONING_STARTED */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* AppEvent_PROVISIONING_SUCCESS */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* AppEvent_PROVISIONING_STOPPED */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* AppEvent_PROVISIONING_WAIT_CONN */
         {ReportError,AppState_WAIT_FOR_CONNECTION    },
        /* Event: AppEvent_TIMEOUT */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     },
        /* Event: AppEvent_ERROR */ 
        {ReportError, AppState_WAIT_FOR_CONNECTION    },
        /* Event: AppEvent_RESTART */ 
        {ReportError,AppState_WAIT_FOR_CONNECTION     }
    }
};

/* Application state's context */

AppState g_CurrentState;

/*****************************************************************************
                  Trigger Mode Functions
*****************************************************************************/

/*!
 *  \brief      do nothing
 *  \param      None
 *  \return     None
 */
void triggerModeDoNothing()
{
    // Do nothing .....
}

/*!
 *  \brief      No Traffic Timer expire - we can enter LPDS
 *  \param      None
 *  \return     None
 */
int32_t TriggerModeTimerExpire()
{
    SlFdSet_t rxSet;
    SlTimeval_t timeVal = {0};

    /* Clear timer flag */
    gTimerStarted = FALSE;

    /* Make Sure Select is set */
    SL_SOCKET_FD_ZERO(&rxSet);
    SL_SOCKET_FD_SET(gLocalSocketDescriptor, &rxSet);

    /* Call select function with relevant parameters */
    if(sl_Select(gNfds, &rxSet, NULL, NULL, &timeVal) > 0)
    {
        LOG_MESSAGE("Data waiting, disable low power\r\n");
        return(0);
    }

    LOG_MESSAGE("\r\nEnter LP Mode\r\n");

    /* Enter Low Power Mode */
    EnterLowPowerMode(1000 * LPDS_MAX_TIME);

    /* On Wake Up return to current PC */
    LOG_MESSAGE("\r\nAwake from Low Power Mode \r\n");

    /* Clear pending events */
    ProcessEvents();

    return(0);
}

/*!
 *  \brief      Enter low power mode
 *  \param      For how long
 *  \return     None
 */
int32_t EnterLowPowerMode(unsigned long long timerVal)
{
    ClockP_setTimeout(gLPDSHandle,timerVal);
    ClockP_start(gLPDSHandle);

    /* removing dependency from the UART RX */
    UART_control(gUartHandle, UART_CMD_RXDISABLE, NULL);

    Power_idleFunc();

    /* removing dependency from the UART RX */
    UART_control(gUartHandle, UART_CMD_RXENABLE, NULL);

    return(0);
}

/*!
 *  \brief      Main Select/Trigger loop
 *              Run select command (configure the "trigger") 
 *              and receive the data
 *              If no data, open timer to check if we can go to LPDS
 *  \param      None
 *  \return     None
 */
int32_t RunSelectTraffic()
{
    SlFdSet_t rxSet;
    SlTimeval_t timeVal = {0};
    int32_t LoopCount = 0;
    int status;
    uint16_t tmpCounter;

    gTimerStarted = FALSE;

    /* This tasks waits for incoming data forever */
    /* Main task for the trigger mode application */
    while(1)
    {
        SL_SOCKET_FD_ZERO(&rxSet);
        SL_SOCKET_FD_SET(gLocalSocketDescriptor, &rxSet);
        tmpCounter = 0;

        do
        {
            /* Call select function with relevant parameters */
            status = sl_Select(gNfds, &rxSet, NULL, NULL, &timeVal);

            if(status == TRUE)
            {
                tmpCounter++;
            }
            else
            {
                tmpCounter = 0;
            }

            /* If no data available and no timer already set, set the timer */
            if(gTimerStarted == FALSE)
            {
                gTimerStarted = TRUE;
                ClockP_setTimeout(gAsyncEvtTimerHandle,
                                  1000 * LPDS_NO_TRAFFIC_TIMER);
                ClockP_start(gAsyncEvtTimerHandle);
            }

            /* ProcessEvents will call sl_Task() to
                                            get the events (Trigger/Timer) */
            ProcessEvents();

            /* If Trigger Event arrived (or if data is waiting but no event)
             * Go to receive data */
            if((gTriggerModeEventArrived == TRUE) || (tmpCounter > 10))
            {
                /* Clear Event Indication */
                gTriggerModeEventArrived = FALSE;
                tmpCounter = 0;
                break;
            }
        }
        while(1);       /* Only reception of the event will
                                    take us out of the loop */

        /* Data received, Stop Timer */
        gTimerStarted = FALSE;
        ClockP_stop(gAsyncEvtTimerHandle);

        /* Double check data await */
        if(SL_SOCKET_FD_ISSET(gLocalSocketDescriptor, &rxSet))
        {
            /* The socket_fd has data available to be read */
            status =
              sl_Recv(gLocalSocketDescriptor, gMyBuffer, sizeof(gMyBuffer), 0);

            /* No data - socket was closed */
            if(status <= 0)
            {
                SlSockAddrIn_t Addr;
                int AddrSize = sizeof(SlSockAddrIn_t);

                /* This means the other side closed the socket */
                LOG_MESSAGE(
                    "Socket Closed by remote peer,"
                    " wait for new TCP connection\n\r");

                do
                {
                    gLocalSocketDescriptor =
                        sl_Accept(gSockID, (SlSockAddr_t*)&Addr,
                                  (SlSocklen_t*) &AddrSize);
                    ProcessEvents();
                }
                while((gLocalSocketDescriptor == SL_ERROR_BSD_EAGAIN) &&
                      (gIsWlanConnected));

                if(!gIsWlanConnected)
                {
                    sl_Close(gSockID);
                    LOG_MESSAGE(
                        "Accept failed, closing socket and restarting\r\n");
                    return(0);
                }

                gNfds = gLocalSocketDescriptor + 1;

                LOG_MESSAGE("TCP Connection accepted..\r\n");
            }
            else
            {
                /* Print received data statistics */
                LOG_MESSAGE( "[%d] Data received: %d Bytes\n\r",
                 ++LoopCount,
                             status                  );

                /* We leave this part to your own implementation */
            }
        }
    }
}

/*!
 *  \brief      Open TCP Socket
 *  \param      None
 *  \return     None
 */
int32_t BsdTcpRxOpen()
{
    SlSockAddrIn_t Addr;
    SlSockAddrIn_t LocalAddr;
    SlSockNonblocking_t enableOption;
    int AddrSize;
    int SockID;
    int sd;
    int32_t status;

    LocalAddr.sin_family = SL_AF_INET;
    LocalAddr.sin_port = sl_Htons(5001);
    LocalAddr.sin_addr.s_addr = 0;

    AddrSize = sizeof(SlSockAddrIn_t);

    enableOption.NonBlockingEnabled = 1;

    if((SockID = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, 0)) < 0)
    {
        LOG_MESSAGE("sl_Socket failed: ");
        ASSERT_ON_ERROR((int32_t)SockID);
        return(0);
    }

    if(
        (status =
            sl_SetSockOpt(    SockID, 
                    SL_SOL_SOCKET, 
                    SL_SO_NONBLOCKING,
                              (_u8 *)&enableOption,
                              sizeof(enableOption)))  
                              < 0)
    {
        LOG_MESSAGE("sl_SetSockOpt failed: ");
        ASSERT_ON_ERROR((int32_t)status);
        return(0);
    }

    if((status = sl_Bind(SockID, (SlSockAddr_t *)&LocalAddr, AddrSize)) < 0)
    {
        LOG_MESSAGE("sl_Bind failed: ");
        ASSERT_ON_ERROR((int32_t)status);
        return(0);
    }

    if((status = sl_Listen(SockID, 0)) < 0)
    {
        LOG_MESSAGE("sl_Listen failed: ");
        ASSERT_ON_ERROR((int32_t)status);
        return(0);
    }

    LOG_MESSAGE("Waiting for TCP Accept\r\n");

    do
    {
        sd = sl_Accept(SockID, (SlSockAddr_t*)&Addr, (SlSocklen_t*) &AddrSize);
        ProcessEvents();
    }
    while((sd == SL_ERROR_BSD_EAGAIN) && (gIsWlanConnected));

    if(!gIsWlanConnected)
    {
        sl_Close(SockID);
        LOG_MESSAGE("Accept failed, closing socket and restarting\r\n");
        return(0);
    }

    // First select
    gNfds = sd + 1;

    LOG_MESSAGE("TCP Connection accepted..\r\n");

    gLocalSocketDescriptor = sd;

    gSockID = SockID;

    /* Start Main Trigger mode Application task */
    RunSelectTraffic();

    return(0);
}

/*****************************************************************************
                  SimpleLink Callback Functions
*****************************************************************************/

//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************

void SimpleLinkSocketTriggerEventHandler(SlSockTriggerEvent_t *pSlTriggerEvent)
{
    switch(pSlTriggerEvent->Event)
    {
    case SL_SOCKET_TRIGGER_EVENT_SELECT:
    {
        //LOG_MESSAGE("Notify User SL_SOCKET_TRIGGER_EVENT_SELECT \n\r");
        gTriggerModeEventArrived = TRUE;
        break;
    }

    default:
        break;
    }
}

void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    switch(pWlanEvent->Id)
    {
    case SL_WLAN_EVENT_CONNECT:
        LOG_MESSAGE(
            " [Event] STA connected to AP"
            " - BSSID:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x, ",
            pWlanEvent->Data.Connect.Bssid[0],
            pWlanEvent->Data.Connect.Bssid[1],
            pWlanEvent->Data.Connect.Bssid[2],
            pWlanEvent->Data.Connect.Bssid[3],
            pWlanEvent->Data.Connect.Bssid[4],
            pWlanEvent->Data.Connect.Bssid[5]);

        /* set the string terminate */
        pWlanEvent->Data.Connect.SsidName[pWlanEvent->Data.Connect.SsidLen] =
            '\0';

        LOG_MESSAGE("SSID:%s\r\n", pWlanEvent->Data.Connect.SsidName);
        SignalEvent(AppEvent_CONNECTED);
        break;

    case SL_WLAN_EVENT_DISCONNECT:
        LOG_MESSAGE(" [Event] STA disconnected from AP (Reason Code = %d)\r\n",
                    pWlanEvent->Data.Disconnect.ReasonCode);
        SignalEvent(AppEvent_DISCONNECT);
        break;

    case SL_WLAN_EVENT_STA_ADDED:
        LOG_MESSAGE(
            " [Event] New STA Addeed (MAC Address:"
            " %.2x:%.2x:%.2x:%.2x:%.2x)\r\n",
            pWlanEvent->Data.STAAdded.Mac[0],
            pWlanEvent->Data.STAAdded.Mac[1],
            pWlanEvent->Data.STAAdded.Mac[2],
            pWlanEvent->Data.STAAdded.Mac[3],
            pWlanEvent->Data.STAAdded.Mac[4],
            pWlanEvent->Data.STAAdded.Mac[5]);
        break;

    case SL_WLAN_EVENT_STA_REMOVED:
        LOG_MESSAGE(
            " [Event] STA Removed (MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x)\r\n",
            pWlanEvent->Data.STAAdded.Mac[0],
            pWlanEvent->Data.STAAdded.Mac[1],
            pWlanEvent->Data.STAAdded.Mac[2],
            pWlanEvent->Data.STAAdded.Mac[3],
            pWlanEvent->Data.STAAdded.Mac[4],
            pWlanEvent->Data.STAAdded.Mac[5]);
        break;

    case SL_WLAN_EVENT_PROVISIONING_PROFILE_ADDED:
        LOG_MESSAGE(" [Provisioning] Profile Added: SSID: %s\r\n",
                    pWlanEvent->Data.ProvisioningProfileAdded.Ssid);
        if(pWlanEvent->Data.ProvisioningProfileAdded.ReservedLen > 0)
        {
            LOG_MESSAGE(" [Provisioning] Profile Added: PrivateToken:%s\r\n",
                        pWlanEvent->Data.ProvisioningProfileAdded.Reserved);
        }
        break;

    case SL_WLAN_EVENT_PROVISIONING_STATUS:
    {
        switch(pWlanEvent->Data.ProvisioningStatus.ProvisioningStatus)
        {
        case SL_WLAN_PROVISIONING_GENERAL_ERROR:
        case SL_WLAN_PROVISIONING_ERROR_ABORT:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_INVALID_PARAM:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_HTTP_SERVER_DISABLED:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_PROFILE_LIST_FULL:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_PROVISIONING_ALREADY_STARTED:
            LOG_MESSAGE(" [Provisioning] Provisioning Error status=%d\r\n",
                        pWlanEvent->Data.ProvisioningStatus.ProvisioningStatus);
            SignalEvent(AppEvent_ERROR);
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_FAIL_NETWORK_NOT_FOUND:
            LOG_MESSAGE(
                " [Provisioning] Profile confirmation failed:"
                " network not found\r\n");
            SignalEvent(AppEvent_PROVISIONING_STARTED);
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_FAIL_CONNECTION_FAILED:
            LOG_MESSAGE(
                " [Provisioning] Profile confirmation failed:"
                " Connection failed\r\n");
            SignalEvent(AppEvent_PROVISIONING_STARTED);
            break;

        case
            SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_CONNECTION_SUCCESS_IP_NOT_ACQUIRED
            :
            LOG_MESSAGE(
                " [Provisioning] Profile confirmation failed:"
                " IP address not acquired\r\n");
            SignalEvent(AppEvent_PROVISIONING_STARTED);
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_SUCCESS_FEEDBACK_FAILED:
            LOG_MESSAGE(
                " [Provisioning] Profile Confirmation failed "
                "(Connection Success, feedback to Smartphone app failed)\r\n");
            SignalEvent(AppEvent_PROVISIONING_STARTED);
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_SUCCESS:
            LOG_MESSAGE(" [Provisioning] Profile Confirmation Success!\r\n");
            SignalEvent(AppEvent_PROVISIONING_SUCCESS);
            break;

        case SL_WLAN_PROVISIONING_AUTO_STARTED:
            LOG_MESSAGE(" [Provisioning] Auto-Provisioning Started\r\n");
            SignalEvent(AppEvent_RESTART);
            break;

        case SL_WLAN_PROVISIONING_STOPPED:
            LOG_MESSAGE("\r\n Provisioning stopped:");
            LOG_MESSAGE(" Current Role: %s\r\n",
                        Roles[pWlanEvent->Data.ProvisioningStatus.Role]);
            if(ROLE_STA == pWlanEvent->Data.ProvisioningStatus.Role)
            {
                LOG_MESSAGE("WLAN Status: %s\r\n",
                            WlanStatus[pWlanEvent->Data.ProvisioningStatus.
                                       WlanStatus]);

                if(SL_WLAN_STATUS_CONNECTED ==
                   pWlanEvent->Data.ProvisioningStatus.WlanStatus)
                {
                    LOG_MESSAGE("Connected to SSID: %s\r\n",
                                pWlanEvent->Data.ProvisioningStatus.Ssid);
                    SignalEvent(AppEvent_PROVISIONING_STOPPED);
                }
                else if(SL_WLAN_STATUS_SCANING ==
                        pWlanEvent->Data.ProvisioningStatus.WlanStatus)
                {
                    gWaitForConn = 1;
                    SignalEvent(AppEvent_PROVISIONING_WAIT_CONN);
                }
            }
            else
            {
                SignalEvent(AppEvent_RESTART);
            }
            break;

        case SL_WLAN_PROVISIONING_SMART_CONFIG_SYNCED:
            LOG_MESSAGE(" [Provisioning] Smart Config Synced!\r\n");
            break;

        case SL_WLAN_PROVISIONING_SMART_CONFIG_SYNC_TIMEOUT:
            LOG_MESSAGE(" [Provisioning] Smart Config Sync Timeout!\r\n");
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_WLAN_CONNECT:
            LOG_MESSAGE(
                " [Provisioning] Profile confirmation: WLAN Connected!\r\n");
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_IP_ACQUIRED:
            LOG_MESSAGE(
                " [Provisioning] Profile confirmation: IP Acquired!\r\n");
            break;

        case SL_WLAN_PROVISIONING_EXTERNAL_CONFIGURATION_READY:
            LOG_MESSAGE(" [Provisioning] External configuration is ready! \r\n");
            break;

        default:
            LOG_MESSAGE(" [Provisioning] Unknown Provisioning Status: %d\r\n",
                        pWlanEvent->Data.ProvisioningStatus.ProvisioningStatus);
            break;
        }
    }
    break;

    default:
        LOG_MESSAGE(" [Event] - WlanEventHandler has received %d !!!!\r\n",
                    pWlanEvent->Id);
        break;
    }
}

//*****************************************************************************
//
//! \brief The Function Handles the Fatal errors
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
        LOG_MESSAGE("[ERROR] - FATAL ERROR: Abort NWP event detected: "
                    "AbortType=%d, AbortData=0x%x\n\r",
                    slFatalErrorEvent->Data.DeviceAssert.Code,
                    slFatalErrorEvent->Data.DeviceAssert.Value);
    }
    break;

    case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
    {
        LOG_MESSAGE("[ERROR] - FATAL ERROR: Driver Abort detected. \n\r");
    }
    break;

    case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
    {
        LOG_MESSAGE("[ERROR] - FATAL ERROR: No Cmd Ack detected "
                    "[cmd opcode = 0x%x] \n\r",
                    slFatalErrorEvent->Data.NoCmdAck.Code);
    }
    break;

    case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
    {
        LOG_MESSAGE("[ERROR] - FATAL ERROR: Sync loss detected n\r");
    }
    break;

    case SL_DEVICE_EVENT_FATAL_CMD_TIMEOUT:
    {
        LOG_MESSAGE("[ERROR] - FATAL ERROR: Async event timeout detected "
                    "[event opcode =0x%x]  \n\r",
                    slFatalErrorEvent->Data.CmdTimeout.Code);
    }
    break;

    default:
        LOG_MESSAGE("[ERROR] - FATAL ERROR: Unspecified error detected \n\r");
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
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
        LOG_MESSAGE("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                    "Gateway=%d.%d.%d.%d\n\r",
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,3),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,2),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,1),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip,0),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,3),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,2),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,1),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway,0));
        SignalEvent(AppEvent_IP_ACQUIRED);
        break;

    case SL_NETAPP_EVENT_DHCPV4_LEASED:

        LOG_MESSAGE("[NETAPP Event] IP Leased: %d.%d.%d.%d\r\n",
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress,3),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress,2),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress,1),
                    SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress,0));
        SignalEvent(AppEvent_IP_ACQUIRED);
        break;

    case SL_NETAPP_EVENT_DHCP_IPV4_ACQUIRE_TIMEOUT:
        LOG_MESSAGE("[NETAPP Event] IP acquired timeout \r\n");
        break;

    default:
        LOG_MESSAGE("[NETAPP EVENT] Unhandled event [0x%x] \n\r",
                    pNetAppEvent->Id);
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent    - Contains the relevant event information
//! \param[in]  pServerResponse - Should be filled by the user with the
//!                               relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerEventHandler(
    SlNetAppHttpServerEvent_t *pHttpEvent,
    SlNetAppHttpServerResponse_t *
    pHttpResponse)
{
    /* Unused in this application */
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]  pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /* Most of the general errors are not FATAL are are to be handled
       appropriately by the application */
    LOG_MESSAGE("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
                pDevEvent->Data.Error.Code,
                pDevEvent->Data.Error.Source);
}

//*****************************************************************************
//
//! \brief This function handles socket events indication
//!
//! \param[in]  pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    /* This application doesn't work with socket - Events are not expected */
    switch(pSock->Event)
    {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch(pSock->SocketAsyncEvent.SockTxFailData.Status)
        {
        case SL_ERROR_BSD_ECLOSE:
            LOG_MESSAGE("[SOCK ERROR] - close socket (%d) operation "
                        "failed to transmit all queued packets\n\r",
                        pSock->SocketAsyncEvent.SockTxFailData.Sd);
            break;
        default:
            LOG_MESSAGE("[SOCK ERROR] - TX FAILED  :  socket %d , "
                        "reason (%d) \n\n",
                        pSock->SocketAsyncEvent.SockTxFailData.Sd,
                        pSock->SocketAsyncEvent.SockTxFailData.Status);
            break;
        }
        break;

    default:
        LOG_MESSAGE("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
        break;
    }
}

void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest,
                                         SlNetAppResponse_t *pNetAppResponse)
{
    /* Unused in this application */
}

void SimpleLinkNetAppRequestMemFreeEventHandler(_u8 *buffer)
{
    /* Unused in this application */
}

//*****************************************************************************
//                 Local Functions
//*****************************************************************************

//*****************************************************************************
//
//! \brief Return to Factory defaults indication Led
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************

void FactoryIndicationLed(void)
{
    GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
    gLedState = 0;

    while(1)
    {
        gLedCount++;
        gLedCount &= 0x1FFFF;

        if(0 == gLedCount)
        {
            if(gLedState)
            {
                GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
                gLedState = 0;
            }
            else
            {
                GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON);
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
                gLedState = 1;
            }
        }
    }
}

//*****************************************************************************
//
//! \brief Error indication Led
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void ErrorLedDisplay(void)
{
    GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    gErrLedState = 0;

    while(1)
    {
        gErrledCount++;
        gErrledCount &= 0xFFFFF;

        if(0 == gErrledCount)
        {
            if(gErrLedState)
            {
                GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_OFF);
                gErrLedState = 0;
            }
            else
            {
                GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_ON);
                gErrLedState = 1;
            }
        }
    }
}

//*****************************************************************************
//
//! \brief Update status for indication Led
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************

void UpdateLedDisplay()
{
    gDisplayClockActive = FALSE;

    if(LedState_ERROR == gLedDisplayState)
    {
        ErrorLedDisplay();
    }
    else if(LedState_FACTORY_DEFAULT == gLedDisplayState)
    {
        FactoryIndicationLed();
    }
    else
    {
        if(gIsWlanConnected) /* connected (led ON) */
        {
            if(0 == gLedState)
            {
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
                gLedState = 1;
            }
        }
        else /* disconnected (blink led) */
        {
            if(gLedState)
            {
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
                gLedState = 0;
            }
            else
            {
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
                gLedState = 1;
            }
        }
    }
}

//*****************************************************************************
//
//! \brief The interrupt handler for the async-evt timer
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void AsyncEvtTimerHandler()
{
    SignalEvent(AppEvent_TIMEOUT);
}

//*****************************************************************************
//
//! \brief Error Handler - Print and restart connection
//!
//! \param  None
//!
//! \return none
//!
//*****************************************************************************
int32_t ReportError()
{
    LOG_MESSAGE(
        "[Trigger Mode] Trigger Mode Application Error - Restarting \r\n ");
    gIsWlanConnected = 0;
    sl_WlanProvisioning(SL_WLAN_PROVISIONING_CMD_STOP,ROLE_AP, 0, NULL,
                        (uint32_t)NULL);
    ClockP_setTimeout(gAsyncEvtTimerHandle,
    1000 * CONNECTION_PHASE_TIMEOUT_SEC);
    ClockP_start(gAsyncEvtTimerHandle);

    return(0);
}

//*****************************************************************************
//
//! \brief Restart request - disconnected and open timer to re-open 
//!                           provisioning for non connection
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t ProcessRestartRequest()
{
    gIsWlanConnected = 0;

    ClockP_setTimeout(gAsyncEvtTimerHandle,
    1000 * CONNECTION_PHASE_TIMEOUT_SEC);
    ClockP_start(gAsyncEvtTimerHandle);

    return(0);
}

//*****************************************************************************
//
//! \brief Get AP security parameters
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t GetSecureStatus(void)
{
    uint8_t sec_type;
    uint16_t len = 1;
    uint16_t config_opt = SL_WLAN_AP_OPT_SECURITY_TYPE;
    sl_WlanGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (_u8* )&sec_type);
    if(sec_type == AP_SEC_TYPE)
    {
        return(1);
    }
    return(0);
}

//*****************************************************************************
//
//! \brief Set AP security parameters
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t SetSecuredAP(const uint8_t sec_en)
{
    uint8_t val = AP_SEC_TYPE;
    uint8_t open = SL_WLAN_SEC_TYPE_OPEN;
    uint8_t password[65];
    uint16_t len = strlen((char *)AP_SEC_PASSWORD);
    if(sec_en)
    {
        sl_WlanSet(SL_WLAN_CFG_AP_ID, SL_WLAN_AP_OPT_SECURITY_TYPE, 1,
                   (uint8_t *)&val);
        memset(password, 0, sizeof(password));
        memcpy(password, (char *)AP_SEC_PASSWORD, len);
        sl_WlanSet(SL_WLAN_CFG_AP_ID, SL_WLAN_AP_OPT_PASSWORD, len,
                   (uint8_t *)password);
        LOG_MESSAGE(" Setting AP secured parameters\n\r");
    }
    else
    {
        sl_WlanSet(SL_WLAN_CFG_AP_ID, SL_WLAN_AP_OPT_SECURITY_TYPE, 1,
                   (uint8_t *)&open);
    }
    return(0);
}

//*****************************************************************************
//
//! \brief  Start connection - wait for connection
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t StartConnection()
{
    gIsWlanConnected = 0;
    ClockP_setTimeout(gAsyncEvtTimerHandle,
    1000 * CONNECTION_PHASE_TIMEOUT_SEC);
    ClockP_start(gAsyncEvtTimerHandle);

    return(0);
}

//*****************************************************************************
//
//! \brief  Connection event arrived, open timer for IP ACQUIRED
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t HandleConnection()
{
    gIsWlanConnected = 1;

    return(0);
}

//*****************************************************************************
//
//! \brief  Provisioning completed successfully, connection established
//!         Open timer to make sure application negotiation completed as well
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t HandleProvisioningComplete()
{
    gStopInProgress = 0;
    gIsWlanConnected = 1;
    LOG_MESSAGE(
        "[Provisioning] Provisioning Application Ended Successfully \r\n ");
    ClockP_setTimeout(gAsyncEvtTimerHandle,
                      1000 * RECONNECTION_ESTABLISHED_TIMEOUT_SEC);
    ClockP_start(gAsyncEvtTimerHandle);

    return(0);
}

//*****************************************************************************
//
//! \brief  Application side function
//!         Ping to Gateway and open timer for next ping
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t SendPingToGW()
{
    /* Get IP and Gateway information */
    uint16_t len = sizeof(SlNetCfgIpV4Args_t);
    uint16_t ConfigOpt = 0;   /* return value could be one of the following:
    SL_NETCFG_ADDR_DHCP / SL_NETCFG_ADDR_DHCP_LLA / SL_NETCFG_ADDR_STATIC  */
    SlNetCfgIpV4Args_t ipV4 = {0};

    SlNetAppPingReport_t report;
    SlNetAppPingCommand_t pingCommand;

    sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE,&ConfigOpt,&len,(_u8 *)&ipV4);

    /* destination IP of gateway                */
    pingCommand.Ip = ipV4.IpGateway;        

     /* size of ping, in bytes                   */
    pingCommand.PingSize = 150;          

    /* delay between pings, in milliseconds     */    
    pingCommand.PingIntervalTime = 100;      

    /* timeout for every ping in milliseconds   */    
    pingCommand.PingRequestTimeout = 1000;   

    /* max number of ping requests. 0 - forever */    
    pingCommand.TotalNumberOfAttempts = 1;    
    /* report only when finished                */    
    pingCommand.Flags = 0;                          

    /* Ping Gateway */
    sl_NetAppPing(&pingCommand, SL_AF_INET, &report, NULL);

    /* Set Over all ping Statistics */
    gPingSent++;
    if(report.PacketsSent == report.PacketsReceived)
    {
        gPingSuccess++;
    }
    LOG_MESSAGE(
        "Reply from %d.%d.%d.%d: %s, Time=%dms\n\rOverall"
        " Stat Success (%d/%d)\r\n",
        SL_IPV4_BYTE(ipV4.IpGateway,3),SL_IPV4_BYTE(ipV4.IpGateway,
                                                    2),
        SL_IPV4_BYTE(ipV4.IpGateway,1),SL_IPV4_BYTE(ipV4.IpGateway,
                                                    0),
        (report.PacketsSent ==
         report.PacketsReceived) ? " SUCCESS" : " FAIL",
        (report.PacketsSent ==
         report.PacketsReceived) ? report.MinRoundTime : 0,
        gPingSuccess, gPingSent);

    if(gPingSuccess)
    {
        SignalEvent(AppEvent_PING_COMPLETE);
    }
    else
    {
        SignalEvent(AppEvent_RESTART);
    }

    return(0);
}

//*****************************************************************************
//
//! \brief  Application side function
//!          Print AP parameters, and open timer to start real application
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t HandleUserApplication()
{
    LOG_MESSAGE("[App] User Application Started \r\n ");
    gIsWlanConnected = 1;
    gWaitForConn = 0;
    /* as long as the device connected, run user application */

    /* Get IP and Gateway information */
    uint16_t len = sizeof(SlNetCfgIpV4Args_t);
    uint16_t ConfigOpt = 0;   /* return value could be one of the following: 
    SL_NETCFG_ADDR_DHCP / SL_NETCFG_ADDR_DHCP_LLA / SL_NETCFG_ADDR_STATIC  */
    SlNetCfgIpV4Args_t ipV4 = {0};

    sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE,&ConfigOpt,&len,(_u8 *)&ipV4);

    LOG_MESSAGE(
        "\rDHCP is %s \r\n\rIP \r%d.%d.%d.%d \r\n\rMASK \r%d.%d.%d.%d \r\n\rGW"
        " \r%d.%d.%d.%d \r\n\rDNS \r%d.%d.%d.%d\n\r",
        (ConfigOpt == SL_NETCFG_ADDR_DHCP) ? "ON" : "OFF",
        SL_IPV4_BYTE(ipV4.Ip,
                     3),SL_IPV4_BYTE(ipV4.Ip,2),SL_IPV4_BYTE(ipV4.Ip,
                                                             1),
        SL_IPV4_BYTE(ipV4.Ip,0),
        SL_IPV4_BYTE(ipV4.IpMask,3),SL_IPV4_BYTE(ipV4.IpMask,
                                                 2),SL_IPV4_BYTE(ipV4.IpMask,
                                                                 1),
        SL_IPV4_BYTE(ipV4.IpMask,0),
        SL_IPV4_BYTE(ipV4.IpGateway,3),SL_IPV4_BYTE(ipV4.IpGateway,
                                                    2),
        SL_IPV4_BYTE(ipV4.IpGateway,1),SL_IPV4_BYTE(ipV4.IpGateway,0),
        SL_IPV4_BYTE(ipV4.IpDnsServer,
                     3),
        SL_IPV4_BYTE(ipV4.IpDnsServer,2),SL_IPV4_BYTE(ipV4.IpDnsServer,
                                                      1),
        SL_IPV4_BYTE(ipV4.IpDnsServer,0));

    /* Reset Over All Ping Statistics*/
    gPingSent = 0;
    gPingSuccess = 0;
    ClockP_setTimeout(gAsyncEvtTimerHandle, 1000 * PING_TIMEOUT_SEC);
    ClockP_start(gAsyncEvtTimerHandle);

    return(0);
}

//*****************************************************************************
//
//! \brief   Wait for IP, open timer for error
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t HandleWaitForIp()
{
    ClockP_setTimeout(gAsyncEvtTimerHandle,
                      1000 * CONNECTION_PHASE_TIMEOUT_SEC * 3);
    ClockP_start(gAsyncEvtTimerHandle);
    return(0);
}

//*****************************************************************************
//
//! \brief  Disconnect received, disconnect and open timer for re-connect
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t HandleDiscnctEvt()
{
    gIsWlanConnected = 0;
    ClockP_setTimeout(gAsyncEvtTimerHandle,
                      1000 * RECONNECTION_ESTABLISHED_TIMEOUT_SEC);
    ClockP_start(gAsyncEvtTimerHandle);
    return(0);
}

//*****************************************************************************
//
//! \brief  Connection established, check if force provisioning 
//!            (robustness test) or transition to
//!         user application.
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t CheckLanConnection()
{
    /* Force Provisioning Application to run */
    if((FORCE_PROVISIONING)&&(!gWaitForConn))
    {
        LOG_MESSAGE("[App] Force Provisioning Start\r\n");
        returnToFactoryDefault();
        SignalEvent(AppEvent_PROVISIONING_STARTED);
    }
    else
    {
        /* Handle User application */
        SignalEvent(AppEvent_PROVISIONING_STOPPED);
    }
    return(0);
}

//*****************************************************************************
//
//! \brief   IP Acquired, open timer to make sure process completed
//!         user application.
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************

int32_t CheckInternetConnection()
{
    /* Connect to cloud if enabled */
    return(0);
}

//*****************************************************************************
//
//! \brief   Do Nothing
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t DoNothing()
{
    return(0);
}

//*****************************************************************************
//
//! \brief   Handle Provisioning timeout, check if connection success
//!          Continue if success, or start over if fail
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t HandleProvisioningTimeOut()
{
    /* Connection failed, restart */
    SignalEvent(AppEvent_RESTART);
    return(0);
}

//*****************************************************************************
//
//! \brief  This function signals the application events
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int16_t SignalEvent(AppEvent event)
{
    /* Check if FIFO is full, if so, drop event */
    if(mq_send(gSMEventQueue, (const char*)&event, 1, 0) < 0)
    {
        LOG_MESSAGE("ERROR - State Machine Full - dropping event %d",
                    (uint16_t)event);
        return(-1);
    }

    return(0);
}

//*****************************************************************************
//
//! \brief  This function handles init-complete event from SL
//!
//! \param  status - Mode the device is configured or 
//!                 error if initialization failed
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkInitCallback(uint32_t status,
                            SlDeviceInitInfo_t *DeviceInitInfo)
{
    if((int32_t)status == SL_ERROR_RESTORE_IMAGE_COMPLETE)
    {
        LOG_MESSAGE(
            "\r\n**********************************"
            "\r\nReturn to Factory Default been Completed\r\n"
            "Please RESET the Board"
            "\r\n**********************************\r\n");
        gLedDisplayState = LedState_FACTORY_DEFAULT;
        while(1)
        {
            ;
        }
    }

    ASSERT_ON_ERROR((int32_t)status);

    LOG_MESSAGE("Device started in %s role\n\r", (0 == status) ? "Station" : \
                (2 ==
                 status) ? "AP" : ((3 == status) ? "P2P" : "Start Role error"));

    if(ROLE_STA == status)
    {
        gRole = ROLE_STA;
        SignalEvent(AppEvent_STARTED);
    }
    else if(ROLE_AP == status)
    {
        gRole = ROLE_AP;
        SignalEvent(AppEvent_RESTART);
    }
    else
    {
        SignalEvent(AppEvent_ERROR);
    }
}

//*****************************************************************************
//
//! \brief  In case external confirmation (cloud) failed, need to abort
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
void _AbortProvExternalConfirmation()
{
    int16_t retVal;

    LOG_MESSAGE("Aborting provisioning external confirmation...\r\n");
    retVal = sl_WlanProvisioning(
        SL_WLAN_PROVISIONING_CMD_ABORT_EXTERNAL_CONFIRMATION, 0, 0, NULL, 0);
    LOG_MESSAGE("Abort retVal=%d\r\n", retVal);
}

//*****************************************************************************
//
//! \brief  Delete all profiles and return to factory default
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t returnToFactoryDefault()
{
    int32_t slRetVal;
    SlFsRetToFactoryCommand_t RetToFactoryCommand;
    int32_t ExtendedError;

    RetToFactoryCommand.Operation = SL_FS_FACTORY_RET_TO_DEFAULT;
    slRetVal =
        sl_FsCtl((SlFsCtl_e)SL_FS_CTL_RESTORE, 0, NULL,
                 (_u8 *)&RetToFactoryCommand,
                 sizeof(SlFsRetToFactoryCommand_t), NULL, 0,
                 NULL);

    /* If command was not executed because provisioning is currently running,
    send PRVISIONING_STOP command, and wait for the PROVISIONING_STOPPED event 
    (which confirms that the provisioning process was successfully stopped)
    before trying again */

    if(SL_RET_CODE_PROVISIONING_IN_PROGRESS == slRetVal)
    {
        LOG_MESSAGE(
            " [ERROR] Provisioning is already running,"
            " stopping current session...\r\n");
        gStopInProgress = 1;
        slRetVal =
            sl_WlanProvisioning(SL_WLAN_PROVISIONING_CMD_STOP,0, 0, NULL,
                                (uint32_t)NULL);
        ASSERT_ON_ERROR(slRetVal);

        while(gStopInProgress)
        {
            ProcessEvents(); // flush all events
        }

        SignalEvent(AppEvent_RESTART);

        slRetVal =
            sl_FsCtl((SlFsCtl_e)SL_FS_CTL_RESTORE, 0, NULL,
                     (_u8 *)&RetToFactoryCommand,
                     sizeof(SlFsRetToFactoryCommand_t), NULL, 0,
                     NULL);
        ASSERT_ON_ERROR(slRetVal);
    }
    if((int32_t)slRetVal < 0)
    {
        /* Pay attention, for this function the slRetVal is composed from 
                                        Signed RetVal & extended error */
        slRetVal = (_i16)slRetVal >> 16;
        ExtendedError = (_u16)slRetVal & 0xFFFF;
        return(ExtendedError);
    }
    /* Reset */
    sl_Stop(SL_STOP_TIMEOUT);
    sl_Start(NULL, NULL, NULL);

    gIsWlanConnected = 0;

    return(slRetVal);
}

/*!
 *  \brief  This function is the main application task SM
 *  \param  none
 *  \return 0 - success, else - error
 */
int32_t ProcessEvents()
{
    Provisioning_TableEntry_t *pEntry = NULL;
    uint8_t event = 0;

    /* The SimpleLink host driver architecture mandate calling 'sl_task' in
    a NO-RTOS application's main loop.       */
    
    /* The purpose of this call, is to
    handle asynchronous events and get
    flow control information sent from the NWP.*/
    
    /* Every event is classified and 
    later handled by the host driver event handlers. */
    sl_Task(NULL);

    /* Read and execute all messages in queue until no messages are left */
    while(mq_receive(gSMEventQueue, (char*)&event, 1, NULL) >= 0)
    {
       /* Stop Async Event (if any), as we received the event  */
       ClockP_stop(gAsyncEvtTimerHandle);

       /* Debug - Print Event information */
       //LOG_MESSAGE("MainApp: State = %d, Event = %d\n\r",
       //g_CurrentState, event);

       /* Find Next event entry */
       pEntry =
           (Provisioning_TableEntry_t *)
           &gTransitionTable[g_CurrentState][event];

        /* Set next state */
        g_CurrentState = pEntry->nextState;

        /* Call Event function */
        if(NULL != pEntry->p_evtHndl)
        {
            if(pEntry->p_evtHndl() < 0)
            {
                LOG_MESSAGE("Event handler failed..!! \n\r");
                while(1)
                {
                    ;
                }
            }
        }
    }
    return(0);
}

//*****************************************************************************
//
//! \brief  Start Provisioning example
//!
//! \param  None
//!
//! \return None
//!
//*****************************************************************************
int32_t ProvisioningStart()
{
    int32_t retVal = 0;
    int32_t status = 0;
    uint8_t configOpt = 0;
    uint16_t configLen = 0;
    uint8_t simpleLinkMac[SL_MAC_ADDR_LEN];
    uint16_t macAddressLen;
    uint8_t provisioningCmd;
    SlDeviceVersion_t ver = {0};

    LOG_MESSAGE("\n\r\n\r\n\r==================================\n\r");
    LOG_MESSAGE(" Trigger Mode Example Ver. %s\n\r",APPLICATION_VERSION);
    LOG_MESSAGE("==================================\n\r");

    status = GetSecureStatus();
    if((SECURED_AP_ENABLE) || (status != SECURED_AP_ENABLE))
    {
        /*Will enter only if previous and present Secure AP 
                                        are different or Secure AP is 1*/
        SetSecuredAP(SECURED_AP_ENABLE);
        if(ROLE_AP == gRole)
        {
            /* Reset device to start secured AP */
            sl_Stop(SL_STOP_TIMEOUT);
            gRole = sl_Start(NULL, NULL, NULL);
        }
    }

    /* Get device's info */
    configOpt = SL_DEVICE_GENERAL_VERSION;
    configLen = sizeof(ver);
    retVal =
        sl_DeviceGet(SL_DEVICE_GENERAL, &configOpt, &configLen, (_u8 *)(&ver));

    if(SL_RET_CODE_PROVISIONING_IN_PROGRESS == retVal)
    {
        LOG_MESSAGE(
            " [ERROR] Provisioning is already running,"
            " stopping current session...\r\n");
        SignalEvent(AppEvent_ERROR);
        return(0);
    }

    LOG_MESSAGE(
        "\n\r CHIP %d\r\n MAC  31.%d.%d.%d.%d\r\n PHY "
        " %d.%d.%d.%d\r\n NWP  %d.%d.%d.%d\r\n ROM  %d\r\n HOST %d.%d.%d.%d\r\n",
        ver.ChipId,
        ver.FwVersion[0],ver.FwVersion[1],
        ver.FwVersion[2],ver.FwVersion[3],
        ver.PhyVersion[0],ver.PhyVersion[1],
        ver.PhyVersion[2],ver.PhyVersion[3],
        ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],
        ver.NwpVersion[3],
        ver.RomVersion,
        SL_MAJOR_VERSION_NUM,SL_MINOR_VERSION_NUM,SL_VERSION_NUM,
        SL_SUB_VERSION_NUM);

    macAddressLen = sizeof(simpleLinkMac);
    sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET,NULL,&macAddressLen,
                 (unsigned char *)simpleLinkMac);
    LOG_MESSAGE(" MAC address: %x:%x:%x:%x:%x:%x\r\n\r\n",simpleLinkMac[0],
                simpleLinkMac[1],simpleLinkMac[2],simpleLinkMac[3],
                simpleLinkMac[4],
                simpleLinkMac[5]);

    /* start provisioning */

    provisioningCmd = SL_WLAN_PROVISIONING_CMD_START_MODE_APSC;

    if(provisioningCmd <=
       SL_WLAN_PROVISIONING_CMD_START_MODE_APSC_EXTERNAL_CONFIGURATION)
    {
        LOG_MESSAGE(
            "\r\n Starting Provisioning! mode=%d "
            "(0-AP, 1-SC, 2-AP+SC, 3-AP+SC+WAC)\r\n\r\n",
            provisioningCmd);
    }
    else
    {
        LOG_MESSAGE("\r\n Provisioning Command = %d \r\n\r\n",provisioningCmd);
    }

    gIsWlanConnected = 0;

    if(SC_KEY_ENABLE)
    {
        _u8 key[SL_WLAN_SMART_CONFIG_KEY_LENGTH];

        memcpy(key, (char *)SC_KEY, SL_WLAN_SMART_CONFIG_KEY_LENGTH);
        retVal =
            sl_WlanProvisioning(provisioningCmd, ROLE_STA,
                                PROVISIONING_INACTIVITY_TIMEOUT,
                                (char *)&key,
                                (uint32_t)NULL);
    }
    else
    {
        retVal =
            sl_WlanProvisioning(provisioningCmd, ROLE_STA,
                                PROVISIONING_INACTIVITY_TIMEOUT,
                                NULL,
                                (uint32_t)NULL);
    }

    if(retVal < 0)
    {
        LOG_MESSAGE(" Provisioning Command Error, num:%d\r\n",retVal);
    }

    return(0);
}

//*****************************************************************************
//
//! \brief  Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void DisplayBanner(char * AppName)
{
    LOG_MESSAGE("\n\n\n\r");
    LOG_MESSAGE("\r\r *************************************************\n\r");
    LOG_MESSAGE("\r\r           CC32xx %s Application       \n\r", AppName);
    LOG_MESSAGE("\r\r *************************************************\n\r");
    LOG_MESSAGE("\n\n\n\r");
}

//*****************************************************************************
//
//! \brief  Main
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
int32_t mainThread(void *arg)
{
    int32_t RetVal;
    ClockP_Params EventParams;
    ClockP_Params DisplayParams;
    ClockP_Params LPDSParams;

    GPIO_init();
    SPI_init();
    Power_init();

    /*
     *  This function enables the Power policy function to run
     *  in the idle loop. Alternatively, you can set the field
     *  'enablePolicy' in the Power_config structure (CC3200_LP.c)
     *  to 1.
     */
    Power_enablePolicy();

    /* init Terminal, and print App name */
    gUartHandle = InitTerm();
    DisplayBanner(APPLICATION_NAME);

    /* Switch off all LEDs on boards */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_2, CONFIG_GPIO_LED_OFF);

    /* Initialize Simple Link */
    if((RetVal =
            sl_Start(NULL, NULL, (P_INIT_CALLBACK)SimpleLinkInitCallback)) < 0)
    {
        LOG_MESSAGE("sl_Start Failed\r\n");
        if(RetVal == SL_ERROR_RESTORE_IMAGE_COMPLETE)
        {
            LOG_MESSAGE(
                "\r\n**********************************\r\n"
                "Return to Factory Default been Completed"
                "\r\nPlease RESET the Board"
                "\r\n**********************************\r\n");
            gLedDisplayState = LedState_FACTORY_DEFAULT;
        }
        while(1)
        {
            ;
        }
    }

    /* Create message queue */
    gSMEventQueue = mq_open("SMQueue", O_CREAT, (int32_t)NULL, NULL);

    /* Create an events timer */
    ClockP_Params_init(&EventParams);
    gAsyncEvtTimerHandle = ClockP_create((ClockP_Fxn) & AsyncEvtTimerHandler, 
                                         0,
                                         &EventParams);

    /* Create a display timer */
    ClockP_Params_init(&DisplayParams);
    gDisplayHandle = ClockP_create((ClockP_Fxn) & UpdateLedDisplay, 0,
                                         &DisplayParams);

    /* Create a dummy timer */
    ClockP_Params_init(&LPDSParams);
    gLPDSHandle = ClockP_create((ClockP_Fxn) & triggerModeDoNothing, 0,
                                         &LPDSParams);

    do
    {
        RetVal = ProcessEvents();

        /* Handle the display process */
        if(!gDisplayClockActive && !gIsWlanConnected)
        {
            ClockP_setTimeout(gDisplayHandle,500);
            ClockP_start(gDisplayHandle);
            gDisplayClockActive = TRUE;
        }
    }
    while(!RetVal); /* Exit on failure */

    return(0);
}
