/*
 * FreeRTOS version 202012.00-LTS
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
 */

/**
 * @file ota_demo_core_mqtt.c
 * @brief OTA update example using coreMQTT.
 */

/* Standard includes. */
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#include "fsl_debug_console.h"

#include "aws_application_version.h"

#include "ota_update.h"

/* MQTT include. */
#include "core_mqtt_agent.h"

#include "core_pkcs11.h"

/* OTA Library include. */
#include "ota.h"
#include "ota_config.h"

/* OTA Library Interface include. */
#include "ota_os_freertos.h"
#include "ota_mqtt_interface.h"

#include "ota_pal.h"

/* Include for getting provisioned thing name. */
#include "provision_interface.h"

/*-----------------------------------------------------------*/

/**
 *  Maximum number of reserved topics for Job subscriptions.
 */
#define MAX_OTA_TOPIC_SUBSCRIPTIONS             ( 22 )

#define OTA_STATISTICS_INTERVAL_MS              ( 5000 )

#define OTA_POLLING_DELAY_MS                    ( 1000 )

#define JOB_RESPONSE_TOPIC_FILTER               "$aws/things/+/jobs/$next/get/accepted"
#define JOB_RESPONSE_TOPIC_FILTER_LENGTH        ( 37 )


#define JOB_NOTIFICATION_TOPIC_FILTER           "$aws/things/+/jobs/notify-next"
#define JOB_NOTIFICATION_TOPIC_FILTER_LENGTH    ( 30 )

#define DATA_TOPIC_FILTER                       "$aws/things/+/streams/+/data/cbor"
#define DATA_TOPIC_FILTER_LENGTH                ( 33 )

/**
 * @brief Struct for firmware version.
 */
const AppVersion32_t appFirmwareVersion =
{
    .u.x.major = APP_VERSION_MAJOR,
    .u.x.minor = APP_VERSION_MINOR,
    .u.x.build = APP_VERSION_BUILD,
};

static TimerHandle_t otaStatsTimer = NULL;

/**
 * @brief Semaphore for synchronizing buffer operations.
 */
static SemaphoreHandle_t bufferSemaphore;

/**
 * @breif Semaphore used to synchronize MQTT operations.
 */
static SemaphoreHandle_t opSemaphore;

/**
 * @brief MQTT operation status.
 */
static MQTTStatus_t opStatus;

/**
 * @brief Update File path buffer.
 */
static uint8_t updateFilePath[ OTA_MAX_FILE_PATH_SIZE ];

/**
 * @brief Certificate File path buffer.
 */
static uint8_t certFilePath[ OTA_MAX_FILE_PATH_SIZE ];

/**
 * @brief Stream name buffer.
 */
static uint8_t streamName[ OTA_MAX_STREAM_NAME_SIZE ];

/**
 * @brief Decode memory.
 */
static uint8_t decodeMem[ otaconfigFILE_BLOCK_SIZE ];

/**
 * @brief Bitmap memory.
 */
static uint8_t bitmap[ OTA_MAX_BLOCK_BITMAP_SIZE ];

/**
 * @brief Event buffer.
 */
static OtaEventData_t eventBuffer[ otaconfigMAX_NUM_OTA_DATA_BUFFERS ];

/**
 * @brief The buffer passed to the OTA Agent from application while initializing.
 */
static OtaAppBuffer_t otaBuffer =
{
    .pUpdateFilePath    = updateFilePath,
    .updateFilePathsize = OTA_MAX_FILE_PATH_SIZE,
    .pCertFilePath      = certFilePath,
    .certFilePathSize   = OTA_MAX_FILE_PATH_SIZE,
    .pStreamName        = streamName,
    .streamNameSize     = OTA_MAX_STREAM_NAME_SIZE,
    .pDecodeMemory      = decodeMem,
    .decodeMemorySize   = otaconfigFILE_BLOCK_SIZE,
    .pFileBitmap        = bitmap,
    .fileBitmapSize     = OTA_MAX_BLOCK_BITMAP_SIZE
};

