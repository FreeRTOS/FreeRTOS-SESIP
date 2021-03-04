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
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/**
 *
 * @brief File shows a sample IoT application using FreeRTOS and the associated libraries.
 *
 * Application uses FreeRTOS TCP stack and mbedTLS to create a mutually authenticated TLS connection to AWS IoT.
 * It uses corePKCS11 APIs pre-provisioning credentials, thing name and performing all crypto operations using the
 * pre-provisioned credentials. Application creates a main task which loops and publishes HelloWorld messages over
 * MQTT to AWS IoT core. It creates an OTA update demo task in the background which listens for incoming OTA jobs and
 * downloads the new firmware image over MQTT protocol. The same underlying TCP connection is shared between the OTA
 * demo and the MQTT demo. A light-weight MQTT agent is implemented to show how thread safety of MQTT APIs is handled
 * across different tasks. Application also creates two restricted tasks, a Read-Write task and a Read-Only task which
 * demonstrates MPU functionalities.
 */


/* FreeRTOS kernel includes. */
#include "LPC54018.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"

#include "pin_mux.h"

#include <stdbool.h>
#include <stdio.h>


#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "NetworkInterface.h"

#include "core_mqtt.h"
#include "tls_freertos_pkcs11.h"

#include "provision_interface.h"

#include "core_pkcs11.h"
#include "pkcs11.h"

#include "user/demo-restrictions.h"
#include "ota_update.h"
#include "core_mqtt_agent.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/**
 * @brief Milliseconds per second macro used to convert ticks to milliseconds.
 */
#define MILLISECONDS_PER_SECOND    ( 1000U )

/**
 * @brief Milliseconds per FreeRTOS tick macro used to get the current tick in
 * milliseconds for MQTT library.
 *
 */
#define MILLISECONDS_PER_TICK      ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )


/**
 * @brief MQTT incoming buffer size.
 * This is the buffer size to hold an incoming packet from MQTT connection. The
 * buffer size should be set to maximum expected size as required by all MQTT applications including OTA.
 */

#define MQTT_INCOMING_BUFFER_SIZE    ( 2048 )

/**
 * @brief ROOT CA used for mutual authentication of TLS connection with AWS IoT MQTT broker.
 * Certificate is available publicly.
 *  see: https://docs.aws.amazon.com/iot/latest/developerguide/server-authentication.html
 */
#define democonfigROOT_CA_PEM                                            \
    ""                                                                   \
    "-----BEGIN CERTIFICATE-----\n"                                      \
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
    "rqXRfboQnoZsG4q5WTP468SQvvG5\n"                                     \
    "-----END CERTIFICATE-----\n"


/**
 * @brief Task priority of the MQTT Hello World task.
 */
#define hello_task_PRIORITY    ( configMAX_PRIORITIES - 1 )


/**
 * @brief MQTT hello world demo task.
 * Task creates a secure TLS connection with MQTT broker, spawns OTA demo task
 * and then keeps publishing messages in a loop at regular intervals. The task never
 * exits the loop.
 *
 * @param[in] pvParameters The parameters for hello world task.
 */
static void hello_task( void * pvParameters );

/**
 * @brief Function to return current timestamp used by MQTT library.
 * Function gets current tick value from FreeRTOS and converts it into milliseconds
 * since the start of tick.
 *
 * @return Time since start of the tick in milliseconds.
 *
 */

static uint32_t getTimeStampMs( void );

/**
 * @brief Callback executed when an MQTT packet is received by the library.
 * This application defined callback is registered with MQTT library and invoked
 * for every incoming packets. Callback first invokes MQTT agent handler function to
 * check for any general ACK packets for Subscribe/Publish etc.. If the packet is not
 * processed by MQTT agent, then its a publish packet and it calls each of the demos
 * MQTT handler functions.
 *
 * @param[in] pContext The context defined by the application passed to MQTT library.
 * @param[in] pPacketInfo Pointer to the packet info structure containing details of MQTT packet.
 * @param[in] pDeserializedInfo Pointer to structure contained deserialized publish information.
 *
 */
static void eventCallback( MQTTContext_t * pContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo );

