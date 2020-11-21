/*
 * FreeRTOS OTA Update signature validation.
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

#include "ota_update.h"
#include "fsl_debug_console.h"

#include "spifi_boot.h"

#include "iot_crypto.h"
#include "core_pkcs11.h"
#include "aws_ota_codesigner_certificate.h"
#include "aws_iot_ota_pal.h"

/**
 * @brief The crypto algorithm used for the digital signature.
 */
#define CRYPTO_ALGORITHM      cryptoASYMMETRIC_ALGORITHM_ECDSA

/**
 * @brief The signature method used for calculating the signature.
 */
#define SIGNATURE_METHOD      cryptoHASH_ALGORITHM_SHA256

#define DATA_BUFFER_LENGTH    ( 4096 )

static uint8_t dataBuffer[ DATA_BUFFER_LENGTH ];

static CK_RV prvGetCertificateHandlePKCS11( CK_FUNCTION_LIST_PTR pxFunctionList,
                                            CK_SESSION_HANDLE xSession,
                                            const char * pcLabelName,
                                            CK_OBJECT_HANDLE_PTR pxCertHandle )
{
    CK_ATTRIBUTE xTemplate;
    CK_RV xResult = CKR_OK;
    CK_ULONG ulCount = 0;
    CK_BBOOL xFindInit = CK_FALSE;

    /* Get the certificate handle. */
    if( 0 == xResult )
    {
        xTemplate.type = CKA_LABEL;
        xTemplate.ulValueLen = strlen( pcLabelName ) + 1;
        xTemplate.pValue = ( char * ) pcLabelName;
        xResult = pxFunctionList->C_FindObjectsInit( xSession, &xTemplate, 1 );
    }

    if( 0 == xResult )
    {
        xFindInit = CK_TRUE;
        xResult = pxFunctionList->C_FindObjects( xSession,
                                                 ( CK_OBJECT_HANDLE_PTR ) pxCertHandle,
                                                 1,
                                                 &ulCount );
    }

    if( ( CK_TRUE == xFindInit ) && ( xResult == 0 ) )
    {
        xResult = pxFunctionList->C_FindObjectsFinal( xSession );
    }

    return xResult;
}

/* Note that this function mallocs a buffer for the certificate to reside in,
 * and it is the responsibility of the caller to free the buffer. */
static CK_RV prvGetCertificatePKCS11( const char * pcLabelName,
                                      uint8_t ** ppucData,
                                      uint32_t * pulDataSize )
{
    /* Find the certificate */
    CK_OBJECT_HANDLE xHandle = 0;
    CK_RV xResult;
    CK_FUNCTION_LIST_PTR xFunctionList;
    CK_SLOT_ID xSlotId;
    CK_ULONG xCount = 1;
    CK_SESSION_HANDLE xSession;
    CK_ATTRIBUTE xTemplate = { 0 };
    uint8_t * pucCert = NULL;
    CK_BBOOL xSessionOpen = CK_FALSE;

    xResult = C_GetFunctionList( &xFunctionList );

    if( CKR_OK == xResult )
    {
        xResult = xFunctionList->C_Initialize( NULL );
    }

    if( ( CKR_OK == xResult ) || ( CKR_CRYPTOKI_ALREADY_INITIALIZED == xResult ) )
    {
        xResult = xFunctionList->C_GetSlotList( CK_TRUE, &xSlotId, &xCount );
    }

    if( CKR_OK == xResult )
    {
        xResult = xFunctionList->C_OpenSession( xSlotId, CKF_SERIAL_SESSION, NULL, NULL, &xSession );
    }

    if( CKR_OK == xResult )
    {
        xSessionOpen = CK_TRUE;
        xResult = prvGetCertificateHandlePKCS11( xFunctionList, xSession, pcLabelName, &xHandle );
    }

    if( ( xHandle != 0 ) && ( xResult == CKR_OK ) ) /* 0 is an invalid handle */
    {
        /* Get the length of the certificate */
        xTemplate.type = CKA_VALUE;
        xTemplate.pValue = NULL;
        xResult = xFunctionList->C_GetAttributeValue( xSession, xHandle, &xTemplate, xCount );

        if( xResult == CKR_OK )
        {
            pucCert = pvPortMalloc( xTemplate.ulValueLen );
        }

        if( ( xResult == CKR_OK ) && ( pucCert == NULL ) )
        {
            xResult = CKR_HOST_MEMORY;
        }

        if( xResult == CKR_OK )
        {
            xTemplate.pValue = pucCert;
            xResult = xFunctionList->C_GetAttributeValue( xSession, xHandle, &xTemplate, xCount );

            if( xResult == CKR_OK )
            {
                *ppucData = pucCert;
                *pulDataSize = xTemplate.ulValueLen;
            }
            else
            {
                vPortFree( pucCert );
            }
        }
    }
    else /* Certificate was not found. */
    {
        *ppucData = NULL;
        *pulDataSize = 0;
    }

    if( xSessionOpen == CK_TRUE )
    {
        ( void ) xFunctionList->C_CloseSession( xSession );
    }

    return xResult;
}

