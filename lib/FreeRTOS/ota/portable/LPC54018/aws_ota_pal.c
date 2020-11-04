/* OTA PAL implementation for NXP LPC54018 IoT Module. */

#include <string.h>

#include "aws_ota_agent_config.h"

#include "aws_iot_ota_pal.h"
#include "iot_crypto.h"
#include "core_pkcs11.h"
#include "aws_ota_codesigner_certificate.h"

#include "fsl_debug_console.h"
#include "mflash_drv.h"
#include "spifi_boot.h"


/* Specify the OTA signature algorithm we support on this platform. */
const char cOTA_JSON_FileSignatureKey[OTA_FILE_SIG_KEY_STR_MAX_LENGTH] = "sig-sha256-ecdsa";


#define OTA_IMAGE_SLOT_SIZE (0x200000)
#define OTA_UPDATE_IMAGE_ADDR (BOOT_EXEC_IMAGE_ADDR + 1 * OTA_IMAGE_SLOT_SIZE)
#define OTA_BACKUP_IMAGE_ADDR (BOOT_EXEC_IMAGE_ADDR + 2 * OTA_IMAGE_SLOT_SIZE)

#define OTA_MAX_IMAGE_SIZE (OTA_IMAGE_SLOT_SIZE)
#define OTA_UPDATE_IMAGE_PTR ((void *)OTA_UPDATE_IMAGE_ADDR)
#define OTA_BACKUP_IMAGE_PTR ((void *)OTA_BACKUP_IMAGE_ADDR)


/* low level file context structure */
typedef struct
{
    const OTA_FileContext_t *FileXRef;
    uint8_t *BaseAddr;
    uint32_t Size;
} LL_FileContext_t;


static LL_FileContext_t prvPAL_CurrentFileContext;