/**
 * @brief Callback indicating a publish has been successful.
 * Callback will be invoked by MQTT agent upon successfully publishing a message and a
 * PUBACK is received from the MQTT broker for the published message.
 *
 * @param[in] pOperation Pointer to the publish operation passed to the MQTT agent.
 * @param[in] The status of the MQTT operation.
 *
 */
static void publishCompleteCallback( struct MQTTOperation * pOperation,
                                     MQTTStatus_t status );

/**
 * @brief Vendor provided function to initializes the cryptographic module.
 */
extern void CRYPTO_InitHardware( void );

/**
 * @brief Function used to dump the MPU memory regions allocated by linker script.
 */
extern void printRegions( void );


/**
 * @brief Static buffer used to receive an MQTT payload from broker.
 * The same buffer is used by all the tasks using a shared MQTT connection, to recieve the MQTT
 * payload. Hence it should be kept to the maximum expected MQTT payload size as required by all
 * applications.
 */
static uint8_t ucBuffer[ MQTT_INCOMING_BUFFER_SIZE ];


static const uint8_t ucIPAddress[ 4 ] = { 192, 168, 1, 43 };
static const uint8_t ucNetMask[ 4 ] = { 255, 255, 255, 0 };
static const uint8_t ucGatewayAddress[ 4 ] = { 192, 168, 1, 1 };
static const uint8_t ucDNSServerAddress[ 4 ] = { 192, 168, 1, 1 };
static const uint8_t ucMACAddress[ 6 ] = { 0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x01 };

/**
 * @brief Global entry time into the application to use as a reference timestamp
 * in the #prvGetTimeMs function. #prvGetTimeMs will always return the difference
 * between the current time and the global entry time. This will reduce the chances
 * of overflow for the 32 bit unsigned integer used for holding the timestamp.
 */
static uint32_t ulGlobalEntryTimeMs;

/**
 * @brief Semaphore used to synchronize publish complete callback.
 */
static SemaphoreHandle_t xPublishCompleteSemaphore;


/*******************************************************************************
 * Code
 ******************************************************************************/


/*!
 * @brief Application entry point.
 */
int main( void )
{
    /* Init board hardware. */
    CLOCK_EnableClock( kCLOCK_InputMux );

    vTraceEnable( TRC_START );

    /* attach 12 MHz clock to FLEXCOMM0 (debug console) */
    CLOCK_AttachClk( BOARD_DEBUG_UART_CLK_ATTACH );

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    CRYPTO_InitHardware();
    printRegions();

    /* Provision certificates over UART. */
    vUartProvision();

    FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );

    if( xTaskCreate( hello_task, "Hello_task", 2048, NULL, hello_task_PRIORITY | portPRIVILEGE_BIT, NULL ) !=
        pdPASS )
    {
        PRINTF( "Hello Task creation failed!.\n" );

        while( 1 )
        {
        }
    }

    xCreateRestrictedTasks( hello_task_PRIORITY );


    vTaskStartScheduler();

    while( 1 )
    {
    }
}


static uint32_t getTimeStampMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

static void eventCallback( MQTTContext_t * pContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo )
{
    BaseType_t xResult;

    xResult = MQTTAgent_ProcessEvent( pContext, pPacketInfo, pDeserializedInfo );

    #if ( OTA_UPDATE_ENABLED == 1 )
        if( xResult == pdFALSE )
        {
            xResult = xOTAProcessMQTTEvent( pContext, pPacketInfo, pDeserializedInfo );
        }
    #endif

    /* Process any application callback here. */
}

static void publishCompleteCallback( struct MQTTOperation * pOperation,
                                     MQTTStatus_t status )
{
    xSemaphoreGive( xPublishCompleteSemaphore );
}

