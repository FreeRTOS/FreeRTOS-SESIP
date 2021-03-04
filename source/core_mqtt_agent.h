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
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @brief Header file containing MQTT agent APIS.
 * MQTT agents APIs are used to manage MQTT APIS thread safety in a multithreaded evironment.
 * This is a lightweight MQTT agent implementation to demonstrate how concurrent tasks can share the same
 * MQTT connection and publish and subscribe to different topics with the MQTT broker.
 */

#ifndef CORE_MQTT_AGENT_H
#define CORE_MQTT_AGENT_H

/* FreeRTOS include. */
#include "FreeRTOS.h"

/* MQTT library include */
#include "core_mqtt.h"

/**
 * @brief Forward declaration of MQTT operation struct.
 * The struct is used by the application to enqueue an MQTT operation to be processed
 * by the MQTT agent.
 */
struct MQTTOperation;

/**
 * @brief Callback invoked by MQTT agent to notify the status of MQTT operation.
 * The callback will be invoked on a sucessful sent if its Qos0 publish, or when an ACK packet
 * is received for Qos1,2 publishes, subscribe and unsubscribe.
 *
 * @param[in] pOperation Pointer to the MQTT operation structure passed from application.
 * @param[in] status Status of the MQTT operation.
 */
typedef void ( * MQTTOperationStatusCallback_t ) ( struct MQTTOperation * pOperation,
                                                   MQTTStatus_t status );

/**
 * @brief Definitions for all MQTT operation types handled by the MQTT agent.
 */
typedef enum MQTTOperationType
{
    MQTT_OP_PUBLISH = 0,
    MQTT_OP_SUBSCRIBE,
    MQTT_OP_UNSUBSCRIBE,
    MQTT_OP_RECEIVE,
    MQTT_OP_STOP
} MQTTOperationType_t;

/**
 * @brief Structure used to hold parameters for MQTT operation enqueued with the agent.
 */
typedef union MQTTOperationInfo
{
    MQTTPublishInfo_t * pPublishInfo;
    struct
    {
        MQTTSubscribeInfo_t * pSubscriptionList;
        uint16_t numSubscriptions;
    } subscriptionInfo;
} MQTTOperationInfo_t;

/**
 * @brief Structure used to hold the MQTT operation enqueued with the MQTT agent.
 */
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
 * @param[in] pContext The corteMQTT library MQTT context.
 * @return pdTRUE if the initialization was successful.
 *
 */
BaseType_t MQTTAgent_Init( MQTTContext_t * pContext );

/*
 * @brief Enqueues an MQTT operation to be executed in agent context.
 * Result of the operation will be available using MQTTOperationStatusCallback_t.
 * @param[in] pOperation Pointer to the structure containing operation type and params.
 * @param[in] timeoutTicks Timeout in ticks API blocks for enqueue operation to succeed.
 * @return pdTRUE If the operation was successfully enqueued with the agent.
 */
BaseType_t MQTTAgent_Enqueue( MQTTOperation_t * pOperation,
                              TickType_t timeoutTicks );

/*
 * @brief Handler invoked for incoming MQTT packets to the MQTT agent.
 * The API is invoked from the main MQTT event callback on every packet received on the MQTT
 * connection. The agent should process only ACK packets and invokes the application task callbacks.
 * It should return pdFALSE for all other packets indicating further processing is required.
 *
 * @param[in] pMQTTContext Pointer to the context used by the coreMQTT library.
 * @param[in] pPacketInfo Pointer to the MQTT packet information.
 * @param[in] pDeserializedInfo Pointer to deserialized Publish packet information.
 * @return pdTRUE if the agent processed the message indicating no further processing required.
 *
 */
BaseType_t MQTTAgent_ProcessEvent( MQTTContext_t * pMQTTContext,
                                   struct MQTTPacketInfo * pPacketInfo,
                                   struct MQTTDeserializedInfo * pDeserializedInfo );

/**
 * @brief Stops the agent task and deletes the queue.
 * Should be called before disconnecting an MQTT connection.
 */
void MQTTAgent_Stop( void );

#endif /* ifndef CORE_MQTT_AGENT_H */