/**
 * @brief Publish message to a topic.
 *
 * This function publishes a message to a given topic & QoS.
 *
 * @param[in] pacTopic Mqtt topic filter.
 *
 * @param[in] topicLen Length of the topic filter.
 *
 * @param[in] pMsg Message to publish.
 *
 * @param[in] msgSize Message size.
 *
 * @param[in] qos Quality of Service
 *
 * @return OtaMqttSuccess if success , other error code on failure.
 */
static OtaMqttStatus_t mqttPublish( const char * const pacTopic,
                                    uint16_t topicLen,
                                    const char * pMsg,
                                    uint32_t msgSize,
                                    uint8_t qos );

/**
 * @brief Subscribe to the MQTT topic filter, and registers the handler for the topic filter with the subscription manager.
 *
 * This function subscribes to the Mqtt topics with the Quality of service
 * received as parameter. This function also registers a callback for the
 * topicfilter.
 *
 * @param[in] pTopicFilter Mqtt topic filter.
 *
 * @param[in] topicFilterLength Length of the topic filter.
 *
 * @param[in] qos Quality of Service
 *
 * @param[in] callback Callback to be registered for handling an incoming PUBLISH message on the topic.
 *
 * @return OtaMqttSuccess if success , other error code on failure.
 */
static OtaMqttStatus_t mqttSubscribe( const char * pTopicFilter,
                                      uint16_t topicFilterLength,
                                      uint8_t qos );

/**
 * @brief Unsubscribe to the Mqtt topics.
 *
 * This function unsubscribes to the Mqtt topics with the Quality of service
 * received as parameter.
 *
 * @param[in] pTopicFilter Mqtt topic filter.
 *
 * @param[in] topicFilterLength Length of the topic filter.
 *
 * @param[qos] qos Quality of Service
 *
 * @return  OtaMqttSuccess if success , other error code on failure.
 */
static OtaMqttStatus_t mqttUnsubscribe( const char * pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t qos );

/* Callbacks used to handle different events. */

/**
 * @brief Application callback invoked by OTA agent, for various events processed.
 * Application callback can be used to perform various operations like additional self tests if needed
 * for image, postponing the activation of the new image, etc..
 *
 * @param[in] event Event from OTA lib of type OtaJobEvent_t.
 * @param[in] pData Data associated with the event.
 *
 */
static void otaAppCallback( OtaJobEvent_t event,
                            const void * pData );

/**
 * @brief Callback that notifies the OTA library when a job document is received.
 *
 * @param[in] pPublishInfo MQTT packet information which stores details of the
 * job document.
 */
static void mqttJobCallback( MQTTPublishInfo_t * pPublishInfo );

/**
 * @brief Callback that notifies the OTA library when a data block is received.
 *
 * @param[in] pPublishInfo MQTT packet that stores the information of the file block.
 */
static void mqttDataCallback( MQTTPublishInfo_t * pPublishInfo );

/**
 * @brief Application callback to validate image signature before closing and commiting
 * the image.
 */
static OtaPalStatus_t appCloseFileCallback( OtaFileContext_t * const pFileContext );

static OtaInterfaces_t otaInterface =
{
    /* Initialize OTA library OS Interface. */
    .os.event.init             = OtaInitEvent_FreeRTOS,
    .os.event.send             = OtaSendEvent_FreeRTOS,
    .os.event.recv             = OtaReceiveEvent_FreeRTOS,
    .os.event.deinit           = OtaDeinitEvent_FreeRTOS,
    .os.timer.start            = OtaStartTimer_FreeRTOS,
    .os.timer.stop             = OtaStopTimer_FreeRTOS,
    .os.timer.delete           = OtaDeleteTimer_FreeRTOS,
    .os.mem.malloc             = Malloc_FreeRTOS,
    .os.mem.free               = Free_FreeRTOS,

    /* Initialize the OTA library MQTT Interface.*/
    .mqtt.subscribe            = mqttSubscribe,
    .mqtt.publish              = mqttPublish,
    .mqtt.unsubscribe          = mqttUnsubscribe,

    /* Initialize the OTA library PAL Interface.*/
    .pal.getPlatformImageState = xOtaPalGetPlatformImageState,
    .pal.setPlatformImageState = xOtaPalSetPlatformImageState,
    .pal.writeBlock            = xOtaPalWriteBlock,
    .pal.activate              = xOtaPalActivateNewImage,
    .pal.closeFile             = appCloseFileCallback,
    .pal.reset                 = xOtaPalResetDevice,
    .pal.abort                 = xOtaPalAbort,
    .pal.createFile            = xOtaPalCreateFileForRx
};

