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

#include "ota_pal.h"
#include "fsl_debug_console.h"
#include "spifi_boot.h"
#include "mflash_drv.h"

#define OTA_IMAGE_SLOT_SIZE      ( 0x200000 )
#define OTA_UPDATE_IMAGE_ADDR    ( BOOT_EXEC_IMAGE_ADDR + 1 * OTA_IMAGE_SLOT_SIZE )
#define OTA_BACKUP_IMAGE_ADDR    ( BOOT_EXEC_IMAGE_ADDR + 2 * OTA_IMAGE_SLOT_SIZE )

#define OTA_MAX_IMAGE_SIZE       ( OTA_IMAGE_SLOT_SIZE )
#define OTA_UPDATE_IMAGE_PTR     ( ( void * ) OTA_UPDATE_IMAGE_ADDR )
#define OTA_BACKUP_IMAGE_PTR     ( ( void * ) OTA_BACKUP_IMAGE_ADDR )


/* Specify the OTA signature algorithm we support on this platform. */
const char OTA_JsonFileSignatureKey[ OTA_FILE_SIG_KEY_STR_MAX_LENGTH ] = "sig-sha256-ecdsa";

/* low level file context structure */
typedef struct
{
    const OtaFileContext_t * FileXRef;
    uint8_t * BaseAddr;
    uint32_t Size;
} LL_FileContext_t;


static LL_FileContext_t prvPAL_CurrentFileContext;

static LL_FileContext_t * prvPAL_GetLLFileContext( OtaFileContext_t * const C )
{
    LL_FileContext_t * FileContext;

    if( ( C == NULL ) || ( C->pFile == NULL ) )
    {
        return NULL;
    }

    FileContext = ( LL_FileContext_t * ) C->pFile;

    if( ( FileContext == NULL ) || ( FileContext->FileXRef != C ) )
    {
        return NULL;
    }

    return FileContext;
}


OtaPalImageState_t xOtaPalGetPlatformImageState( OtaFileContext_t * const pFileContext )
{
    struct boot_ucb ucb;

    PRINTF( "[OTA-NXP] GetPlatformImageState\r\n" );

    boot_ucb_read( &ucb );

    switch( ucb.state )
    {
        case BOOT_STATE_NEW:
            return OtaPalImageStateValid;

        case BOOT_STATE_PENDING_COMMIT:
            return OtaPalImageStatePendingCommit;

        default:
            break;
    }

    return OtaPalImageStateInvalid;
}

