/*
 * FreeRTOS Provision v0.0.1
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

/* Logging includes. */
#include "logging_levels.h"
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "PROVISION"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif
#include "logging_stack.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard include. */
#include "stdio.h"

/* PKCS #11 includes. */
#include "core_pkcs11_config.h"
#include "core_pkcs11.h"
#include "core_pki_utils.h"
#include "pkcs11.h"

/* mbed TLS includes. */
#include "mbedtls/x509_csr.h"
#include "mbedtls/pk.h"
#include "mbedtls/pk_internal.h"

/* Custom mbedtls utils include. */
#include "mbedtls_error.h"

/* Provisioning innclude. */
#include "provision.h"

/**
 * @brief Size of buffer to use for a generated CSR.
 */
#define CSR_BUF_SIZE ( 4096UL )

/**
 * @brief Represents string to be logged when mbedTLS returned error
 * does not contain a high-level code.
 */
    static const char * pNoHighLevelMbedTlsCodeStr = "<No-High-Level-Code>";

/**
 * @brief Represents string to be logged when mbedTLS returned error
 * does not contain a low-level code.
 */
    static const char * pNoLowLevelMbedTlsCodeStr = "<No-Low-Level-Code>";

/**
 * @brief Utility for converting the high-level code in an mbedTLS error to string,
 * if the code-contains a high-level code; otherwise, using a default string.
 */
    #define mbedtlsHighLevelCodeOrDefault( mbedTlsCode )    \
    ( mbedtls_strerror_highlevel( mbedTlsCode ) != NULL ) ? \
    mbedtls_strerror_highlevel( mbedTlsCode ) : pNoHighLevelMbedTlsCodeStr

/**
 * @brief Utility for converting the level-level code in an mbedTLS error to string,
 * if the code-contains a level-level code; otherwise, using a default string.
 */
    #define mbedtlsLowLevelCodeOrDefault( mbedTlsCode )    \
    ( mbedtls_strerror_lowlevel( mbedTlsCode ) != NULL ) ? \
    mbedtls_strerror_lowlevel( mbedTlsCode ) : pNoLowLevelMbedTlsCodeStr

static CK_RV xCreateDeviceKeyPair( CK_SESSION_HANDLE xSession,
                                   uint8_t * pucPrivateKeyLabel,
                                   uint8_t * pucPublicKeyLabel,
                                   CK_OBJECT_HANDLE_PTR pxPrivateKeyHandle,
                                   CK_OBJECT_HANDLE_PTR pxPublicKeyHandle )
{
    CK_RV xResult;
    CK_MECHANISM xMechanism =
    {
        CKM_EC_KEY_PAIR_GEN, NULL_PTR, 0
    };
    CK_FUNCTION_LIST_PTR pxFunctionList;
    CK_BYTE xEcParams[] = pkcs11DER_ENCODED_OID_P256; /* prime256v1 */
    CK_KEY_TYPE xKeyType = CKK_EC;

    CK_BBOOL xTrue = CK_TRUE;
    CK_ATTRIBUTE xPublicKeyTemplate[] =
    {
        { CKA_KEY_TYPE,  &xKeyType, sizeof( xKeyType )                           },
        { CKA_VERIFY,    &xTrue,    sizeof( xTrue )                              },
        { CKA_EC_PARAMS, xEcParams, sizeof( xEcParams )                          },
        { CKA_LABEL,     pucPublicKeyLabel,    strlen( ( const char * ) pucPublicKeyLabel ) }
    };

    CK_ATTRIBUTE xPrivateKeyTemplate[] =
    {
        { CKA_KEY_TYPE, &xKeyType, sizeof( xKeyType )                            },
        { CKA_TOKEN,    &xTrue,    sizeof( xTrue )                               },
        { CKA_PRIVATE,  &xTrue,    sizeof( xTrue )                               },
        { CKA_SIGN,     &xTrue,    sizeof( xTrue )                               },
        { CKA_LABEL,    pucPrivateKeyLabel,   strlen( ( const char * ) pucPrivateKeyLabel ) }
    };

    LogInfo( ( "Creating an EC Key Pair." ) );

    xResult = C_GetFunctionList( &pxFunctionList );
    if( xResult == CKR_OK )
    {
        xResult = pxFunctionList->C_GenerateKeyPair( xSession,
                                                     &xMechanism,
                                                     xPublicKeyTemplate,
                                                     sizeof( xPublicKeyTemplate ) / sizeof( CK_ATTRIBUTE ),
                                                     xPrivateKeyTemplate, sizeof( xPrivateKeyTemplate ) / sizeof( CK_ATTRIBUTE ),
                                                     pxPublicKeyHandle,
                                                     pxPrivateKeyHandle );
        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to generate an EC Key Pair. C_GenerateKeyPair failed with %0x.", xResult ) );
        }
    }
    else
    {
        LogError( ( "Failed to generate an EC Key Pair. Could not get function list pointer." ) );

    }

    return xResult;
}

