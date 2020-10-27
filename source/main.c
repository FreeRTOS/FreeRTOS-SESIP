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
#include "plaintext_freertos.h"

#include "provision_interface.h"

#include "core_pkcs11.h"
#include "pkcs11.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

static const uint8_t ucIPAddress[ 4 ] = { 192,168,86,43 };
static const uint8_t ucNetMask[ 4 ] = { 255,255,255,0 };
static const uint8_t ucGatewayAddress[ 4 ] = { 192,168,86,1 };
static const uint8_t ucDNSServerAddress[ 4 ] = { 192,168,86,1 };
static const uint8_t ucMACAddress[6] = { 0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x01};

/* Task priorities. */
#define hello_task_PRIORITY (configMAX_PRIORITIES - 1)
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void hello_task(void *pvParameters);

extern void CRYPTO_InitHardware(void);

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
        for(i = 0; i < sizeof( uint32_t ); i++ )
        {
            ulNumber = ( ulNumber | ( ulBytes[i] ) ) << i;
        }
    }

    if( xResult == CKR_OK )
    {
        xReturn = pdTRUE;
    }
	return xReturn;
}

BaseType_t xApplicationGetRandomNumber( uint32_t *pulNumber )
{
	*pulNumber = uxRand();
	return pdTRUE;
}


/*!
 * @brief Application entry point.
 */
int main(void)
{
    /* Init board hardware. */
    CLOCK_EnableClock(kCLOCK_InputMux);

    /* attach 12 MHz clock to FLEXCOMM0 (debug console) */
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    CRYPTO_InitHardware();

    /* Provision certificates over UART. */
    vUartProvision();

    FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );

    if (xTaskCreate(hello_task, "Hello_task", 2048, NULL, hello_task_PRIORITY, NULL) !=
        pdPASS)
    {
        PRINTF("Hello Task creation failed!.\n");
        while (1)
            ;
    }
    vTaskStartScheduler();
    for (;;)
        ;
}

/*!
 * @brief Task responsible for printing of "Hello world." message.
 */

uint32_t getTimeStampMs()
{
	return 0;
}

void eventCallback( MQTTContext_t * pContext, MQTTPacketInfo_t * pPacketInfo, MQTTDeserializedInfo_t * pDeserializedInfo )
{

}


