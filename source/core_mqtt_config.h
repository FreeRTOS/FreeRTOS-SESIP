/*
 * coreMQTT v1.0.0
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
 * @file core_mqtt_config_defaults.h
 * @brief This represents the default values for the configuration macros
 * for the MQTT library.
 *
 * @note This file SHOULD NOT be modified. If custom values are needed for
 * any configuration macro, a core_mqtt_config.h file should be provided to
 * the MQTT library to override the default values defined in this file.
 * To use the custom config file, the MQTT_DO_NOT_USE_CUSTOM_CONFIG preprocessor
 * macro SHOULD NOT be set.
 */

#ifndef CORE_MQTT_CONFIG_H_
#define CORE_MQTT_CONFIG_H_

/**************************************************/
/******* DO NOT CHANGE the following order ********/
/**************************************************/

/* Include logging header files and define logging macros in the following order:
 * 1. Include the header file "logging_levels.h".
 * 2. Define the LIBRARY_LOG_NAME and LIBRARY_LOG_LEVEL macros depending on
 * the logging configuration for MQTT.
 * 3. Include the header file "logging_stack.h", if logging is enabled for MQTT.
 */

#include "logging_levels.h"

/* Logging configuration for the MQTT library. */
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "MQTT"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_WARN
#endif


#include "logging_stack.h"

/* The macro definition for MQTT_DO_NOT_USE_CUSTOM_CONFIG is for Doxygen
 * documentation only. */

/**
 * @brief Define this macro to build the MQTT library without the custom config
 * file core_mqtt_config.h.
 *
 * Without the custom config, the MQTT library builds with
 * default values of config macros defined in core_mqtt_config_defaults.h file.
 *
 * If a custom config is provided, then MQTT_DO_NOT_USE_CUSTOM_CONFIG should not
 * be defined.
 */
#ifdef DOXYGEN
    #define MQTT_DO_NOT_USE_CUSTOM_CONFIG
#endif

/**
 * @brief Determines the maximum number of MQTT PUBLISH messages, pending
 * acknowledgment at a time, that are supported for incoming and outgoing
 * direction of messages, separately.
 *
 * QoS 1 and 2 MQTT PUBLISHes require acknowledgment from the server before
 * they can be completed. While they are awaiting the acknowledgment, the
 * client must maintain information about their state. The value of this
 * macro sets the limit on how many simultaneous PUBLISH states an MQTT
 * context maintains, separately, for both incoming and outgoing direction of
 * PUBLISHes.
 *
 * @note The MQTT context maintains separate state records for outgoing
 * and incoming PUBLISHes, and thus, 2 * MQTT_STATE_ARRAY_MAX_COUNT amount
 * of memory is statically allocated for the state records.
 *
 * <b>Possible values:</b> Any positive 32 bit integer. <br>
 * <b>Default value:</b> `10`
 */
#ifndef MQTT_STATE_ARRAY_MAX_COUNT
    /* Default value for the maximum acknowledgment pending PUBLISH messages. */
    #define MQTT_STATE_ARRAY_MAX_COUNT    ( 10U )
#endif

/**
 * @brief The number of retries for receiving CONNACK.
 *
 * The MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT will be used only when the
 * timeoutMs parameter of #MQTT_Connect is passed as 0 . The transport
 * receive for CONNACK will be retried MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT
 * times before timing out. A value of 0 for this config will cause the
 * transport receive for CONNACK  to be invoked only once.
 *
 * <b>Possible values:</b> Any positive 16 bit integer. <br>
 * <b>Default value:</b> `5`
 */
#ifndef MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT
    /* Default value for the CONNACK receive retries. */
    #define MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT    ( 5U )
#endif

/**
 * @brief Number of milliseconds to wait for a ping response to a ping
 * request as part of the keep-alive mechanism.
 *
 * If a ping response is not received before this timeout, then
 * #MQTT_ProcessLoop will return #MQTTKeepAliveTimeout.
 *
 * <b>Possible values:</b> Any positive integer up to SIZE_MAX. <br>
 * <b>Default value:</b> `500`
 */
#ifndef MQTT_PINGRESP_TIMEOUT_MS
    /* Wait 0.5 seconds by default for a ping response. */
    #define MQTT_PINGRESP_TIMEOUT_MS    ( 500U )
#endif

#endif /* ifndef CORE_MQTT_CONFIG_H_ */