static int32_t privateKeySigningCallback( void * pvContext,
                                          mbedtls_md_type_t xMdAlg,
                                          const unsigned char * pucHash,
                                          size_t xHashLen,
                                          unsigned char * pucSig,
                                          size_t * pxSigLen,
                                          int32_t ( * piRng )( void *,
                                                               unsigned char *,
                                                               size_t ),
                                          void * pvRng )
{
    CK_RV xResult = CKR_OK;
    int32_t lFinalResult = 0;
    CK_MECHANISM xMech = { 0 };
    xMech.mechanism = CKM_ECDSA;
    CK_FUNCTION_LIST_PTR pxP11FunctionList;
    char pcPrivateKeyLabel[] = { pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS };
    CK_SESSION_HANDLE xPrivateKeyHandle;
    CK_SESSION_HANDLE xSession;


    xResult = C_GetFunctionList( &pxP11FunctionList );
    if( xResult != CKR_OK )
    {
        LogError( ( "Failed to sign callback hash. Could not get a "
                    "PKCS #11 function pointer." ) );
    }

    if( xResult == CKR_OK )
    {
        xResult = xInitializePkcs11Session( &xSession );
        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to sign callback hash. Could not initialize "
                        "a PKCS #11 session." ) );
        }
    }

    if( xResult == CKR_OK )
    {
        xResult = xFindObjectWithLabelAndClass( xSession, pcPrivateKeyLabel, 
                        CKO_CERTIFICATE, &xPrivateKeyHandle );
        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to sign callback hash. Could not find private key "
                        "object handle." ) );
        }
    }

    /* Unreferenced parameters. */
    ( void ) pvContext;
    ( void ) ( piRng );
    ( void ) ( pvRng );
    ( void ) ( xMdAlg );

    if( CKR_OK == xResult )
    {
        xResult = pxP11FunctionList->C_SignInit( xSession,
                                                 &xMech,
                                                 xPrivateKeyHandle );
    }

    if( CKR_OK == xResult )
    {
        xResult = pxP11FunctionList->C_Sign( xSession,
                                             pucHash,
                                             xHashLen,
                                             pucSig,
                                             ( CK_ULONG_PTR ) pxSigLen );
    }

    /* PKCS #11 for P256 returns a 64-byte signature with 32 bytes for R and 32 bytes for S.
     * This must be converted to an ASN.1 encoded array. */
    if( *pxSigLen != pkcs11ECDSA_P256_SIGNATURE_LENGTH )
    {
        xResult = CKR_FUNCTION_FAILED;
        LogError( ( "Failed to sign message using PKCS #11. Expected signature "
                    "length of %ul, but received %ul.", pkcs11ECDSA_P256_SIGNATURE_LENGTH, *pxSigLen ) );
    }

    if( xResult == CKR_OK )
    {
        PKI_pkcs11SignatureTombedTLSSignature( pucSig, pxSigLen );
    }

    if( xResult != CKR_OK )
    {
        LogError( ( "Failed to sign message using PKCS #11 with error code %02X.", xResult ) );
    }

    if( xResult != CKR_OK )
    {
        lFinalResult = -1;
    }

    return lFinalResult;
}

