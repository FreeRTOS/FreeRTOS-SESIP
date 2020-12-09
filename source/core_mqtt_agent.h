/*
 * Core MQTT Agent.
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

#ifndef MQTT_AGENT_H
#define MQTT_AGENT_H

#include "FreeRTOS.h"
#include "queue.h"

#include "core_mqtt.h"

struct MQTTOperation;

typedef void ( * MQTTOperationStatusCallback_t ) ( struct MQTTOperation *pOperation, MQTTStatus_t status );

typedef enum MQTTOperationType
{
	MQTT_OP_PUBLISH = 0,
	MQTT_OP_SUBSCRIBE,
	MQTT_OP_UNSUBSCRIBE,
	MQTT_OP_RECEIVE,
	MQTT_OP_STOP
} MQTTOperationType_t;


typedef union MQTTOperationInfo
{
	MQTTPublishInfo_t * pPublishInfo;
	struct
	{
		MQTTSubscribeInfo_t *pSubscriptionList;
		uint16_t numSubscriptions;
	} subscriptionInfo;

} MQTTOperationInfo_t;

typedef struct MQTTOperation
{
	MQTTOperationType_t type;
	MQTTOperationInfo_t info;
	MQTTOperationStatusCallback_t callback;
	uint16_t packetIdentifier;
} MQTTOperation_t;

/**
 * @brief Initializes Agent task and creates the queue for MQTT operations.
 * Enqueues an MQTT receive operation by default.
 * The API should be called after an MQTT connection is established.
 *
 */
BaseType_t MQTTAgent_Init( MQTTContext_t *pContext );

/*
 * @brief Enqueues an MQTT operation to be executed in agent context.
 * Result of the operation will be available using MQTTOperationStatusCallback_t.
 *
 */
BaseType_t MQTTAgent_Enqueue( MQTTOperation_t *pOperation, TickType_t timeoutTicks );

/*
 * @brief API to process an MQTT event for an agent.
 * The API follows chain of responsibility principle, and should be invoked from the main MQTT event loop.
 * API returns pdFALSE if it cant process the MQTT event.
 */
BaseType_t MQTTAgent_ProcessEvent( MQTTContext_t *pMQTTContext,
		                           struct MQTTPacketInfo * pPacketInfo,
		                           struct MQTTDeserializedInfo * pDeserializedInfo );
/**
 * @brief Stops the agent task and deletes the queue.
 * Should be called before disconnecting an MQTT connection.
 */
void MQTTAgent_Stop( void );

#endif
