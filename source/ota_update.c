/*
 * FreeRTOS OTA Update task.
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

#include "FreeRTOS.h"
#include "task.h"

#include "fsl_debug_console.h"

#include "core_pkcs11.h"
#include "provision_interface.h"

#include "core_mqtt.h"

/* FreeRTOS OTA agent includes. */
#include "aws_iot_ota_agent.h"

#include "aws_application_version.h"

/**
 * @brief The delay used in the main OTA Demo task loop to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define OTA_UPDATE_TASK_DELAY_SECONDS                  ( 1UL )


/* Task priority for OTA update task. */
#define ota_update_task_PRIORITY    ( configMAX_PRIORITIES - 2 )

/* Declare the firmware version structure for all to see. */
const AppVersion32_t xAppFirmwareVersion =
{
    .u.x.ucMajor = APP_VERSION_MAJOR,
    .u.x.ucMinor = APP_VERSION_MINOR,
    .u.x.usBuild = APP_VERSION_BUILD,
};


/**
 * @brief The OTA agent has completed the update job or it is in
 * self test mode. If it was accepted, we want to activate the new image.
 * This typically means we should reset the device to run the new firmware.
 * If now is not a good time to reset the device, it may be activated later
 * by your user code. If the update was rejected, just return without doing
 * anything and we'll wait for another job. If it reported that we should
 * start test mode, normally we would perform some kind of system checks to
 * make sure our new firmware does the basic things we think it should do
 * but we'll just go ahead and set the image as accepted for demo purposes.
 * The accept function varies depending on your platform. Refer to the OTA
 * PAL implementation for your platform in aws_ota_pal.c to see what it
 * does for you.
 *
 * @param[in] eEvent Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @return None.
 */
static void App_OTACompleteCallback( OTA_JobEvent_t eEvent )
{
    OTA_Err_t xErr = kOTA_Err_Uninitialized;

    DEFINE_OTA_METHOD_NAME( "App_OTACompleteCallback" );

    /* OTA job is completed. so delete the MQTT and network connection. */
    if( eEvent == eOTA_JobEvent_Activate )
    {
    	PRINTF( "Received eOTA_JobEvent_Activate callback from OTA Agent.\r\n" );
        /* Activate the new firmware image. */
        OTA_ActivateNewImage();

        /* We should never get here as new image activation must reset the device.*/
        PRINTF( "New image activation failed.\r\n" );

        for( ; ; )
        {
        }
    }
    else if( eEvent == eOTA_JobEvent_Fail )
    {
    	PRINTF( "Received eOTA_JobEvent_Fail callback from OTA Agent.\r\n" );

        /* Nothing special to do. The OTA agent handles it. */
    }
    else if( eEvent == eOTA_JobEvent_StartTest )
    {
        /* This demo just accepts the image since it was a good OTA update and networking
         * and services are all working (or we wouldn't have made it this far). If this
         * were some custom device that wants to test other things before calling it OK,
         * this would be the place to kick off those tests before calling OTA_SetImageState()
         * with the final result of either accepted or rejected. */

    	PRINTF( "Received eOTA_JobEvent_StartTest callback from OTA Agent.\r\n" );
        xErr = OTA_SetImageState( eOTA_ImageState_Accepted );

        if( xErr != kOTA_Err_None )
        {
        	PRINTF( " Error! Failed to set image state as accepted.\r\n" );
        }
    }
}


void OTAUpdateTask( void * pvParameters )
{
    OTA_State_t eState;
    static OTA_ConnectionContext_t xOTAConnectionCtx = { 0 };
    char *pcThingName = NULL;
    uint32_t thingNameLength = 0;
    CK_RV status;

    status =  ulGetThingName( &pcThingName, &thingNameLength );
    if( status != CKR_OK )
    {
    	PRINTF( "Cannot find Thing name.\r\n" );
    }
    else
    {
    	(void) thingNameLength;

    	xOTAConnectionCtx.pvControlClient = pvParameters;

    	PRINTF( "OTA Update version %u.%u.%u\r\n",
    			xAppFirmwareVersion.u.x.ucMajor,
				xAppFirmwareVersion.u.x.ucMinor,
				xAppFirmwareVersion.u.x.usBuild );

    	for( ; ; )
    	{

    		/* Check if OTA Agent is suspended and resume.*/
    		if( ( eState = OTA_GetAgentState() ) == eOTA_AgentState_Suspended )
    		{
    			OTA_Resume( &xOTAConnectionCtx );
    		}

    		/* Initialize the OTA Agent , if it is resuming the OTA statistics will be cleared for new connection.*/
    		OTA_AgentInit( ( void * ) ( &xOTAConnectionCtx ),
    				( const uint8_t * ) pcThingName,
					App_OTACompleteCallback,
					( TickType_t ) ~0 );

    		while( ( eState = OTA_GetAgentState() ) != eOTA_AgentState_Stopped ) // && _networkConnected )
    		{
    			/* Wait forever for OTA traffic but allow other tasks to run and output statistics only once per second. */
    			vTaskDelay( pdMS_TO_TICKS( OTA_UPDATE_TASK_DELAY_SECONDS * 1000 ) );

    			PRINTF( "Received: %u   Queued: %u   Processed: %u   Dropped: %u\r\n",
    					OTA_GetPacketsReceived(), OTA_GetPacketsQueued(), OTA_GetPacketsProcessed(), OTA_GetPacketsDropped() );
    		}
    	}
    }
}

BaseType_t xCreateOTAUpdateTask( MQTTContext_t *pMQTTContext )
{
   return xTaskCreate( OTAUpdateTask, "OTA_task", 2048, pMQTTContext, ota_update_task_PRIORITY | portPRIVILEGE_BIT, NULL );
}