/*-----------------------------------------------------------*/


static void otaEventBufferFree( OtaEventData_t * const pxBuffer )
{
    if( xSemaphoreTake( bufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        pxBuffer->bufferUsed = false;
        ( void ) xSemaphoreGive( bufferSemaphore );
    }
    else
    {
        PRINTF( "Failed to get buffer semaphore in free" );
    }
}

/*-----------------------------------------------------------*/

OtaEventData_t * otaEventBufferGet( void )
{
    uint32_t ulIndex = 0;
    OtaEventData_t * pFreeBuffer = NULL;

    if( xSemaphoreTake( bufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        for( ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++ )
        {
            if( eventBuffer[ ulIndex ].bufferUsed == false )
            {
                eventBuffer[ ulIndex ].bufferUsed = true;
                pFreeBuffer = &eventBuffer[ ulIndex ];
                break;
            }
        }

        ( void ) xSemaphoreGive( bufferSemaphore );
    }
    else
    {
        PRINTF( "Failed to get buffer semaphore in get" );
    }

    return pFreeBuffer;
}

/*-----------------------------------------------------------*/

static void otaAppCallback( OtaJobEvent_t event,
                            const void * pData )
{
    OtaErr_t err = OtaErrUninitialized;

    /* OTA job is completed. so delete the MQTT and network connection. */
    if( event == OtaJobEventActivate )
    {
        PRINTF( "Received OtaJobEventActivate callback from OTA Agent.\r\n" );

        /* OTA job is completed. so delete the network connection. */
        /*MQTT_Disconnect( &mqttContext ); */

        /* Activate the new firmware image. */
        OTA_ActivateNewImage();

        /* We should never get here as new image activation must reset the device.*/
        PRINTF( "New image activation failed.\r\n" );

        for( ; ; )
        {
        }
    }
    else if( event == OtaJobEventFail )
    {
        PRINTF( "Received OtaJobEventFail callback from OTA Agent.\r\n" );

        /* Nothing special to do. The OTA agent handles it. */
    }
    else if( event == OtaJobEventStartTest )
    {
        /* This demo just accepts the image since it was a good OTA update and networking
         * and services are all working (or we would not have made it this far). If this
         * were some custom device that wants to test other things before validating new
         * image, this would be the place to kick off those tests before calling
         * OTA_SetImageState() with the final result of either accepted or rejected. */

        PRINTF( "Received OtaJobEventStartTest callback from OTA Agent.\r\n" );
        err = OTA_SetImageState( OtaImageStateAccepted );

        if( err != OtaErrNone )
        {
            PRINTF( " Failed to set image state as accepted.\r\n" );
        }
    }
    else if( event == OtaJobEventProcessed )
    {
        if( pData != NULL )
        {
            otaEventBufferFree( ( OtaEventData_t * ) pData );
        }
    }
}

/*-----------------------------------------------------------*/

static void mqttJobCallback( MQTTPublishInfo_t * pPublishInfo )
{
    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    pData = otaEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedJobDocument;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );
    }
    else
    {
        PRINTF( "No OTA data buffers available.\r\n" );
    }
}

/*-----------------------------------------------------------*/

static void mqttDataCallback( MQTTPublishInfo_t * pPublishInfo )
{
    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    pData = otaEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedFileBlock;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );
    }
    else
    {
        PRINTF( "No OTA data buffers available.\r\n" );
    }
}

/*-----------------------------------------------------------*/

BaseType_t xOTAProcessMQTTEvent( MQTTContext_t * pMQTTContext,
                                 struct MQTTPacketInfo * pPacketInfo,
                                 struct MQTTDeserializedInfo * pDeserializedInfo )
{
    BaseType_t result = pdFALSE;
    bool isMatched = false;

