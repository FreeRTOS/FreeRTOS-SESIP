/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* FreeRTOS kernel includes. */
#include "LPC54018.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

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
#include "aws_iot_ota_mqtt.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

static const uint8_t ucIPAddress[ 4 ] = { 192, 168, 1, 43 };
static const uint8_t ucNetMask[ 4 ] = { 255, 255, 255, 0 };
static const uint8_t ucGatewayAddress[ 4 ] = { 192, 168, 1, 1 };
static const uint8_t ucDNSServerAddress[ 4 ] = { 192, 168, 1, 1 };
static const uint8_t ucMACAddress[ 6 ] = { 0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x01 };

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

/* Task priorities. */
#define hello_task_PRIORITY    ( configMAX_PRIORITIES - 1 )

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void hello_task( void * pvParameters );

extern void CRYPTO_InitHardware( void );

/*******************************************************************************
 * Code
 ******************************************************************************/

uint32_t uxRand()
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

BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber )
{
    *pulNumber = uxRand();
    return pdTRUE;
}


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

        while( 1 );
    }

    xCreateRestrictedTasks( hello_task_PRIORITY );


    vTaskStartScheduler();

    while(1);
}

/*!
 * @brief Task responsible for printing of "Hello world." message.
 */

uint32_t getTimeStampMs()
{
    return 0;
}

void eventCallback( MQTTContext_t * pContext,
                    MQTTPacketInfo_t * pPacketInfo,
                    MQTTDeserializedInfo_t * pDeserializedInfo )
{

#if( OTA_UPDATE_ENABLED == 1 )

	vOTAMQTTEventCallback(pContext, pPacketInfo, pDeserializedInfo );
#endif

}


