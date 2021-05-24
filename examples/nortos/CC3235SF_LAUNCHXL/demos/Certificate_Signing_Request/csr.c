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

   Application Name     - CSR - Certificate Signing Request
   Application Overview -

   Application Details  - Refer to 'CSR' README.html

*****************************************************************************/

//****************************************************************************
//
//! \addtogroup
//! @{
//
//****************************************************************************

//*****************************************************************************
// Includes
//*****************************************************************************

// Standard includes
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
/* TI-DRIVERS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/net/wifi/simplelink.h>

#include "common.h"
#include "uart_term.h"
#include "time.h"

/* For usleep() */
#include <unistd.h>
#include <ti/net/utils/str_mpl.h>
//*****************************************************************************
// Defines
//*****************************************************************************

#define APPLICATION_NAME            "CSR - Certificate Signing Request"
#define APPLICATION_VERSION         "1.0.0.2"

#define SL_STOP_TIMEOUT             (200)

//*****************************************************************************
// Typedefs
//*****************************************************************************

//*****************************************************************************
// Function prototypes
//*****************************************************************************

//*****************************************************************************
// Globals
//*****************************************************************************
char cmdStr[CMD_BUFFER_LEN + 1];
//*****************************************************************************
// Callback Functions
//*****************************************************************************

//*****************************************************************************
//
//! \brief Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_1
//!
//! \param  index
//!
//! \return none
//!
//*****************************************************************************

//*****************************************************************************
// SimpleLink Callback Functions
//*****************************************************************************

void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    // do nothing...
}

void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest,
                                         SlNetAppResponse_t *pNetAppResponse)
{
    // do nothing...
}

//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
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
        UART_PRINT("\n\r[WLAN EVENT] STA Connected to the AP: %s \n\r",
                   pWlanEvent->Data.Connect.SsidName);
    }
    break;

    case SL_WLAN_EVENT_DISCONNECT:
    {
        UART_PRINT("\n\r[WLAN ERROR] Device disconnected from the AP: %s\n\r",
                   pWlanEvent->Data.Disconnect.SsidName);
    }
    break;
    case SL_WLAN_EVENT_STA_ADDED:
        UART_PRINT(
            "\n\r[WLAN EVENT] New STA Addeed "
            "(MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x)\r\n",
            pWlanEvent->Data.STAAdded.Mac[0],
            pWlanEvent->Data.STAAdded.Mac[1],
            pWlanEvent->Data.STAAdded.Mac[2],
            pWlanEvent->Data.STAAdded.Mac[3],
            pWlanEvent->Data.STAAdded.Mac[4],
            pWlanEvent->Data.STAAdded.Mac[5]);
        break;

    case SL_WLAN_EVENT_STA_REMOVED:
        UART_PRINT(
            "\n\r[WLAN EVENT] STA Removed "
            "(MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x)\r\n",
            pWlanEvent->Data.STAAdded.Mac[0],
            pWlanEvent->Data.STAAdded.Mac[1],
            pWlanEvent->Data.STAAdded.Mac[2],
            pWlanEvent->Data.STAAdded.Mac[3],
            pWlanEvent->Data.STAAdded.Mac[4],
            pWlanEvent->Data.STAAdded.Mac[5]);
        break;
    default:
    {
        UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                   pWlanEvent->Id);
    }
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
        UART_PRINT(
            "[ERROR] - FATAL ERROR: Abort NWP event detected:"
            " AbortType=%d, AbortData=0x%x\n\r",
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
        UART_PRINT(
            "[ERROR] - FATAL ERROR: No Cmd Ack detected "
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
        UART_PRINT(
            "[ERROR] - FATAL ERROR: Async event timeout detected "
            "[event opcode =0x%x]  \n\r",
            slFatalErrorEvent->Data.CmdTimeout.Code);
    }
    break;

    default:
        UART_PRINT("[ERROR] - FATAL ERROR: Unspecified error detected \n\r");
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
    {
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
    }
    break;

    default:
    {
        UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                   pNetAppEvent->Id);
    }
    break;
    }
}