OtaPalStatus_t xOtaPalSetPlatformImageState( OtaFileContext_t * const pFileContext,
                                             OtaImageState_t eState )
{
    OtaPalStatus_t result = OtaPalSuccess;
    struct boot_ucb ucb;

    PRINTF( "[OTA-NXP] SetPlatformImageState %d\r\n", eState );

    boot_ucb_read( &ucb );

    switch( eState )
    {
        case OtaImageStateAccepted:

            if( ucb.state == BOOT_STATE_PENDING_COMMIT )
            {
                ucb.state = BOOT_STATE_VOID;

                if( 0 != boot_ucb_write( &ucb ) )
                {
                    PRINTF( "[OTA-NXP] FLASH operation failed during commit\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalCommitFailed, 0 );
                }

                boot_wdtdis(); /* disable watchdog */

                if( 0 != boot_overwrite_rollback() )
                {
                    /* rollback image may be partially overwritten - do not return error as that would initiate a rollback */
                    PRINTF( "[OTA-NXP] FLASH operation failed during overwrite\r\n" );
                    ucb.rollback_img = NULL;

                    if( 0 != boot_ucb_write( &ucb ) )
                    {
                        PRINTF( "[OTA-NXP] FLASH operation failed during commit\r\n" );
                    }
                    else
                    {
                        PRINTF( "[OTA-NXP] rollback disabled\r\n" );
                    }
                }
            }
            else
            {
                PRINTF( "[OTA-NXP] Image is not in pending commit state\r\n" );
                result = OTA_PAL_COMBINE_ERR( OtaPalCommitFailed, 0 );
            }

            break;

        case OtaImageStateRejected:

            if( ucb.state == BOOT_STATE_PENDING_COMMIT )
            {
                if( ucb.rollback_img == NULL )
                {
                    PRINTF( "[OTA-NXP] Attempt to reject image without possibility for rollback\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalRejectFailed, 0 );
                }

                ucb.state = BOOT_STATE_INVALID;

                if( 0 != boot_ucb_write( &ucb ) )
                {
                    PRINTF( "[OTA-NXP] FLASH operation failed during reject\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalRejectFailed, 0 );
                }
            }
            else if( ucb.state == BOOT_STATE_NEW )
            {
                ucb.state = BOOT_STATE_VOID;

                if( 0 != boot_ucb_write( &ucb ) )
                {
                    PRINTF( "[OTA-NXP] FLASH operation failed during reject\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalRejectFailed, 0 );
                }
            }

            break;

        case OtaImageStateAborted:

            if( ucb.state == BOOT_STATE_PENDING_COMMIT )
            {
                if( ucb.rollback_img == NULL )
                {
                    PRINTF( "[OTA-NXP] Attempt to abort without possibility for rollback\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalAbortFailed, 0 );
                }

                ucb.state = BOOT_STATE_INVALID;

                if( 0 != boot_ucb_write( &ucb ) )
                {
                    PRINTF( "[OTA-NXP] FLASH operation failed during abort\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalAbortFailed, 0 );
                }
            }
            else if( ucb.state == BOOT_STATE_NEW )
            {
                ucb.state = BOOT_STATE_VOID;

                if( 0 != boot_ucb_write( &ucb ) )
                {
                    PRINTF( "[OTA-NXP] FLASH operation failed during abort\r\n" );
                    result = OTA_PAL_COMBINE_ERR( OtaPalAbortFailed, 0 );
                }
            }

            break;

        case OtaImageStateTesting:
            break;

        default:
            result = OTA_PAL_COMBINE_ERR( OtaPalBadImageState, 0 );
            break;
    }

    return result;
}

OtaPalStatus_t xOtaPalResetDevice( OtaFileContext_t * const pFileContext )
{
    PRINTF( "[OTA-NXP] ResetDevice\r\n" );
    boot_cpureset();
    return OtaPalSuccess;
}

OtaPalStatus_t xOtaPalActivateNewImage( OtaFileContext_t * const pFileContext )
{
    PRINTF( "[OTA-NXP] ActivateNewImage\r\n" );

    if( 0 != boot_update_request( OTA_UPDATE_IMAGE_PTR, OTA_BACKUP_IMAGE_PTR ) )
    {
        return OTA_PAL_COMBINE_ERR( OtaPalActivateFailed, 0 );
    }

    DbgConsole_Flush();
    xOtaPalResetDevice( pFileContext );
    return OtaPalSuccess;
}

int16_t xOtaPalWriteBlock( OtaFileContext_t * const pFileContext,
                           uint32_t offset,
                           uint8_t * const pData,
                           uint32_t blockSize )
{
    int32_t result;
    LL_FileContext_t * FileContext;

    PRINTF( "[OTA-NXP] WriteBlock %x : %x\r\n", offset, blockSize );

    FileContext = prvPAL_GetLLFileContext( pFileContext );

    if( FileContext == NULL )
    {
        return -1;
    }

    result = mflash_drv_write( ( void * ) ( FileContext->BaseAddr + offset ), pData, blockSize );

    if( result == 0 )
    {
        /* zero indicates no error, return number of bytes written to the caller */
        result = blockSize;

        if( FileContext->Size < offset + blockSize )
        {
            /* extend file size according to highest offset */
            FileContext->Size = offset + blockSize;
        }
    }

    return result;
}
OtaPalStatus_t xOtaPalCloseFile( OtaFileContext_t * const pFileContext )
{
    OtaPalStatus_t result = OtaPalSuccess;
    LL_FileContext_t * FileContext;

    PRINTF( "[OTA-NXP] CloseFile\r\n" );

    FileContext = prvPAL_GetLLFileContext( pFileContext );

    if( FileContext == NULL )
    {
        return OTA_PAL_COMBINE_ERR( OtaPalFileClose, 0 );
    }

    pFileContext->pFile = NULL;
    FileContext->FileXRef = NULL;
    return result;
}

OtaPalStatus_t xOtaPalCreateFileForRx( OtaFileContext_t * const pFileContext )
{
    LL_FileContext_t * FileContext = &prvPAL_CurrentFileContext;

    PRINTF( "[OTA-NXP] CreateFileForRx\r\n" );

    if( pFileContext->fileSize > OTA_MAX_IMAGE_SIZE )
    {
        return OTA_PAL_COMBINE_ERR( OtaPalRxFileTooLarge, 0 );
    }

    FileContext->FileXRef = pFileContext; /* cross reference for integrity check */
    FileContext->BaseAddr = OTA_UPDATE_IMAGE_PTR;
    FileContext->Size = 0;

    pFileContext->pFile = ( uint8_t * ) FileContext;

    return OtaPalSuccess;
}



OtaPalStatus_t xOtaPalAbort( OtaFileContext_t * const pFileContext )
{
    OtaPalStatus_t result = OtaPalSuccess;

    PRINTF( "[OTA-NXP] Abort\r\n" );

    pFileContext->pFile = NULL;
    return result;
}

OtaPalStatus_t xOtaPalOpenFileForRead( OtaFileContext_t * const pContext )
{
    LL_FileContext_t * FileContext = &prvPAL_CurrentFileContext;


    FileContext->FileXRef = pContext; /* cross reference for integrity check */
    pContext->pFile = ( uint8_t * ) FileContext;
    pContext->fileSize = FileContext->Size;

    return OtaPalSuccess;
}


int32_t xOtaPalReadBlock( OtaFileContext_t * const pContext,
                          uint32_t offset,
                          uint8_t * pData,
                          uint16_t blockSize )
{
    uint16_t bytesToRead = blockSize;
    LL_FileContext_t * FileContext;

    FileContext = prvPAL_GetLLFileContext( pContext );

    if( FileContext == NULL )
    {
        return -1;
    }

    if( ( offset + bytesToRead ) > FileContext->Size )
    {
        bytesToRead = ( FileContext->Size - offset );
    }

    if( bytesToRead > 0 )
    {
        memcpy( pData, ( void * ) ( FileContext->BaseAddr + offset ), bytesToRead );
    }

    return ( int32_t ) ( bytesToRead );
}