static void hello_task( void * pvParameters )
{
    MQTTContext_t xMQTTContext = { 0 };
    TransportInterface_t xTransport = { 0 };
    MQTTFixedBuffer_t xFixedBuffer = { 0 };
    MQTTPublishInfo_t xPublishInfo = { 0 };
    MQTTOperation_t xPublishOperation = { 0 };
    bool bSessionPresent = true;
    MQTTConnectInfo_t xMQTTConnectInfo = { 0 };
    MQTTStatus_t xMQTTStatus = MQTTSuccess;


    NetworkCredentials_t xNetworkCredentials = { 0 };
    TlsTransportStatus_t xTransportStatus = TLS_TRANSPORT_SUCCESS;
    NetworkContext_t xNetworkContext = { 0 };

    HeapStats_t xHeapStats;


    CK_ULONG ulTemp = 0;
    char * pcEndpoint = NULL;
    char * pcThingName = NULL;
    uint32_t ulThingNameLength;
    CK_RV xPKCS11Result = CKR_OK;

    int32_t lCounter = 0;
    char cPayload[ 32 ] = { 0 };
    size_t xPayloadLength;

    BaseType_t xStatus;



    xNetworkCredentials.pRootCa = ( const unsigned char * ) democonfigROOT_CA_PEM;
    xNetworkCredentials.rootCaSize = sizeof( democonfigROOT_CA_PEM );

    /* Configure MQTT Context */
    /* Clear context. */
    memset( ( void * ) &xMQTTContext, 0x00, sizeof( MQTTContext_t ) );

    while( FreeRTOS_IsNetworkUp() == pdFALSE )
    {
        PRINTF( "No Network yet\r\n" );
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }

    /* Set transport interface members. */
    xTransport.pNetworkContext = &xNetworkContext;
    xTransport.send = TLS_FreeRTOS_send;
    xTransport.recv = TLS_FreeRTOS_recv;

    ulGlobalEntryTimeMs = getTimeStampMs();

    /* Set buffer members. */
    xFixedBuffer.pBuffer = ucBuffer;
    xFixedBuffer.size = MQTT_INCOMING_BUFFER_SIZE;

    xMQTTStatus = MQTT_Init( &xMQTTContext, &xTransport, getTimeStampMs, eventCallback, &xFixedBuffer );

    /* Client ID must be unique to broker. This field is required. */
    xPKCS11Result = ulGetThingName( &pcThingName, &ulThingNameLength );
    xPKCS11Result = ulGetThingEndpoint( &pcEndpoint, &ulTemp );

    if( ( xMQTTStatus == MQTTSuccess ) && ( xPKCS11Result == CKR_OK ) )
    {
        xMQTTConnectInfo.pClientIdentifier = pcThingName;

        xMQTTConnectInfo.clientIdentifierLength = ulThingNameLength;

        /* True for creating a new session with broker, false if we want to resume an old one. */
        xMQTTConnectInfo.cleanSession = true;

        /* The following fields are optional. */
        /* Value for keep alive. */
        xMQTTConnectInfo.keepAliveSeconds = 60;

        /* Optional username and password. */
        xMQTTConnectInfo.pUserName = "";
        xMQTTConnectInfo.userNameLength = strlen( xMQTTConnectInfo.pUserName );
        xMQTTConnectInfo.pPassword = "";
        xMQTTConnectInfo.passwordLength = strlen( xMQTTConnectInfo.pPassword );

        FreeRTOS_debug_printf( ( "Attempting a connection\n" ) );
        xTransportStatus = TLS_FreeRTOS_Connect( xMQTTContext.transportInterface.pNetworkContext, pcEndpoint, 8883, &xNetworkCredentials, 4000, 36000 );

        if( TLS_TRANSPORT_SUCCESS == xTransportStatus )
        {
            /* Send the connect packet. Use 100 ms as the timeout to wait for the CONNACK packet. */
            xMQTTStatus = MQTT_Connect( &xMQTTContext, &xMQTTConnectInfo, NULL, 100, &bSessionPresent );

            TLS_FreeRTOS_SetRecvTimeout( xMQTTContext.transportInterface.pNetworkContext, 500 );

            if( xMQTTStatus == MQTTSuccess )
            {
                xStatus = MQTTAgent_Init( &xMQTTContext );
                configASSERT( xStatus == pdTRUE );

                xPublishCompleteSemaphore = xSemaphoreCreateBinary();
                configASSERT( xPublishCompleteSemaphore != NULL );

                #if ( OTA_UPDATE_ENABLED == 1 )
                    xStatus = xStartOTAUpdateDemo();
                    configASSERT( xStatus == pdTRUE );
                #endif

                for( ; ; )
                {
                    xPayloadLength = snprintf( cPayload, sizeof( cPayload ), "Hello %ld", lCounter++ );

                    /* Since we requested a clean session, this must be false */
                    assert( bSessionPresent == false );

                    /* Do something with the connection. Publish some data. */
                    xPublishInfo.qos = MQTTQoS0;
                    xPublishInfo.dup = false;
                    xPublishInfo.retain = false;
                    xPublishInfo.pTopicName = "Test/Hello";
                    xPublishInfo.topicNameLength = 10;
                    xPublishInfo.pPayload = cPayload;
                    xPublishInfo.payloadLength = xPayloadLength;

                    xPublishOperation.type = MQTT_OP_PUBLISH;
                    xPublishOperation.info.pPublishInfo = &xPublishInfo;
                    xPublishOperation.callback = publishCompleteCallback;

                    MQTTAgent_Enqueue( &xPublishOperation, portMAX_DELAY );

                    xSemaphoreTake( xPublishCompleteSemaphore, portMAX_DELAY );

                    PRINTF( "Published helloworld.\r\n" );

                    vTaskDelay( pdMS_TO_TICKS( 5000 ) );
                }

                vPortGetHeapStats( &xHeapStats );
                FreeRTOS_debug_printf( ( "Available heap space              %d\n", xHeapStats.xAvailableHeapSpaceInBytes ) );
                FreeRTOS_debug_printf( ( "Largest Free Block                %d\n", xHeapStats.xSizeOfLargestFreeBlockInBytes ) );
                FreeRTOS_debug_printf( ( "Smallest Free Block               %d\n", xHeapStats.xSizeOfSmallestFreeBlockInBytes ) );
                FreeRTOS_debug_printf( ( "Number of Free Blocks             %d\n", xHeapStats.xNumberOfFreeBlocks ) );
                FreeRTOS_debug_printf( ( "Minimum Ever Free Bytes Remaining %d\n", xHeapStats.xMinimumEverFreeBytesRemaining ) );
                FreeRTOS_debug_printf( ( "Number of Successful Allocations  %d\n", xHeapStats.xNumberOfSuccessfulAllocations ) );
                FreeRTOS_debug_printf( ( "Number of Successful Frees        %d\n", xHeapStats.xNumberOfSuccessfulFrees ) );


                /* Disconnect */
                MQTT_Disconnect( &xMQTTContext );
            }

            TLS_FreeRTOS_Disconnect( xMQTTContext.transportInterface.pNetworkContext );
        }

        else if( TLS_TRANSPORT_INVALID_PARAMETER == xTransportStatus )
        {
            FreeRTOS_debug_printf( ( "Error Connecting to server : bad parameter\n" ) );
        }
        else if( TLS_TRANSPORT_CONNECT_FAILURE == xTransportStatus )
        {
            FreeRTOS_debug_printf( ( "Error Connecting to server : connect failure\n" ) );
        }
        else
        {
            FreeRTOS_debug_printf( ( "Error Connecting to server : unknown\n" ) );
        }
    }

    for( ; ; )
    {
        FreeRTOS_debug_printf( ( "Demo FAILURE\r\n" ) );
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

/**
 *  Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
 * events are only received if implemented in the MAC driver.
 */
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
    uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
    char cBuffer[ 16 ];
    static BaseType_t xTasksAlreadyCreated = pdFALSE;

    /* If the network has just come up...*/
    if( eNetworkEvent == eNetworkUp )
    {
        /* Create the tasks that use the IP stack if they have not already been
         * created. */
        if( xTasksAlreadyCreated == pdFALSE )
        {
            /* Demos that use the network are created after the network is
             * up. */
            PRINTF( ( "---------STARTING DEMO---------\r\n" ) );
            /* vStartSimpleMQTTDemo(); */
            xTasksAlreadyCreated = pdTRUE;
        }

        /* Print out the network configuration, which may have come from a DHCP
         * server. */
        FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
        FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
        PRINTF( "\r\n\r\nIP Address: %s\r\n", cBuffer );

        FreeRTOS_inet_ntoa( ulNetMask, cBuffer );
        PRINTF( "Subnet Mask: %s\r\n", cBuffer );

        FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );
        PRINTF( "Gateway Address: %s\r\n", cBuffer );

        FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
        PRINTF( "DNS Server Address: %s\r\n\r\n\r\n", cBuffer );
    }
}