/*-----------------------------------------------------------*/

static int prvRandom ( void * pvCtx,
                    unsigned char * pucRandom,
                    size_t xRandomLength )
{
    CK_SESSION_HANDLE * pxSession = ( CK_SESSION_HANDLE * ) pvCtx; 
    CK_RV xResult;
    CK_FUNCTION_LIST_PTR pxP11FunctionList;

    xResult = C_GetFunctionList( &pxP11FunctionList );
    if( xResult != CKR_OK )
    {
        LogError( ( "Failed to generate a random number in RNG callback. Could not get a "
                    "PKCS #11 function pointer." ) );
    }
    else
    {
        xResult = pxP11FunctionList->C_GenerateRandom( *pxSession, pucRandom, xRandomLength );
        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to generate a random number in RNG callback. "
                        "C_GenerateRandom failed with %0x.", xResult ) );
        }
    }

    return xResult;
}

static int prvExtractEcPublicKey( mbedtls_ecdsa_context * pxEcdsaContext, CK_OBJECT_HANDLE xPublicKey )
{
    CK_ATTRIBUTE xTemplate;
    int lMbedResult = 0;
    CK_RV xResult = CKR_OK;
    CK_SESSION_HANDLE xSession;
    CK_BYTE xEcPoint[ 256 ] = { 0 };
    CK_FUNCTION_LIST_PTR pxP11FunctionList;

    mbedtls_ecdsa_init( pxEcdsaContext );
    mbedtls_ecp_group_init( &( pxEcdsaContext->grp ) );


    xResult = C_GetFunctionList( &pxP11FunctionList );
    if( xResult != CKR_OK )
    {
        LogError( ( "Failed to extract EC public key. Could not get a "
                    "PKCS #11 function pointer." ) );
    }
    else
    {
        xResult = xInitializePkcs11Session( &xSession );
        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to extract EC public key. Could not initialize "
                        "a PKCS #11 session." ) );
        }
    }

    if( xResult == CKR_OK )
    {
        xTemplate.type = CKA_EC_POINT;
        xTemplate.pValue = xEcPoint;
        xTemplate.ulValueLen = sizeof( xEcPoint );
        xResult = pxP11FunctionList->C_GetAttributeValue( xSession, xPublicKey, &xTemplate, 1 );
        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to extract EC public key. Could not get attribute value. "
                        "C_GetAttributeValue failed with %0x.", xResult ) );
        }
    }
    
    if( xResult == CKR_OK )
    {
        lMbedResult = mbedtls_ecp_group_load( &( pxEcdsaContext->grp ), MBEDTLS_ECP_DP_SECP256R1 );
        if( lMbedResult != 0 )
        {
            LogError( ( "Failed creating an EC key. "
                        "mbedtls_ecp_group_load failed: mbed "
                        "TLS error = %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( lMbedResult ),
                        mbedtlsLowLevelCodeOrDefault( lMbedResult ) ) );
            xResult = CKR_FUNCTION_FAILED;
        }
        else
        {
            lMbedResult  = mbedtls_ecp_point_read_binary( &( pxEcdsaContext->grp ), &( pxEcdsaContext->Q ), &xEcPoint[ 2 ], xTemplate.ulValueLen - 2 );

            if( lMbedResult != 0 )
            {
                LogError( ( "Failed creating an EC key. "
                            "mbedtls_ecp_group_load failed: mbed "
                            "TLS error = %s : %s.",
                            mbedtlsHighLevelCodeOrDefault( lMbedResult ),
                            mbedtlsLowLevelCodeOrDefault( lMbedResult ) ) );
                xResult = CKR_FUNCTION_FAILED;
            }
        }
    }

    return lMbedResult;
}

