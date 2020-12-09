/*
 * FreeRTOS PKCS #11 V1.0.3
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#ifndef _PROVISION_H_
#define _PROVISION_H_

#include "core_pkcs11.h"

uint8_t * vCreateCsr( void );

CK_RV xProvisionCert( CK_BYTE_PTR xCert,
                      CK_ULONG xCertLen, 
                      CK_BYTE_PTR xCertLabel,
                      CK_ULONG xCertLabelLen );

CK_RV xCheckIfProvisioned( void );

CK_RV xDestroyCryptoObjects( void );

CK_RV xProvisionPublicKey( CK_BYTE_PTR pucKey,
                           CK_ULONG xKeyLength,
                           CK_KEY_TYPE xPublicKeyType,
                           CK_BYTE_PTR pxPublicKeyLabel,
                           CK_ULONG ulPublicKeyLabelLen );

#endif /* ifndef _PROVISION_H_ */
