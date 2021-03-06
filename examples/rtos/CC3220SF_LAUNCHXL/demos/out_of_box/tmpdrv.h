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

#ifndef __TMP006DRV_H__
#define __TMP006DRV_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

/* TI-DRIVERS Header files */
#include <ti/drivers/I2C.h>

/* driverlib Header files */
#include <ti/devices/cc32xx/inc/hw_types.h>

//*****************************************************************************
// TMP006 & TMP116 Register offset address
//*****************************************************************************
#define TMP006_VOBJECT_REG_ADDR         0x0
#define TMP006_TAMBIENT_REG_ADDR        0x1
#define TMP006_CONFIG_REG_ADDR          0x2
#define TMP006_MANUFAC_ID_REG_ADDR      0xFE
#define TMP006_DEVICE_ID_REG_ADDR       0xFF
#define TMP006_ADDR                     0x41
#define TMP116_TAMBIENT_REG_ADDR        0x0
#define TMP116_DEVICE_ID_REG_ADDR       0x0F
#define TMP116_ADDR                     0x49

//*****************************************************************************
// TMP006 & TMP116 Device details
//*****************************************************************************
#define TMP006_MANUFAC_ID       0x5449
#define TMP006_DEVICE_ID        0x0067
#define TMP116_DEVICE_ID        0x0116


//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************

//****************************************************************************
//
//! \brief Initialize the temperature sensor
//!         1. Get the device manufacturer and version
//!         2. Add any initialization here
//!
//! \param i2cHandle[in] the handle to the openned i2c device
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TMP006DrvOpen(I2C_Handle i2cHandle);

//****************************************************************************
//
//! \brief Get the temperature value
//!         1. Get the sensor voltage reg and ambient temp reg values
//!         2. Compute the temperature from the read values
//!
//! \param[in]  i2cHandle   the handle to the openned i2c device
//! \param[out]     pfCurrTemp  the pointer to the temperature value store
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TMPDrvGetTemp(I2C_Handle i2cHandle,
                     float *pfCurrTemp);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __TMP006DRV_H__