static void hello_task(void *pvParameters)
{

	MQTTContext_t mqttContext;
	TransportInterface_t transport;
	MQTTFixedBuffer_t fixedBuffer;
	uint8_t buffer[ 1024 ];
	MQTTStatus_t status;
	NetworkContext_t someNetworkInterface;
	MQTTConnectInfo_t connectInfo;
	bool sessionPresent = true;

	// Configure MQTT Context
	// Clear context.
	memset( ( void * ) &mqttContext, 0x00, sizeof( MQTTContext_t ) );

	while(FreeRTOS_IsNetworkUp() == pdFALSE)
	{
		PRINTF("No Network yet\r\n");
		vTaskDelay(pdMS_TO_TICKS(500));
	}


	// Set transport interface members.
	transport.pNetworkContext = &someNetworkInterface;
	transport.send = Plaintext_FreeRTOS_send;
	transport.recv = Plaintext_FreeRTOS_recv;

	// Set buffer members.
	fixedBuffer.pBuffer = buffer;
	fixedBuffer.size = 1024;

	status = MQTT_Init( &mqttContext, &transport, getTimeStampMs, eventCallback, &fixedBuffer );

	if( status == MQTTSuccess )
	{
	    for (;;)
	    {
			// True for creating a new session with broker, false if we want to resume an old one.
			connectInfo.cleanSession = true;

			// Client ID must be unique to broker. This field is required.
			connectInfo.pClientIdentifier = "MyThingName";
			connectInfo.clientIdentifierLength = strlen( connectInfo.pClientIdentifier );

			// The following fields are optional.
			// Value for keep alive.
			connectInfo.keepAliveSeconds = 60;

			// Optional username and password.
			connectInfo.pUserName = "";
			connectInfo.userNameLength = strlen( connectInfo.pUserName );
			connectInfo.pPassword = "";
			connectInfo.passwordLength = strlen( connectInfo.pPassword );

			FreeRTOS_debug_printf(("Attempting a connection\n"));
			PlaintextTransportStatus_t status = Plaintext_FreeRTOS_Connect(mqttContext.transportInterface.pNetworkContext,"test.mosquitto.org",1883,36000,36000);
			if (PLAINTEXT_TRANSPORT_SUCCESS == status)
			{
				// Send the connect packet. Use 100 ms as the timeout to wait for the CONNACK packet.
				status = MQTT_Connect( &mqttContext, &connectInfo, NULL, 100, &sessionPresent );
				if( status == MQTTSuccess )
				{
			    	static int counter = 0;
					char payload[20] = {0};

					int payload_length = snprintf(payload,sizeof(payload),"Hello %d",counter++);

					// Since we requested a clean session, this must be false
					assert( sessionPresent == false );

					// Do something with the connection. Publish some data.
					MQTTPublishInfo_t  info = {
						MQTTQoS0,
						false,  // This should not be retained
						false,  // This is not a duplicate message
						"Test/Hello", 10,
						payload, payload_length
					};

					MQTT_Publish(&mqttContext, &info , 1);

					// Disconnect
					MQTT_Disconnect(&mqttContext);

				}

				Plaintext_FreeRTOS_Disconnect(mqttContext.transportInterface.pNetworkContext);

				/* Print heap statistics */
				{
					HeapStats_t heap_stats;
					vPortGetHeapStats(&heap_stats);
					FreeRTOS_debug_printf(( "Available heap space              %d\n", heap_stats.xAvailableHeapSpaceInBytes ));
					FreeRTOS_debug_printf(( "Largest Free Block                %d\n", heap_stats.xSizeOfLargestFreeBlockInBytes ));
					FreeRTOS_debug_printf(( "Smallest Free Block               %d\n", heap_stats.xSizeOfSmallestFreeBlockInBytes ));
					FreeRTOS_debug_printf(( "Number of Free Blocks             %d\n", heap_stats.xNumberOfFreeBlocks ));
					FreeRTOS_debug_printf(( "Minimum Ever Free Bytes Remaining %d\n", heap_stats.xMinimumEverFreeBytesRemaining ));
					FreeRTOS_debug_printf(( "Number of Successful Allocations  %d\n", heap_stats.xNumberOfSuccessfulAllocations ));
					FreeRTOS_debug_printf(( "Number of Successful Frees        %d\n", heap_stats.xNumberOfSuccessfulFrees ));
				}

				vTaskDelay(pdMS_TO_TICKS(10000));
			} else if(PLAINTEXT_TRANSPORT_INVALID_PARAMETER == status )
			{
				FreeRTOS_debug_printf(( "Error Connecting to server : bad parameter\n" ));
			}
			else if(PLAINTEXT_TRANSPORT_CONNECT_FAILURE == status)
			{
				FreeRTOS_debug_printf(( "Error Connecting to server : connect failure\n" ));
			}
			else
			{
				FreeRTOS_debug_printf(( "Error Connecting to server : unknown\n" ));
			}
	    }
	}

	for(;;)
	{
		FreeRTOS_debug_printf(("MQTT FAILURE\n"));
		vTaskDelay(pdMS_TO_TICKS(1000));
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
            // vStartSimpleMQTTDemo();
            xTasksAlreadyCreated = pdTRUE;
        }

        /* Print out the network configuration, which may have come from a DHCP
         * server. */
        FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
        FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
        PRINTF(  "\r\n\r\nIP Address: %s\r\n", cBuffer  );

        FreeRTOS_inet_ntoa( ulNetMask, cBuffer );
        PRINTF(  "Subnet Mask: %s\r\n", cBuffer  );

        FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );
        PRINTF(  "Gateway Address: %s\r\n", cBuffer  );

        FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
        PRINTF(  "DNS Server Address: %s\r\n\r\n\r\n", cBuffer  );
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
	PRINTF( "\n\nMALLOC FAIL\n\n");
	for(;;);
}