    assert( pMQTTContext != NULL );
    assert( pPacketInfo != NULL );
    assert( pDeserializedInfo != NULL );

    /* Handle incoming publish. The lower 4 bits of the publish packet
     * type is used for the dup, QoS, and retain flags. Hence masking
     * out the lower bits to check if the packet is publish. */
    if( ( pPacketInfo->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
    {
        assert( pDeserializedInfo->pPublishInfo != NULL );
        /* Handle incoming publish. */
        MQTT_MatchTopic( pDeserializedInfo->pPublishInfo->pTopicName,
                         pDeserializedInfo->pPublishInfo->topicNameLength,
                         JOB_RESPONSE_TOPIC_FILTER,
                         JOB_RESPONSE_TOPIC_FILTER_LENGTH, &isMatched );

        if( isMatched == true )
        {
            mqttJobCallback( pDeserializedInfo->pPublishInfo );
            result = pdTRUE;
        }

        if( isMatched == false )
        {
            MQTT_MatchTopic( pDeserializedInfo->pPublishInfo->pTopicName,
                             pDeserializedInfo->pPublishInfo->topicNameLength,
                             JOB_NOTIFICATION_TOPIC_FILTER,
                             JOB_NOTIFICATION_TOPIC_FILTER_LENGTH, &isMatched );

            if( isMatched == true )
            {
                mqttJobCallback( pDeserializedInfo->pPublishInfo );
                result = pdTRUE;
            }
        }

        if( isMatched == false )
        {
            MQTT_MatchTopic( pDeserializedInfo->pPublishInfo->pTopicName,
                             pDeserializedInfo->pPublishInfo->topicNameLength,
                             DATA_TOPIC_FILTER,
                             DATA_TOPIC_FILTER_LENGTH, &isMatched );

            if( isMatched == true )
            {
                mqttDataCallback( pDeserializedInfo->pPublishInfo );
                result = pdTRUE;
            }
        }
    }

    return result;
}

static void mqttOperationCallback( struct MQTTOperation * pOperation,
                                   MQTTStatus_t status )
{
    opStatus = status;
    xSemaphoreGive( opSemaphore );
}


/*-----------------------------------------------------------*/

static OtaMqttStatus_t mqttSubscribe( const char * pTopicFilter,
                                      uint16_t topicFilterLength,
                                      uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;

    MQTTSubscribeInfo_t pSubscriptionList[ 1 ] = { 0 };
    MQTTOperation_t operation = { 0 };
    BaseType_t status;

    assert( pTopicFilter != NULL );
    assert( topicFilterLength > 0 );

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    /* Set the QoS , topic and topic length. */
    pSubscriptionList[ 0 ].qos = qos;
    pSubscriptionList[ 0 ].pTopicFilter = pTopicFilter;
    pSubscriptionList[ 0 ].topicFilterLength = topicFilterLength;

    operation.type = MQTT_OP_SUBSCRIBE;
    operation.info.subscriptionInfo.pSubscriptionList = pSubscriptionList;
    operation.info.subscriptionInfo.numSubscriptions = 1;
    operation.callback = mqttOperationCallback;

    /* Send SUBSCRIBE packet. */
    status = MQTTAgent_Enqueue( &operation, portMAX_DELAY );

    if( status != pdTRUE )
    {
        PRINTF( "Failed to enqueue subscribe operation. \r\n" );
        otaRet = OtaMqttSubscribeFailed;
    }
    else
    {
        xSemaphoreTake( opSemaphore, portMAX_DELAY );

        if( opStatus != MQTTSuccess )
        {
            PRINTF( "Failed to subscribe to topic %s, error = %d.\r\n",
                    pTopicFilter,
                    opStatus );
            otaRet = OtaMqttSubscribeFailed;
        }
        else
        {
            PRINTF( "Subscribed to topic %s.\r\n",
                    pTopicFilter );
        }
    }

    return otaRet;
}

/*-----------------------------------------------------------*/

static OtaMqttStatus_t mqttPublish( const char * const pacTopic,
                                    uint16_t topicLen,
                                    const char * pMsg,
                                    uint32_t msgSize,
                                    uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    MQTTPublishInfo_t publishInfo = { 0 };
    MQTTOperation_t operation = { 0 };
    BaseType_t status;

    /* Set the required publish parameters. */
    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = qos;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    operation.type = MQTT_OP_PUBLISH;
    operation.info.pPublishInfo = &publishInfo;
    operation.callback = mqttOperationCallback;

    status = MQTTAgent_Enqueue( &operation, portMAX_DELAY );

    if( status != pdTRUE )
    {
        PRINTF( "Failed to enqueue PUBLISH operation with the agent.\r\n" );
        otaRet = OtaMqttPublishFailed;
    }
    else
    {
        xSemaphoreTake( opSemaphore, portMAX_DELAY );

        if( opStatus != MQTTSuccess )
        {
            PRINTF( "Failed to publish to topic %s, error = %d.\r\n",
                    pacTopic,
                    opStatus );
            otaRet = OtaMqttPublishFailed;
        }
        else
        {
            PRINTF( "Published to topic %s.\r\n",
                    pacTopic );
        }
    }

    return otaRet;
}

/*-----------------------------------------------------------*/

static OtaMqttStatus_t mqttUnsubscribe( const char * pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    MQTTOperation_t operation = { 0 };
    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];
    BaseType_t status;

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    /* Set the QoS , topic and topic length. */
    pSubscriptionList[ 0 ].qos = qos;
    pSubscriptionList[ 0 ].pTopicFilter = pTopicFilter;
    pSubscriptionList[ 0 ].topicFilterLength = topicFilterLength;

    operation.type = MQTT_OP_UNSUBSCRIBE;
    operation.info.subscriptionInfo.numSubscriptions = 1;
    operation.info.subscriptionInfo.pSubscriptionList = pSubscriptionList;

    /* Send UNSUBSCRIBE packet. */
    status = MQTTAgent_Enqueue( &operation, portMAX_DELAY );

    if( status != pdTRUE )
    {
        PRINTF( "Failed to enqueue UNSUBSCRIBE operation with broker.\r\n" );
        otaRet = OtaMqttUnsubscribeFailed;
    }
    else
    {
        xSemaphoreTake( opSemaphore, portMAX_DELAY );

        if( opStatus != MQTTSuccess )
        {
            PRINTF( "Failed to Unsubsribe topic %s to broker.\r\n",
                    pTopicFilter );
            otaRet = OtaMqttUnsubscribeFailed;
        }
        else
        {
            PRINTF( "Unsubsribe topic %s to broker.\r\n",
                    pTopicFilter );
        }
    }

    return otaRet;
}

/*-----------------------------------------------------------*/
static void prvOTAStatsTimerCallback( TimerHandle_t xTimer )
{
    /* OTA library packet statistics per job.*/
    OtaAgentStatistics_t otaStatistics = { 0 };

    if( OTA_GetState() != OtaAgentStateStopped )
    {
        /* Get OTA statistics for currently executing job. */
        OTA_GetStatistics( &otaStatistics );

        PRINTF( " Received: %u   Queued: %u   Processed: %u   Dropped: %u \r\n",
                otaStatistics.otaPacketsReceived,
                otaStatistics.otaPacketsQueued,
                otaStatistics.otaPacketsProcessed,
                otaStatistics.otaPacketsDropped );
    }
}

static OtaPalStatus_t appCloseFileCallback( OtaFileContext_t * const pFileContext )
{
    OtaPalStatus_t status = OtaPalSuccess;

    /* First close the file for writing. */
    status = xOtaPalCloseFile( pFileContext );

    if( status == OtaPalSuccess )
    {
        /* Validate the signature of the image. */

        if( xValidateImageSignature( pFileContext->pFilePath,
                                     ( char * ) pFileContext->pCertFilepath,
                                     pFileContext->pSignature->data,
                                     pFileContext->pSignature->size ) != pdTRUE )
        {
            status = OTA_PAL_COMBINE_ERR( OtaPalSignatureCheckFailed, 0 );
        }
        else
        {
            PRINTF( "**** OTA image signature is valid. ***** \r\n" );
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

BaseType_t xStartOTAUpdateDemo( void )
{
    BaseType_t result = pdTRUE;


    /* OTA library return status. */
    OtaErr_t otaRet = OtaErrNone;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t eventMsg = { 0 };

    char * pThingName = NULL;

    uint32_t thingNameLength;

    CK_RV pkcsllRet;

    if( ( pkcsllRet = ulGetThingName( &pThingName, &thingNameLength ) ) != CKR_OK )
    {
        PRINTF( "Cannot get thing name for initializing OTA, pkcs11 error = %d.\r\n", pkcsllRet );
        result = pdFALSE;
    }

    if( result == pdTRUE )
    {
        opSemaphore = xSemaphoreCreateBinary();

        if( opSemaphore == NULL )
        {
            result = pdFALSE;
        }
    }

    if( result == pdTRUE )
    {
        bufferSemaphore = xSemaphoreCreateMutex();

        if( bufferSemaphore == NULL )
        {
            result = pdFALSE;
        }
    }

    /****************************** Init OTA Library. ******************************/

    if( result == pdTRUE )
    {
        if( ( otaRet = OTA_Init( &otaBuffer,
                                 &otaInterface,
                                 ( const uint8_t * ) pThingName,
                                 otaAppCallback ) ) != OtaErrNone )
        {
            PRINTF( "Failed to initialize OTA, error = %u.", otaRet );
            result = pdFALSE;
        }
    }

    /****************************** Create OTA Task. ******************************/

    if( result == pdTRUE )
    {
        if( ( result = xTaskCreate( otaAgentTask,
                                    "OTA_task",
                                    otaconfigSTACK_SIZE,
                                    NULL,
                                    otaconfigTASK_PRIORITY | portPRIVILEGE_BIT,
                                    NULL ) ) != pdTRUE )
        {
            PRINTF( "Failed to create OTA Update task.\r\n" );
            result = pdFALSE;
        }
    }

    /* Start a periodic timer to report the statistics of OTA. */
    if( result == pdTRUE )
    {
        otaStatsTimer = xTimerCreate( "OTAStatsTimer",
                                      pdMS_TO_TICKS( OTA_STATISTICS_INTERVAL_MS ),
                                      pdTRUE,
                                      NULL,
                                      prvOTAStatsTimerCallback );

        if( otaStatsTimer == NULL )
        {
            result = pdFALSE;
        }
        else
        {
            xTimerStart( otaStatsTimer, portMAX_DELAY );
        }
    }

    /* Start OTA update. */
    if( result == pdTRUE )
    {
        eventMsg.eventId = OtaAgentEventStart;

        if( OTA_SignalEvent( &eventMsg ) != true )
        {
            PRINTF( "Failed to start OTA agent.\r\n" );
            result = pdFALSE;
        }
    }

    return result;
}

BaseType_t xSuspendOTAUpdate( void )
{
    OtaErr_t otaRet;
    BaseType_t result;

    /* Suspend OTA operations. */

    if( ( otaRet = OTA_Suspend() ) == OtaErrNone )
    {
        while( OTA_GetState() != OtaAgentStateSuspended )
        {
            /* Wait for OTA Library state to suspend */
            vTaskDelay( pdMS_TO_TICKS( OTA_POLLING_DELAY_MS ) );
        }
    }
    else
    {
        PRINTF( "OTA failed to suspend. StatusCode=%d.", otaRet );
        result = pdFALSE;
    }

    return result;
}

BaseType_t xResumeOTAUpdate( void )
{
    OtaErr_t otaRet;
    BaseType_t result = pdTRUE;

    /* Suspend OTA operations. */

    if( OTA_GetState() == OtaAgentStateSuspended )
    {
        if( ( otaRet = OTA_Resume() ) == OtaErrNone )
        {
            while( OTA_GetState() == OtaAgentStateSuspended )
            {
                /* Wait for OTA Library state to resume. */
                vTaskDelay( pdMS_TO_TICKS( OTA_POLLING_DELAY_MS ) );
            }
        }
        else
        {
            PRINTF( "OTA failed to resume. StatusCode=%d.", otaRet );
            result = pdFALSE;
        }
    }

    return result;
}