static CK_RV prvDestroyProvidedObjects( CK_SESSION_HANDLE xSession,
                               CK_BYTE_PTR * ppxPkcsLabels,
                               CK_OBJECT_CLASS * xClass,
                               CK_ULONG ulCount )
{
    CK_RV xResult;
    CK_FUNCTION_LIST_PTR pxFunctionList;
    CK_OBJECT_HANDLE xObjectHandle;
    CK_BYTE * pxLabel;
    CK_ULONG uiIndex = 0;

    xResult = C_GetFunctionList( &pxFunctionList );

    for( uiIndex = 0; uiIndex < ulCount; uiIndex++ )
    {
        pxLabel = ppxPkcsLabels[ uiIndex ];

        xResult = xFindObjectWithLabelAndClass( xSession,
                                                ( char * ) pxLabel,
                                                xClass[ uiIndex ],
                                                &xObjectHandle );

        while( ( xResult == CKR_OK ) && ( xObjectHandle != CK_INVALID_HANDLE ) )
        {
            xResult = pxFunctionList->C_DestroyObject( xSession, xObjectHandle );

            /* PKCS #11 allows a module to maintain multiple objects with the same
             * label and type. The intent of this loop is to try to delete all of them.
             * However, to avoid getting stuck, we won't try to find another object
             * of the same label/type if the previous delete failed. */
            if( xResult == CKR_OK )
            {
                xResult = xFindObjectWithLabelAndClass( xSession,
                                                        ( char * ) pxLabel,
                                                        xClass[ uiIndex ],
                                                        &xObjectHandle );
            }
            else
            {
                break;
            }
        }

        if( xResult == CKR_FUNCTION_NOT_SUPPORTED )
        {
            break;
        }
    }

    return xResult;
}

static int prvSetupCsrCtx( mbedtls_x509write_csr * pxCtx )
{
    int lMbedResult = 0;

    mbedtls_x509write_csr_init( pxCtx );
    mbedtls_x509write_csr_set_md_alg( pxCtx, MBEDTLS_MD_SHA256 );

    lMbedResult = mbedtls_x509write_csr_set_key_usage( pxCtx, MBEDTLS_X509_KU_DIGITAL_SIGNATURE );
    configASSERT( lMbedResult == 0 );

    lMbedResult = mbedtls_x509write_csr_set_ns_cert_type( pxCtx, MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT );
    configASSERT( lMbedResult == 0 );

    lMbedResult = mbedtls_x509write_csr_set_subject_name( pxCtx, (const char *) "CN=TestSubject" );
    configASSERT( lMbedResult == 0 );
    
    return lMbedResult;
}

uint8_t * vCreateCsr( void )
{
    /* PKCS #11 variables. */
    CK_RV xResult = CKR_OK;
    CK_OBJECT_HANDLE xPrivateKey;
    CK_OBJECT_HANDLE xPublicKey;
    CK_FUNCTION_LIST_PTR pxP11FunctionList;
    CK_SESSION_HANDLE xSession;

    /* CSR output buffer. */
    uint8_t * pucCsrBuf = NULL;

    /* mbed TLS variables. */
    int lMbedResult = 0;
    mbedtls_ecdsa_context xEcdsaContext;
    mbedtls_pk_context privKey;
    mbedtls_pk_info_t privKeyInfo;
    const mbedtls_pk_info_t *header = mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY);
    mbedtls_x509write_csr req;

    xResult = C_GetFunctionList( &pxP11FunctionList );
    configASSERT( xResult == CKR_OK );

    xResult = xInitializePkcs11Session( &xSession );
    configASSERT( xResult == CKR_OK );

    lMbedResult = prvSetupCsrCtx(&req);
    configASSERT( lMbedResult == 0 );

    mbedtls_pk_init( &privKey );

    lMbedResult =  mbedtls_pk_setup(&privKey, header);
    configASSERT( lMbedResult == 0 );

    xResult = xCreateDeviceKeyPair( xSession,
                                           ( uint8_t * ) pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS,
                                           ( uint8_t * ) pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS,
                                           &xPrivateKey,
                                           &xPublicKey );
    configASSERT( xResult == CKR_OK );

    lMbedResult = prvExtractEcPublicKey( &xEcdsaContext, xPublicKey );
    configASSERT( lMbedResult == 0 );

    memcpy(&privKeyInfo, privKey.pk_info, sizeof(mbedtls_pk_info_t));

    privKeyInfo.sign_func = privateKeySigningCallback;
    privKey.pk_info = &privKeyInfo;
    privKey.pk_ctx = &xEcdsaContext;

    mbedtls_x509write_csr_set_key(&req, &privKey);

    pucCsrBuf = ( uint8_t * ) pvPortMalloc(CSR_BUF_SIZE);
    configASSERT( lMbedResult == 0 );

    lMbedResult = mbedtls_x509write_csr_pem( &req, ( unsigned char *) pucCsrBuf, CSR_BUF_SIZE, &prvRandom, &xSession );
    configASSERT( lMbedResult == 0 );

    xResult = pxP11FunctionList->C_CloseSession( xSession );
    configASSERT( xResult == CKR_OK );

    pxP11FunctionList->C_Finalize( NULL );

    return pucCsrBuf;
}