//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerEventHandler(SlNetAppHttpServerEvent_t *pHttpEvent,
                                      SlNetAppHttpServerResponse_t *
                                      pHttpResponse)
{
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->Data.Error.Code,
               pDevEvent->Data.Error.Source);
}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
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
            UART_PRINT(
                "[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                "(%d) \n\n",
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

void SimpleLinkSocketTriggerEventHandler(SlSockTriggerEvent_t *pSlTriggerEvent)
{
    /* Unused in this application */
}

int32_t IsValisString(uint8_t * pBuffer,
                      int32_t length)
{
    int i;

    for(i = 0; i < length; i++)
    {
        if(!(((*pBuffer >= 'a') && (*pBuffer <= 'z'))    ||
             ((*pBuffer >= 'A') && (*pBuffer <= 'Z'))     ||
             ((*pBuffer >= '0') && (*pBuffer <= '9'))    ||
             ((*pBuffer >= '*') && (*pBuffer <= '/'))   ||
             (*pBuffer == 0x0) || (*pBuffer == ' ')))
        {
            UART_PRINT("ERROR: Invalid string! \n\r");
            return(FALSE);
        }
        pBuffer++;
    }
    return(TRUE);
}

int32_t IsValisSerial(uint8_t * pBuffer, int32_t length)
{
    int i;

    for(i = 0; i < length; i++)
    {
        if(!(((*pBuffer >= 'a') && (*pBuffer <= 'f'))    ||
             ((*pBuffer >= 'A') && (*pBuffer <= 'F'))     ||
             ((*pBuffer >= '0') && (*pBuffer <= '9'))))
        {
            UART_PRINT("ERROR: Invalid string! \n\r");
            return(FALSE);
        }
        pBuffer++;
    }
    return(TRUE);
}

//*****************************************************************************
// Local Functions
//*****************************************************************************

//*****************************************************************************
//
//! \brief Display Application Banner
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
int32_t displayBanner(void)
{
    int32_t status = -1;
    uint8_t macAddress[SL_MAC_ADDR_LEN];
    uint16_t macAddressLen = SL_MAC_ADDR_LEN;
    uint16_t configSize = 0;
    uint8_t configOpt = SL_DEVICE_GENERAL_VERSION;
    SlDeviceVersion_t ver = {0};

    configSize = sizeof(SlDeviceVersion_t);

    /* Disable Auto Provisioning */
    status =
        sl_WlanProvisioning(SL_WLAN_PROVISIONING_CMD_STOP,
                                 0xFF,
                                 0,
                                 NULL,
                                 0x0);
    ASSERT_ON_ERROR(status);
  
    /* Print device version info. */
    status =
        sl_DeviceGet(SL_DEVICE_GENERAL, &configOpt, &configSize,
                     (uint8_t*)(&ver));
    ASSERT_ON_ERROR(status);

    /* Print device Mac address */
    status = sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET, 0, &macAddressLen,
                          &macAddress[0]);
    ASSERT_ON_ERROR(status);

    UART_PRINT("\n\n\n\r");
    UART_PRINT(
        "\t ======================================================"
        "===============\n\r");
    UART_PRINT("\t    %s Example Ver: %s\n\r",APPLICATION_NAME,
               APPLICATION_VERSION);
    UART_PRINT(
        "\t ======================================================="
        "==============\n\r");
    UART_PRINT("\n\r");
    UART_PRINT("\t CHIP: 0x%x",ver.ChipId);
    UART_PRINT("\n\r");
    UART_PRINT("\t MAC:  %d.%d.%d.%d",ver.FwVersion[0],ver.FwVersion[1],
               ver.FwVersion[2],
               ver.FwVersion[3]);
    UART_PRINT("\n\r");
    UART_PRINT("\t PHY:  %d.%d.%d.%d",ver.PhyVersion[0],ver.PhyVersion[1],
               ver.PhyVersion[2],
               ver.PhyVersion[3]);
    UART_PRINT("\n\r");
    UART_PRINT("\t NWP:  %d.%d.%d.%d",ver.NwpVersion[0],ver.NwpVersion[1],
               ver.NwpVersion[2],
               ver.NwpVersion[3]);
    UART_PRINT("\n\r");
    UART_PRINT("\t ROM:  %d",ver.RomVersion);
    UART_PRINT("\n\r");
    UART_PRINT("\t HOST: %s", SL_DRIVER_VERSION);
    UART_PRINT("\n\r");
    UART_PRINT("\t MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
               macAddress[0],
               macAddress[1],
               macAddress[2],
               macAddress[3],
               macAddress[4],
               macAddress[5]);
    UART_PRINT("\n\r");
    UART_PRINT("\n\r");
    UART_PRINT(
        "\t ======================================================"
        "===============\n\r");
    UART_PRINT("\n\r");
    UART_PRINT("\n\r");

    return(status);
}