/**
 * @brief Application defined random number generation function.
 *
 * The function is used by TCP/IP stack to generate initial sequence number or DHCP
 * transaction number. The implementation uses mbedTLS port of PKCS11 to generate the random
 * numbers.
 */
uint32_t uxRand( void )
{
    static CK_SESSION_HANDLE xSession = CK_INVALID_HANDLE;
    CK_RV xResult = CKR_OK;
    CK_BYTE ulBytes[ sizeof( uint32_t ) ] = { 0 };
    uint32_t ulNumber = 0;
    uint32_t i = 0;
    BaseType_t xReturn = pdFALSE;

    if( xSession == CK_INVALID_HANDLE )
    {
        xResult = xInitializePkcs11Session( &xSession );
        configASSERT( xSession != CK_INVALID_HANDLE );
        configASSERT( xResult == CKR_OK );
    }

    CK_FUNCTION_LIST_PTR pxP11FunctionList;

    xResult = C_GetFunctionList( &pxP11FunctionList );
    configASSERT( xResult == CKR_OK );

    xResult = pxP11FunctionList->C_GenerateRandom( xSession, ulBytes, sizeof( uint32_t ) );

    if( xResult != CKR_OK )
    {
        LogError( ( "Failed to generate a random number in RNG callback. "
                    "C_GenerateRandom failed with %0x.", xResult ) );
    }
    else
    {
        for( i = 0; i < sizeof( uint32_t ); i++ )
        {
            ulNumber = ( ulNumber | ( ulBytes[ i ] ) ) << i;
        }
    }

    if( xResult == CKR_OK )
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}