CK_RV xProvisionCert( CK_BYTE_PTR xCert, CK_ULONG xCertLen )
{
    CK_FUNCTION_LIST_PTR pxFunctionList;
    CK_RV xResult;
    CK_SESSION_HANDLE xSession;
    uint8_t * pucDerObject = NULL;
    int32_t lConversionReturn = 0;
    size_t xDerLen = 0;
    CK_BBOOL xTokenStorage = CK_TRUE;
    CK_OBJECT_HANDLE xObjectHandle = CK_INVALID_HANDLE;

    PKCS11_CertificateTemplate_t xCertificateTemplate;
    CK_OBJECT_CLASS xCertificateClass = CKO_CERTIFICATE;
    CK_CERTIFICATE_TYPE xCertificateType = CKC_X_509;

    CK_BYTE xSubject[] = "TestSubject";

    /* Initialize the client certificate template. */
    xCertificateTemplate.xObjectClass.type = CKA_CLASS;
    xCertificateTemplate.xObjectClass.pValue = &xCertificateClass;
    xCertificateTemplate.xObjectClass.ulValueLen = sizeof( xCertificateClass );
    xCertificateTemplate.xSubject.type = CKA_SUBJECT;
    xCertificateTemplate.xSubject.pValue = xSubject;
    xCertificateTemplate.xSubject.ulValueLen = strlen( ( const char * ) xSubject );
    xCertificateTemplate.xValue.type = CKA_VALUE;
    xCertificateTemplate.xValue.pValue = ( CK_VOID_PTR ) xCert;
    xCertificateTemplate.xValue.ulValueLen = ( CK_ULONG ) xCertLen;
    xCertificateTemplate.xLabel.type = CKA_LABEL;
    xCertificateTemplate.xLabel.pValue = ( CK_VOID_PTR ) pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS;
    xCertificateTemplate.xLabel.ulValueLen = strlen( pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS );
    xCertificateTemplate.xCertificateType.type = CKA_CERTIFICATE_TYPE;
    xCertificateTemplate.xCertificateType.pValue = &xCertificateType;
    xCertificateTemplate.xCertificateType.ulValueLen = sizeof( CK_CERTIFICATE_TYPE );
    xCertificateTemplate.xTokenObject.type = CKA_TOKEN;
    xCertificateTemplate.xTokenObject.pValue = &xTokenStorage;
    xCertificateTemplate.xTokenObject.ulValueLen = sizeof( xTokenStorage );

    xResult = C_GetFunctionList( &pxFunctionList );
    configASSERT( xResult == CKR_OK );

    xResult = xInitializePkcs11Session( &xSession );
    configASSERT( xResult == CKR_OK );

    /* Convert the certificate to DER format if it was in PEM. The DER key
     * should be about 3/4 the size of the PEM key, so mallocing the PEM key
     * size is sufficient. */
    pucDerObject = pvPortMalloc( xCertificateTemplate.xValue.ulValueLen );
    xDerLen = xCertificateTemplate.xValue.ulValueLen;

    if( pucDerObject != NULL )
    {
        lConversionReturn = convert_pem_to_der( xCertificateTemplate.xValue.pValue,
                                                xCertificateTemplate.xValue.ulValueLen,
                                                pucDerObject,
                                                &xDerLen );

        if( 0 != lConversionReturn )
        {
            xResult = CKR_ARGUMENTS_BAD;
        }
    }
    else
    {
        xResult = CKR_HOST_MEMORY;
    }

    if( xResult == CKR_OK )
    {
        /* Set the template pointers to refer to the DER converted objects. */
        xCertificateTemplate.xValue.pValue = pucDerObject;
        xCertificateTemplate.xValue.ulValueLen = xDerLen;
    }

    /* Create an object using the encoded client certificate. */
    if( xResult == CKR_OK )
    {
        LogInfo( ( "Write certificate...\r\n" ) );

        xResult = pxFunctionList->C_CreateObject( xSession,
                                                  ( CK_ATTRIBUTE_PTR ) &xCertificateTemplate,
                                                  sizeof( xCertificateTemplate ) / sizeof( CK_ATTRIBUTE ),
                                                  &xObjectHandle );
    }

    if( pucDerObject != NULL )
    {
        vPortFree( pucDerObject );
    }

    return xResult;

}