static CK_RV prvGetCertificateHandle( CK_FUNCTION_LIST_PTR pxFunctionList,
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
static CK_RV prvGetCertificate( const char * pcLabelName,
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
        xResult = prvGetCertificateHandle( xFunctionList, xSession, pcLabelName, &xHandle );
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


static char *prvPAL_GetCertificate( const uint8_t *pucCertName, uint32_t *ulSignerCertSize )
{
    uint8_t * pucCertData;
    uint32_t ulCertSize;
    uint8_t * pucSignerCert = NULL;
    CK_RV xResult;

    xResult = prvGetCertificate( ( const char * ) pucCertName, &pucSignerCert, ulSignerCertSize );

    if( ( xResult == CKR_OK ) && ( pucSignerCert != NULL ) )
    {
        OTA_LOG_L1(  "Using cert with label: %s OK\r\n", ( const char * ) pucCertName );
    }
    else
    {
        OTA_LOG_L1( "No such certificate file: %s. Using aws_ota_codesigner_certificate.h.\r\n",
                  ( const char * ) pucCertName );

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
        	OTA_LOG_L1( "Error: No memory for certificate in prvPAL_ReadAndAssumeCertificate!\r\n" );
        }
    }

    return ( char * ) pucSignerCert;
}


static LL_FileContext_t *prvPAL_GetLLFileContext(OTA_FileContext_t * const C)
{
    LL_FileContext_t *FileContext;

    if ((C == NULL) || (C->pucFile == NULL))
    {
        return NULL;
    }

    FileContext = (LL_FileContext_t *)C->pucFile;

    if ((FileContext == NULL) || (FileContext->FileXRef != C))
    {
        return NULL;
    }

    return FileContext;
}


static OTA_Err_t prvPAL_CheckFileSignature(OTA_FileContext_t * const C)
{
    LL_FileContext_t *FileContext;
    void *VerificationContext;

    char *cert = NULL;
    uint32_t certsize;

    OTA_LOG_L1("[OTA-NXP] CheckFileSignature\r\n");

    FileContext = prvPAL_GetLLFileContext(C);
    if (FileContext == NULL)
    {
        return kOTA_Err_SignatureCheckFailed;
    }

    cert = prvPAL_GetCertificate((const uint8_t *)C->pucCertFilepath, &certsize);
    if (cert == NULL)
    {
        return kOTA_Err_BadSignerCert;
    }

    if (CRYPTO_SignatureVerificationStart(&VerificationContext, cryptoASYMMETRIC_ALGORITHM_ECDSA, cryptoHASH_ALGORITHM_SHA256) != pdTRUE)
    {
        return kOTA_Err_SignatureCheckFailed;
    }

    CRYPTO_SignatureVerificationUpdate(VerificationContext, FileContext->BaseAddr, FileContext->Size);

    if (CRYPTO_SignatureVerificationFinal(VerificationContext, cert, certsize, C->pxSignature->ucData, C->pxSignature->usSize) != pdTRUE)
    {
        return kOTA_Err_SignatureCheckFailed;
    }

    return kOTA_Err_None;
}


OTA_Err_t prvPAL_Abort( OTA_FileContext_t * const C )
{
    OTA_Err_t result = kOTA_Err_None;

    OTA_LOG_L1("[OTA-NXP] Abort\r\n");

    C->pucFile = NULL;
    return result;
}


OTA_Err_t prvPAL_CreateFileForRx( OTA_FileContext_t * const C )
{
    LL_FileContext_t *FileContext = &prvPAL_CurrentFileContext;

    OTA_LOG_L1("[OTA-NXP] CreateFileForRx\r\n");

    if (C->ulFileSize > OTA_MAX_IMAGE_SIZE)
    {
        return kOTA_Err_RxFileTooLarge;
    }

    FileContext->FileXRef = C; /* cross reference for integrity check */
    FileContext->BaseAddr = OTA_UPDATE_IMAGE_PTR;
    FileContext->Size = 0;

    C->pucFile = (uint8_t *)FileContext;

    return kOTA_Err_None;
}


OTA_Err_t prvPAL_CloseFile( OTA_FileContext_t * const C )
{
    OTA_Err_t result = kOTA_Err_None;
    LL_FileContext_t *FileContext;

    OTA_LOG_L1("[OTA-NXP] CloseFile\r\n");

    FileContext = prvPAL_GetLLFileContext(C);
    if (FileContext == NULL)
    {
        return kOTA_Err_FileClose;
    }

    result = prvPAL_CheckFileSignature(C);
    if (result != kOTA_Err_None)
    {
        OTA_LOG_L1("[OTA-NXP] CheckFileSignature failed\r\n");
    }
    else
    {
        if (0 != boot_update_request(OTA_UPDATE_IMAGE_PTR, OTA_BACKUP_IMAGE_PTR))
        {
            result = kOTA_Err_FileClose;
        }
    }

    C->pucFile = NULL;
    return result;
}


int16_t prvPAL_WriteBlock( OTA_FileContext_t * const C, uint32_t ulOffset, uint8_t * const pcData, uint32_t ulBlockSize )
{
    int32_t result;
    LL_FileContext_t *FileContext;

    OTA_LOG_L1("[OTA-NXP] WriteBlock %x : %x\r\n", ulOffset, ulBlockSize);

    FileContext = prvPAL_GetLLFileContext(C);
    if (FileContext == NULL)
    {
        return -1;
    }

    if (ulOffset + ulBlockSize > OTA_MAX_IMAGE_SIZE)
    {
        return -1;
    }

    result = mflash_drv_write((void *)(FileContext->BaseAddr + ulOffset), pcData, ulBlockSize);
    if (result == 0)
    {
        /* zero indicates no error, return number of bytes written to the caller */
        result = ulBlockSize;
        if (FileContext->Size < ulOffset + ulBlockSize)
        {
            /* extend file size according to highest offset */
            FileContext->Size = ulOffset + ulBlockSize;
        }
    }

    return result;
}


OTA_Err_t prvPAL_ActivateNewImage( void )
{
    OTA_LOG_L1("[OTA-NXP] ActivateNewImage\r\n");
    DbgConsole_Flush();
    prvPAL_ResetDevice();
    return kOTA_Err_None;
}


OTA_Err_t prvPAL_ResetDevice( void )
{
    OTA_LOG_L1("[OTA-NXP] ResetDevice\r\n");
    boot_cpureset();
    return kOTA_Err_None;
}


OTA_Err_t prvPAL_SetPlatformImageState( OTA_ImageState_t eState )
{
    OTA_Err_t result = kOTA_Err_None;
    struct boot_ucb ucb;

    OTA_LOG_L1("[OTA-NXP] SetPlatformImageState %d\r\n", eState);

    boot_ucb_read(&ucb);

    switch (eState)
    {
    case eOTA_ImageState_Accepted:
        if (ucb.state == BOOT_STATE_PENDING_COMMIT)
        {
            ucb.state = BOOT_STATE_VOID;
            if (0 != boot_ucb_write(&ucb))
            {
                OTA_LOG_L1("[OTA-NXP] FLASH operation failed during commit\r\n");
                result = kOTA_Err_CommitFailed;
            }
            boot_wdtdis(); /* disable watchdog */
            if (0 != boot_overwrite_rollback())
            {
                /* rollback image may be partially overwritten - do not return error as that would initiate a rollback */
                OTA_LOG_L1("[OTA-NXP] FLASH operation failed during overwrite\r\n");
                ucb.rollback_img = NULL;
                if (0 != boot_ucb_write(&ucb))
                {
                    OTA_LOG_L1("[OTA-NXP] FLASH operation failed during commit\r\n");
                }
                else
                {
                    OTA_LOG_L1("[OTA-NXP] rollback disabled\r\n");
                }
            }
        }
        else
        {
            OTA_LOG_L1("[OTA-NXP] Image is not in pending commit state\r\n");
            result = kOTA_Err_CommitFailed;
        }
        break;

    case eOTA_ImageState_Rejected:
        if (ucb.state == BOOT_STATE_PENDING_COMMIT)
        {
            if (ucb.rollback_img == NULL)
            {
                OTA_LOG_L1("[OTA-NXP] Attempt to reject image without possibility for rollback\r\n");
                result = kOTA_Err_RejectFailed;
            }
            ucb.state = BOOT_STATE_INVALID;
            if (0 != boot_ucb_write(&ucb))
            {
                OTA_LOG_L1("[OTA-NXP] FLASH operation failed during reject\r\n");
                result = kOTA_Err_RejectFailed;
            }
        }
        else if (ucb.state == BOOT_STATE_NEW)
        {
            ucb.state = BOOT_STATE_VOID;
            if (0 != boot_ucb_write(&ucb))
            {
                OTA_LOG_L1("[OTA-NXP] FLASH operation failed during reject\r\n");
                result = kOTA_Err_RejectFailed;
            }
        }
        break;

    case eOTA_ImageState_Aborted:
        if (ucb.state == BOOT_STATE_PENDING_COMMIT)
        {
            if (ucb.rollback_img == NULL)
            {
                OTA_LOG_L1("[OTA-NXP] Attempt to abort without possibility for rollback\r\n");
                result = kOTA_Err_AbortFailed;
            }
            ucb.state = BOOT_STATE_INVALID;
            if (0 != boot_ucb_write(&ucb))
            {
                OTA_LOG_L1("[OTA-NXP] FLASH operation failed during abort\r\n");
                result = kOTA_Err_AbortFailed;
            }
        }
        else if (ucb.state == BOOT_STATE_NEW)
        {
            ucb.state = BOOT_STATE_VOID;
            if (0 != boot_ucb_write(&ucb))
            {
                OTA_LOG_L1("[OTA-NXP] FLASH operation failed during abort\r\n");
                result = kOTA_Err_AbortFailed;
            }
        }
        break;

    case eOTA_ImageState_Testing:
        break;

    default:
        result = kOTA_Err_BadImageState;
        break;
    }

    return result;
}


OTA_PAL_ImageState_t prvPAL_GetPlatformImageState ( void )
{
    struct boot_ucb ucb;

    OTA_LOG_L1("[OTA-NXP] GetPlatformImageState\r\n");

    boot_ucb_read(&ucb);

    switch (ucb.state)
    {
    case BOOT_STATE_NEW:
        return eOTA_PAL_ImageState_Valid;

    case BOOT_STATE_PENDING_COMMIT:
        return eOTA_PAL_ImageState_PendingCommit;

    default:
      break;
    }

    return eOTA_PAL_ImageState_Invalid;
}


/* Provide access to private members for testing. */
#ifdef AMAZON_FREERTOS_ENABLE_UNIT_TESTS
    #include "aws_ota_pal_test_access_define.h"
#endif