static char * prvGetCertificate( const char * pcCertName,
                                 uint32_t * ulSignerCertSize )
{
    uint8_t * pucCertData;
    uint32_t ulCertSize;
    uint8_t * pucSignerCert = NULL;
    CK_RV xResult;

    xResult = prvGetCertificatePKCS11( pcCertName, &pucSignerCert, ulSignerCertSize );

    if( ( xResult == CKR_OK ) && ( pucSignerCert != NULL ) )
    {
        PRINTF( "Using cert with PKCS11 label: %s OK\r\n", pcCertName );
    }
    else
    {
        PRINTF( "No such certificate file: %s. Using aws_ota_codesigner_certificate.h.\r\n",
                ( const char * ) pcCertName );

        /* Allocate memory for the signer certificate plus a terminating zero so we can copy it and return to the caller. */
        ulCertSize = sizeof( signingcredentialSIGNING_CERTIFICATE_PEM );
        pucSignerCert = pvPortMalloc( ulCertSize );                           /*lint !e9029 !e9079 !e838 malloc proto requires void*. */
        pucCertData = ( uint8_t * ) signingcredentialSIGNING_CERTIFICATE_PEM; /*lint !e9005 we don't modify the cert but it could be set by PKCS11 so it's not const. */

        if( pucSignerCert != NULL )
        {
            memcpy( pucSignerCert, pucCertData, ulCertSize );
            *ulSignerCertSize = ulCertSize;
        }
        else
        {
            PRINTF( "Error: No memory for storing certificate !\r\n" );
        }
    }

    return ( char * ) pucSignerCert;
}


BaseType_t xValidateImageSignature( uint8_t * pFilePath,
                                    char * pCertificatePath,
                                    uint8_t * pSignature,
                                    size_t signatureLength )
{
    void * VerificationContext;
    char * cert = NULL;
    uint32_t certsize;
    OTA_Err_t status = kOTA_Err_None;
    OTA_FileContext_t context = { 0 };
    int32_t bytesRead = 0;
    uint32_t offset = 0;

    PRINTF( "Validating the integrity of new image.\r\n" );

    cert = prvGetCertificate( pCertificatePath, &certsize );

    if( cert == NULL )
    {
        PRINTF( "Cannot get the certificate from path %s for signature validation.", pCertificatePath );
        return pdFALSE;
    }

    context.pucFilePath = pFilePath;

    status = prvPAL_OpenFileForRead( &context );

    if( status != kOTA_Err_None )
    {
        PRINTF( "Cannot open the image file for reading, error = %d.\r\n", status );
        return pdFALSE;
    }
    else
    {
        PRINTF( "Successfully opened the image file for calculating signature.\r\n" );
    }

    if( CRYPTO_SignatureVerificationStart( &VerificationContext, CRYPTO_ALGORITHM, SIGNATURE_METHOD ) != pdTRUE )
    {
        PRINTF( "Cannot initialize the signature for validation.\r\n" );
        return pdFALSE;
    }

    do
    {
        bytesRead = prvPAL_ReadBlock( &context, offset, dataBuffer, DATA_BUFFER_LENGTH );

        if( bytesRead > 0 )
        {
            CRYPTO_SignatureVerificationUpdate( VerificationContext, dataBuffer, bytesRead );
            offset += bytesRead;
        }
    } while( bytesRead == DATA_BUFFER_LENGTH );

    if( bytesRead < 0 )
    {
        PRINTF( "Failed to read bytes from image file. error = %d.\r\n", bytesRead );
        return pdFALSE;
    }

    status = prvPAL_CloseFile( &context );

    if( status != kOTA_Err_None )
    {
        PRINTF( "Failed to close the image file, error = %d.\r\n", status );
        return pdFALSE;
    }

    if( CRYPTO_SignatureVerificationFinal( VerificationContext, cert, certsize, pSignature, signatureLength ) != pdTRUE )
    {
        PRINTF( "Signature validation for the image failed.\r\n" );
        return pdFALSE;
    }

    return pdTRUE;
}
