/*
 * FreeRTOS OTA PAL.
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

#ifndef OTA_PAL_H
#define OTA_PAL_H

#include "ota.h"

OtaPalImageState_t xOtaPalGetPlatformImageState( OtaFileContext_t * const pFileContext );

OtaPalStatus_t xOtaPalSetPlatformImageState( OtaFileContext_t * const pFileContext,
                                             OtaImageState_t eState );

OtaPalStatus_t xOtaPalResetDevice( OtaFileContext_t * const pFileContext );

OtaPalStatus_t xOtaPalActivateNewImage( OtaFileContext_t * const pFileContext );

int16_t xOtaPalWriteBlock( OtaFileContext_t * const pFileContext,
                           uint32_t offset,
                           uint8_t * const pData,
                           uint32_t blockSize );
OtaPalStatus_t xOtaPalCloseFile( OtaFileContext_t * const pFileContext );

OtaPalStatus_t xOtaPalCreateFileForRx( OtaFileContext_t * const pFileContext );


OtaPalStatus_t xOtaPalAbort( OtaFileContext_t * const pFileContext );

OtaPalStatus_t xOtaPalOpenFileForRead( OtaFileContext_t * const pContext );

int32_t xOtaPalReadBlock( OtaFileContext_t * const pContext,
                          uint32_t offset,
                          uint8_t * pData,
                          uint16_t blockSize );

#endif /* OTA_PAL_H */