CK_RV xCheckIfProvisioned( void )
{
    CK_RV xResult = CKR_OK;
    CK_SESSION_HANDLE xSession;
    CK_OBJECT_HANDLE xObject;
    CK_FUNCTION_LIST_PTR pxP11FunctionList;
    
    char pcCertificateLabel[] = { pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS };
    char pcPrivateKeyLabel[] = { pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS };

    xResult = C_GetFunctionList( &pxP11FunctionList );
    configASSERT( xResult == CKR_OK );

    xResult = xInitializePkcs11Session( &xSession );
    configASSERT( xResult == CKR_OK );
    
    xResult = xFindObjectWithLabelAndClass( xSession, pcCertificateLabel, CKO_CERTIFICATE, &xObject );
    if( ( xResult == CKR_OK ) && ( xObject != CK_INVALID_HANDLE ) )
    {
        xResult = xFindObjectWithLabelAndClass( xSession, pcPrivateKeyLabel, CKO_PRIVATE_KEY, &xObject );
        configASSERT( xObject != CK_INVALID_HANDLE );
    }
    
    if( xObject == CK_INVALID_HANDLE )
    {
        xResult = CKR_SESSION_HANDLE_INVALID;
        LogInfo( ( "Could not find existing credentials. "
                    "Device was not already provisioned or memory has been erased." ) );
    }
    else
    {
        LogInfo( ( "Device has existing credentials." ) );
    }

    return xResult;
}

CK_RV xDestroyCryptoObjects( void )
{
    CK_RV xResult;
    CK_SESSION_HANDLE xSession;
    xResult = xInitializePkcs11Session( &xSession );
    configASSERT( xResult == CKR_OK );

    CK_BYTE * pxPkcsLabels[] =
    {
        ( CK_BYTE * ) pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS,
        ( CK_BYTE * ) pkcs11configLABEL_CODE_VERIFICATION_KEY,
        ( CK_BYTE * ) pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS,
        ( CK_BYTE * ) pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS
    };
    CK_OBJECT_CLASS xClass[] =
    {
        CKO_CERTIFICATE,
        CKO_PUBLIC_KEY,
        CKO_PRIVATE_KEY,
        CKO_PUBLIC_KEY
    };

    xResult = prvDestroyProvidedObjects( xSession,
                                       pxPkcsLabels,
                                       xClass,
                                       sizeof( xClass ) / sizeof( CK_OBJECT_CLASS ) );

    return xResult;
}