/*
 * Callback that provides the inputs necessary to generate a randomized TCP
 * Initial Sequence Number per RFC 6528.  THIS IS ONLY A DUMMY IMPLEMENTATION
 * THAT RETURNS A PSEUDO RANDOM NUMBER SO IS NOT INTENDED FOR USE IN PRODUCTION
 * SYSTEMS.
 */
extern uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
                                                    uint16_t usSourcePort,
                                                    uint32_t ulDestinationAddress,
                                                    uint16_t usDestinationPort )
{
    ( void ) ulSourceAddress;
    ( void ) usSourcePort;
    ( void ) ulDestinationAddress;
    ( void ) usDestinationPort;

    return uxRand();
}

/*
 * Set *pulNumber to a random number, and return pdTRUE. When the random number
 * generator is broken, it shall return pdFALSE.
 * The macros ipconfigRAND32() and configRAND32() are not in use
 * anymore in FreeRTOS+TCP.
 *
 * THIS IS ONLY A DUMMY IMPLEMENTATION THAT RETURNS A PSEUDO RANDOM NUMBER SO IS
 * NOT INTENDED FOR USE IN PRODUCTION SYSTEMS.
 */

BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber )
{
    *pulNumber = uxRand();
    return pdTRUE;
}


/**
 *
 * Called if a call to pvPortMalloc() fails because there is insufficient
 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
 * internally by FreeRTOS API functions that create tasks, queues, software
 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h.
 *
 */
void vApplicationMallocFailedHook( void )
{
    PRINTF( "\n\nMALLOC FAIL\n\n" );

    for( ; ; )
    {
    }
}


/**
 *  configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task.
 */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize )
{
    /* If the buffers to be provided to the Idle task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
 * @brief This is to provide the memory that is used by the RTOS daemon/time task.
 *
 * If configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetTimerTaskMemory() to provide the memory that is
 * used by the RTOS daemon/time task.
 */
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize )
{
    /* If the buffers to be provided to the Timer task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