static void hello_task( void * pvParameters )
{
    MQTTContext_t mqttContext;
    TransportInterface_t transport;
    MQTTFixedBuffer_t fixedBuffer;
    uint8_t buffer[ 1024 ];
    MQTTStatus_t status;
    TlsTransportStatus_t transportStatus;
    NetworkContext_t someNetworkInterface = { 0 };
    MQTTConnectInfo_t connectInfo;
    bool sessionPresent = true;
    CK_RV xResult = CKR_OK;
    CK_ULONG ulTemp = 0;
    char * pcEndpoint = NULL;

    NetworkCredentials_t xNetworkCredentials = { 0 };

    xNetworkCredentials.pRootCa = ( const unsigned char * ) democonfigROOT_CA_PEM;
    xNetworkCredentials.rootCaSize = sizeof( democonfigROOT_CA_PEM );

    /* Configure MQTT Context */
    /* Clear context. */
    memset( ( void * ) &mqttContext, 0x00, sizeof( MQTTContext_t ) );

    while( FreeRTOS_IsNetworkUp() == pdFALSE )
    {
        PRINTF( "No Network yet\r\n" );
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }

    /* Set transport interface members. */
    transport.pNetworkContext = &someNetworkInterface;
    transport.send = TLS_FreeRTOS_send;
    transport.recv = TLS_FreeRTOS_recv;

    /* Set buffer members. */
    fixedBuffer.pBuffer = buffer;
    fixedBuffer.size = 1024;

    status = MQTT_Init( &mqttContext, &transport, getTimeStampMs, eventCallback, &fixedBuffer );

    /* Client ID must be unique to broker. This field is required. */
    xResult = ulGetThingName( &connectInfo.pClientIdentifier, &connectInfo.clientIdentifierLength );
    xResult = ulGetThingEndpoint( &pcEndpoint, &ulTemp );

    if( ( status == MQTTSuccess ) && ( xResult == CKR_OK ) )
    {
        /* True for creating a new session with broker, false if we want to resume an old one. */
        connectInfo.cleanSession = true;

        /* The following fields are optional. */
        /* Value for keep alive. */
        connectInfo.keepAliveSeconds = 60;

        /* Optional username and password. */
        connectInfo.pUserName = "";
        connectInfo.userNameLength = strlen( connectInfo.pUserName );
        connectInfo.pPassword = "";
        connectInfo.passwordLength = strlen( connectInfo.pPassword );

        FreeRTOS_debug_printf( ( "Attempting a connection\n" ) );
        transportStatus = TLS_FreeRTOS_Connect( mqttContext.transportInterface.pNetworkContext, pcEndpoint, 8883, &xNetworkCredentials, 36000, 36000 );

        if( TLS_TRANSPORT_SUCCESS == transportStatus )
        {
            /* Send the connect packet. Use 100 ms as the timeout to wait for the CONNACK packet. */
            status = MQTT_Connect( &mqttContext, &connectInfo, NULL, 100, &sessionPresent );

            if( status == MQTTSuccess )
            {

                #if( OTA_UPDATE_ENABLED == 1 )
                    xCreateOTAUpdateTask( &mqttContext );
                #endif

                for( ; ; )
                {
                    static int counter = 0;
                    char payload[ 32 ] = { 0 };

                    int payload_length = snprintf( payload, sizeof( payload ), "Hello %d", counter++ );

                    /* Since we requested a clean session, this must be false */
                    assert( sessionPresent == false );

                    /* Do something with the connection. Publish some data. */
                    MQTTPublishInfo_t info =
                    {
                        MQTTQoS0,
                        false,       /* This should not be retained */
                        false,       /* This is not a duplicate message */
                        "Test/Hello",10,
                        payload,     payload_length
                    };

                    MQTT_Publish( &mqttContext, &info, 1 );

                    vTaskDelay( pdMS_TO_TICKS( 10000 ) );
                }

                HeapStats_t heap_stats;
                vPortGetHeapStats( &heap_stats );
                FreeRTOS_debug_printf( ( "Available heap space              %d\n", heap_stats.xAvailableHeapSpaceInBytes ) );
                FreeRTOS_debug_printf( ( "Largest Free Block                %d\n", heap_stats.xSizeOfLargestFreeBlockInBytes ) );
                FreeRTOS_debug_printf( ( "Smallest Free Block               %d\n", heap_stats.xSizeOfSmallestFreeBlockInBytes ) );
                FreeRTOS_debug_printf( ( "Number of Free Blocks             %d\n", heap_stats.xNumberOfFreeBlocks ) );
                FreeRTOS_debug_printf( ( "Minimum Ever Free Bytes Remaining %d\n", heap_stats.xMinimumEverFreeBytesRemaining ) );
                FreeRTOS_debug_printf( ( "Number of Successful Allocations  %d\n", heap_stats.xNumberOfSuccessfulAllocations ) );
                FreeRTOS_debug_printf( ( "Number of Successful Frees        %d\n", heap_stats.xNumberOfSuccessfulFrees ) );
            }
        }

        else if( TLS_TRANSPORT_INVALID_PARAMETER == status )
        {
            FreeRTOS_debug_printf( ( "Error Connecting to server : bad parameter\n" ) );
        }
        else if( TLS_TRANSPORT_CONNECT_FAILURE == status )
        {
            FreeRTOS_debug_printf( ( "Error Connecting to server : connect failure\n" ) );
        }
        else
        {
            FreeRTOS_debug_printf( ( "Error Connecting to server : unknown\n" ) );
        }
    }

    /* Disconnect */
    MQTT_Disconnect( &mqttContext );

    TLS_FreeRTOS_Disconnect( mqttContext.transportInterface.pNetworkContext );

    for( ; ; )
    {
        FreeRTOS_debug_printf( ( "MQTT FAILURE\n" ) );
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

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



void vApplicationMallocFailedHook( void )
{
    PRINTF( "\n\nMALLOC FAIL\n\n" );

    for( ; ; )
    {
    }
}

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

    /* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
     * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
     * used by the Idle task. */
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
    /*-----------------------------------------------------------*/

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
    /*-----------------------------------------------------------*/
#endif /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