void printBuffer(uint8_t* buff,
                 int16_t buffLen)
{
    int32_t i,line = 0;

    for(i = 0; i < buffLen; i++)
    {
        if((64 == line) || ('\r' == buff[i]))
        {
            UART_PRINT("\r\n");
            line = 0;
        }

        UART_PRINT("%c",buff[i]);
        line++;
    }
    UART_PRINT("\r\n\r\n");
}

void RandSerial(uint8_t *dest,
                uint32_t length)
{
    uint8_t i;
    time_t t;

    srand((unsigned) time(&t));

    for(i = 0; i < length; i++)
    {
        dest[i] = rand() % 0xFF;
    }
}

int32_t SetCertInfo(void)
{
    int16_t retVal = INVALID_VALUE;
    uint8_t i,configOpt;
    uint16_t configLen;
    uint8_t udid[16] = {0};
    SlNetUtilCryptoCmdCreateCertAttrib_t certCmd;
    uint32_t certVersion = DEFAULT_CERT_VERSION;
    uint32_t certSigType;
    uint32_t certDaysValid;
    uint8_t certIsCaString;
    uint32_t certIsCa;
    uint8_t* certSubjectCountry;
    uint8_t* certSubjectState;
    uint8_t* certSubjectLocality;
    uint8_t* certSubjectSur;
    uint8_t* certSubjectCommonName;
    uint8_t* certSubjectOrg;
    uint8_t* certSubjectOrgUnit;
    uint8_t* certSubjectEmail;
    uint8_t* tempStr;
    uint16_t outputLen = 0;
    uint32_t StringLength = 0;

    configLen = sizeof(udid);
    configOpt = SL_DEVICE_IOT_UDID;

    /* read UDID (Unique device ID). This command creates the key pair */
    retVal = sl_DeviceGet(SL_DEVICE_IOT,&configOpt, &configLen,udid);
    if(0 != retVal)
    {
        return(retVal);
    }

    UART_PRINT("Unique device ID = ");
    for(i = 0; i < 16; i++)
    {
        UART_PRINT("%.2x ",udid[i]);
    }
    UART_PRINT("\r\n\n");

    /* Create the device certificate (identity) */
    UART_PRINT(
        "-----Creating CSR-----\r\nEnter the following information: "
        "(press enter to use default values):\r\n\n");

    /* initialize the creation process */
    certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_INIT;
    certCmd.ObjId = SL_NETUTIL_CRYPTO_SERVICES_IOT_RESERVED_INDEX;
    certCmd.Flags = 0;
    retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                           (uint8_t*)&certCmd,sizeof(certCmd),
                           NULL,0,
                           NULL,&outputLen);
    if(0 != retVal)
    {
        UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        return(retVal);
    }

    /* Sets certificate version ( encoded certificate version: 0=v1, 1=v2, 2=v3)
     * SimpleLink device support only v3 */

    certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_VER;
    retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                           (uint8_t*)&certCmd,
                           sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                           (uint8_t*)&certVersion, sizeof(certVersion),
                           NULL, &outputLen);
    if(0 != retVal)
    {
        UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        return(retVal);
    }
	

	/* Sets certificate signature type
     * The type represent the algorithm identifier for the algorithm used
     * by the device to sign the certificate
     * SimpleLink device support only SL_UTILS_CRYPTO_SIG_SHAwECDSA for
       certificate generation */

    certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SIG_TYPE;
    certSigType = SL_NETUTIL_CRYPTO_SIG_SHAwECDSA;

    retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                           (uint8_t*)&certCmd,
                           sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                           (uint8_t*)&certSigType, sizeof(certSigType),
                           NULL,  &outputLen);
    if(0 != retVal)
    {
        UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        return(retVal);
    }

    /* Get and set certificate validity period
     * validity period dates defined from now until now + daysValid */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter validity period (days):");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        tempStr = (uint8_t *)strtok(cmdStr, " ");
        if (tempStr == NULL)
        {
            UART_PRINT("ERROR: strtok - there is not token \n\r");
            retVal = INVALID_VALUE;
            return(retVal);
        }
        certDaysValid = atoi((char *)tempStr);
        if(0 == StringLength)
        {
            certDaysValid = DEFAULT_VALID_DAYS;
            UART_PRINT("  Default validity period (days): %d Days \r\n\n",
                       certDaysValid);
        }
        if(certDaysValid > 0)
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_DAYS_VALID;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t *)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   (uint8_t*)&certDaysValid,
                                   sizeof(certDaysValid),
                                   NULL,  &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate validity period\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }
    /* define if certificate is a ca certificate */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Is certificate CA? (0-No/1-Yes):");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            memcpy(&certIsCaString,DEFAULT_IS_CA,1);
            UART_PRINT("  Default certificate is CA? (0-No/1-Yes): %c  \r\n\n",
                       certIsCaString);
        }
        else
        {
            memcpy(&certIsCaString,strtok(cmdStr, " "),sizeof(uint8_t));
        }
        certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_IS_CA;

        if(('0' == certIsCaString) || ('1' == certIsCaString))
        {
            certIsCa = atoi(( const char *)&certIsCaString);
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   (uint8_t*)&certIsCa, sizeof(certIsCa),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate CA input \n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set subject country */

    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject country (i.e US):");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            certSubjectCountry = (uint8_t *)DEFAULT_COUNTRY;
            StringLength = strlen((char *)certSubjectCountry);
            UART_PRINT("  Default subject country: %s  \r\n\n",
                       certSubjectCountry);
        }
        else
        {
            certSubjectCountry = (uint8_t *)strtok(cmdStr, " ");
        }
        if((StringLength == 2) &&
           (certSubjectCountry[0] >= 'A') &&
           (certSubjectCountry[0] <= 'Z') &&
           (certSubjectCountry[1] >= 'A') &&
           (certSubjectCountry[1] <= 'Z'))
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_COUNTRY;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectCountry,
                                   (strlen((char *)certSubjectCountry) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject country\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set certificate subject state */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject state:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            certSubjectState = (uint8_t *)DEFAULT_STATE;
            UART_PRINT("  Default subject state: %s  \r\n\n", certSubjectState);
            StringLength = strlen((char *)certSubjectState);
        }
        else
        {
            certSubjectState = (uint8_t *)strtok(cmdStr, " ");
        }
        if((StringLength <= MAX_STRING_LENGTH) &&
           (IsValisString(certSubjectState, StringLength)))
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_STATE;

            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectState,
                                   (strlen((char *)certSubjectState) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject state\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set the subject locality */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject locality:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");

        if(0 == StringLength)
        {
            certSubjectLocality = (uint8_t *)DEFAULT_LOCALITY;
            UART_PRINT("  Default subject locality: %s  \r\n\n",
                       certSubjectLocality);
            StringLength = strlen((char *)certSubjectLocality);
        }
        else
        {
            certSubjectLocality = (uint8_t *)strtok(cmdStr, " ");
        }
        if((StringLength <= MAX_STRING_LENGTH) &&
           (IsValisString(certSubjectLocality, StringLength)))
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_LOCALITY;

            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectLocality,
                                   (strlen((char *)certSubjectLocality) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject locality\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set the subject surname */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject surname:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            certSubjectSur = (uint8_t *)DEFAULT_SURNAME;
            UART_PRINT("  Default subject surname: %s  \r\n\n", certSubjectSur);
            StringLength = strlen((char *)certSubjectSur);
        }
        else
        {
            certSubjectSur = (uint8_t *)strtok(cmdStr, " ");
        }

        if((StringLength <= MAX_STRING_LENGTH) &&
           (IsValisString(certSubjectLocality, StringLength)))
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_SUR;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectSur,
                                   (strlen((char *)certSubjectSur) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject surname\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set the subject organization */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject organization:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            certSubjectOrg = (uint8_t *)DEFAULT_ORG;
            UART_PRINT("  Default subject organization: %s  \r\n\n",
                       certSubjectOrg);
            StringLength = strlen((char *)certSubjectOrg);
        }
        else
        {
            certSubjectOrg = (uint8_t *)strtok(cmdStr, " ");
        }

        if((StringLength <= MAX_STRING_LENGTH) &&
           IsValisString(certSubjectOrg, StringLength))
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_ORG;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectOrg,
                                   (strlen((char *)certSubjectOrg) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject organization\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set the subject organization unit */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject organization unit:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            certSubjectOrgUnit = (uint8_t *)DEFAULT_ORG_UNIT;
            UART_PRINT("  Default subject organization unit: %s  \r\n\n",
                       certSubjectOrgUnit);
            StringLength = strlen((char *)certSubjectOrgUnit);
        }
        else
        {
            certSubjectOrgUnit = (uint8_t *)strtok(cmdStr, " ");
        }

        if((StringLength <= MAX_STRING_LENGTH) &&
           IsValisString(certSubjectState, StringLength))
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_ORG_UNIT;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectOrgUnit,
                                   (strlen((char *)certSubjectOrgUnit) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT(
                "ERROR: Invalid certificate subject organization unit \n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set the subject common name  */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject common name:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            /* set default common name to be the uuid */
            certSubjectCommonName = (uint8_t *)cmdStr;
            StringLength = sizeof(udid);
            UART_PRINT("  Default subject common name is the uuid: ");
            for(i = 0; i < 16; i++)
            {
                UART_PRINT("%x",udid[i]);
                sprintf((char *)certSubjectCommonName, "%x", udid[i]);
                certSubjectCommonName += 2;
            }
            certSubjectCommonName = (uint8_t *)cmdStr;
            UART_PRINT("\r\n\n");
        }
        else /* max size is 64 */
        {
            certSubjectCommonName = (uint8_t *)strtok(cmdStr, " ");
            if (certSubjectCommonName == NULL)
            {
                UART_PRINT("ERROR: strtok - there is not token \n\r");
                retVal = INVALID_VALUE;
                return (retVal);
            }
            if(!IsValisString(certSubjectCommonName, StringLength))
            {
                UART_PRINT("ERROR: Invalid certificate subject common name\n\r");
                retVal = INVALID_VALUE;
            }
        }
        if(StringLength <= MAX_STRING_LENGTH)
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_COMMON_NAME;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectCommonName,
                                   (strlen((char *)certSubjectCommonName) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject common name\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    /* Set the subject email */
    while(TRUE)
    {
        UART_PRINT("\r\n\n");
        UART_PRINT("-Enter subject email:");
        StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
        UART_PRINT("\r\n");
        if(0 == StringLength)
        {
            certSubjectEmail = (uint8_t *)DEFAULT_EMAIL;
            UART_PRINT("  Default email: %s  \r\n\n", certSubjectEmail);
            StringLength = strlen((char *)certSubjectEmail);
        }
        else
        {
            certSubjectEmail = (uint8_t *)strtok(cmdStr, " ");
        }

        if(StringLength <= MAX_STRING_LENGTH)
        {
            certCmd.SubCmd = SL_NETUTIL_CRYPTO_CERT_SUBJECT_EMAIL;
            retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                                   (uint8_t*)&certCmd,
                                   sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                                   certSubjectEmail,
                                   (strlen((char *)certSubjectEmail) + 1),
                                   NULL, &outputLen);
        }
        else
        {
            UART_PRINT("ERROR: Invalid certificate subject email\n\r");
            retVal = INVALID_VALUE;
        }
        if(0 != retVal)
        {
            UART_PRINT("ERROR: NetUtilCmd error: %d: \n\r",retVal);
        }
        else
        {
            break;
        }
    }

    UART_PRINT("\r\n");

    /* Close the process and create the certificate */

    certCmd.SubCmd = SL_NETUTIL_CRYPTO_CSR_SIGN_AND_SAVE;
    retVal = sl_NetUtilCmd(SL_NETUTIL_CRYPTO_CMD_CREATE_CERT,
                           (uint8_t*)&certCmd,
                           sizeof(SlNetUtilCryptoCmdCreateCertAttrib_t),
                           NULL, 0,
                           NULL, &outputLen);

    return(retVal);
}

void GetCertPem(uint8_t *Cert)
{
    int32_t fHdl;
    uint8_t fileName[] = "/cert/iot/csr.der";
    uint8_t data[CSR_BUFF_SIZE];
    uint8_t data64base[CSR_BUFF_SIZE];
    int32_t status;
    SlFsFileInfo_t csrFileInf;
    uint32_t length = 0;
    int32_t location = 0;
    status = sl_FsGetInfo(fileName,0,&csrFileInf);
    if(status < 0)
    {
        UART_PRINT("ERROR: FS Get file info error \n\r");
    }

    fHdl = sl_FsOpen(fileName, SL_FS_READ, NULL);

    if(fHdl > 0)
    {
        status = sl_FsRead(fHdl, 0, data, csrFileInf.Len);

        sl_FsClose(fHdl,0,0,0);

        if(status < 0)
        {
            UART_PRINT("ERROR: FS close file error \n\r");
        }

        /* convert to pem format and print to terminal */

        StrMpl_encodeBase64(data,csrFileInf.Len, data64base, &length);

        memcpy((void *)&Cert[location],(void *)BEGIN_CERT_REQ,
               strlen(BEGIN_CERT_REQ));
        location += strlen(BEGIN_CERT_REQ);
        Cert[location] = '\r';
        location++;
        memcpy((void *)&Cert[location],(void *)data64base,length);
        location += length;
        Cert[location] = '\r';
        location++;
        memcpy((void *)&Cert[location],
               (void *)END_CERT_REQ,strlen(END_CERT_REQ));
        location += strlen(END_CERT_REQ);
        Cert[location] = '\r';
        location++;

        printBuffer(Cert,location);
    }
    else
    {
        UART_PRINT("ERROR: FS open file error \n\r");
    }
}

void GetSignedCertAndWriteToFS()
{
    uint8_t fileName[180] = {0};
    uint8_t* tempStr;
    int32_t fHdl;
    int32_t CertLength = 0;
    int32_t StringLength = 0;


    UART_PRINT("Enter certificate file name:");
    StringLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, FALSE, FALSE);
    UART_PRINT("\r\n");
    if(StringLength > 0)
    {
        tempStr = (uint8_t *)strtok(cmdStr, " ");
        if (tempStr == NULL)
        {
            UART_PRINT("ERROR: strtok - there is not token \n\r");
            return;
        }
        memcpy(fileName,(uint8_t *)tempStr,StringLength + 1);
    }
    else
    {
        StringLength = strlen(DEFAULT_FILE_NAME) + 1;
        memcpy(fileName,DEFAULT_FILE_NAME,StringLength);
        UART_PRINT("  Default certificate file name: %s  \r\n", fileName);
        UART_PRINT("\r\n");
    }
    if(StringLength < FILE_NAME_MAX_LEN)
    {
        UART_PRINT(
            "Copy certificate, sign with ecc-secp256r1, in pem format only, "
            "and paste here:\n\r");
        CertLength = GetCmd((char *)cmdStr, CMD_BUFFER_LEN, TRUE, TRUE);

        fHdl =
            sl_FsOpen(fileName,
                      (SL_FS_CREATE | SL_FS_CREATE_FAILSAFE | SL_FS_OVERWRITE |
                       SL_FS_CREATE_MAX_SIZE(CertLength)),NULL);
        if(fHdl >= 0)
        {
            sl_FsWrite(fHdl, 0, (unsigned char *)cmdStr, CertLength);
            sl_FsClose(fHdl,0,0,0);
            UART_PRINT("\r\n");
            UART_PRINT("The certificate is ready to use! \n\r");
        }
        else
        {
            UART_PRINT("ERROR: FS open file error: %d \n\r", fHdl);
        }
    }
}

//***************************************************************************
//
//! \brief Task Created by main function.This task starts simpleLink, set NWP
//!        power policy, connects to an AP. Give Signal to the other task about
//!        the connection.wait for the message form the interrupt handlers and
//!        the other task. Accordingly print the wake up cause from the low
//!        power modes.
//!
//! \param pvParameters is a general void pointer (not used here).
//!
//! \return none
//
//*****************************************************************************

void mainThread(void *pvParameters)
{
    uint8_t CertPemFormat[CSR_BUFF_SIZE];
    int32_t status = -1;

    GPIO_init();
    SPI_init();

    /* Configure the UART */
    InitTerm();

    /* Turn on user LED */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    /* Start the device */
    status = sl_Start(0, 0, 0);
    if(status < 0)
    {
        ERR_PRINT(status);
    }

    /* Displays the Application Banner */
    displayBanner();

    /* Get certificate information from the user,
        and sign with internal private key */
    status = SetCertInfo();
    if(status < 0)
    {
        ERR_PRINT(status);
        return;
    }

    /* Print cert in pem format to terminal */
    GetCertPem(CertPemFormat);

    /* Receive signed certificate and write to device FS */
    GetSignedCertAndWriteToFS();
}
