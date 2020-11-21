/* OTA PAL implementation for NXP LPC54018 IoT Module. */

#include <string.h>

#include "aws_ota_agent_config.h"

#include "aws_iot_ota_pal.h"
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

    C->pucFile = NULL;
    FileContext->FileXRef = NULL;
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

OTA_Err_t prvPAL_OpenFileForRead( OTA_FileContext_t * const pContext )
{
    LL_FileContext_t *FileContext = &prvPAL_CurrentFileContext;

    if ( FileContext->FileXRef != NULL )
    {
        return kOTA_Err_NoFreeContext;
    }

    FileContext->FileXRef = pContext; /* cross reference for integrity check */
    pContext->pucFile = (uint8_t *)FileContext;
    pContext->ulFileSize = FileContext->Size;

    return kOTA_Err_None;
}


int32_t prvPAL_ReadBlock( OTA_FileContext_t * const pContext, uint32_t ulOffset, uint8_t *pucData, uint16_t usBlockSize )
{
	uint16_t bytesToRead = usBlockSize;
	LL_FileContext_t *FileContext;

	FileContext = prvPAL_GetLLFileContext(pContext);
	if (FileContext == NULL)
	{
		return -1;
	}

	if( (ulOffset + bytesToRead) > FileContext->Size )
    {
		bytesToRead = ( FileContext->Size - ulOffset );
    }

	if( bytesToRead > 0 )
	{
		memcpy( pucData, (void *)(FileContext->BaseAddr + ulOffset), bytesToRead );
	}

    return ( int32_t ) ( bytesToRead );
}


OTA_Err_t prvPAL_ActivateNewImage( void )
{
    OTA_LOG_L1("[OTA-NXP] ActivateNewImage\r\n");

    if (0 != boot_update_request(OTA_UPDATE_IMAGE_PTR, OTA_BACKUP_IMAGE_PTR))
    {
        return kOTA_Err_ActivateFailed;
    }

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
