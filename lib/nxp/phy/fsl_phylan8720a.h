/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _FSL_PHYLAN8720A_H_
#define _FSL_PHYLAN8720A_H_

#include "fsl_phy.h"

/*!
 * @addtogroup phy_driver
 * @{
 */

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief PHY driver version */
#define FSL_PHY_DRIVER_VERSION (MAKE_VERSION(2, 0, 1)) /*!< Version 2.0.1. */

/*! @brief Defines the PHY registers. */
#define PHY_SEPCIAL_CONTROL_REG 0x1FU /*!< The PHY control two register. */

#define PHY_CONTROL_ID1 0x07U /*!< The PHY ID1*/

/*!@brief Defines the mask flag of operation mode in special control register*/
#define PHY_SPECIALCTL_AUTONEGDONE_MASK 0x1000U /*!< The PHY auto-negotiation complete mask. */
#define PHY_SPECIALCTL_DUPLEX_MASK      0x0010U /*!< The PHY duplex mask. */
#define PHY_SPECIALCTL_100SPEED_MASK    0x0008U /*!< The PHY speed mask. */
#define PHY_SPECIALCTL_10SPEED_MASK     0x0004U /*!< The PHY speed mask. */
#define PHY_SPECIALCTL_SPEEDUPLX_MASK   0x001cU /*!< The PHY speed and duplex mask. */

/*! @brief Defines the mask flag in PHY auto-negotiation advertise register. */
#define PHY_ALL_CAPABLE_MASK 0x1e0U

/*! @brief ENET MDIO operations structure. */
extern const phy_operations_t phylan8720a_ops;

/*******************************************************************************
 * API
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @name PHY Driver
 * @{
 */

/*!
 * @brief Initializes PHY.
 *
 *  This function initialize PHY.
 *
 * @param handle       PHY device handle.
 * @param config       Pointer to structure of phy_config_t.
 * @retval kStatus_Success  PHY initialize success
 * @retval kStatus_Fail  PHY initialize fail
 * @retval kStatus_PHY_SMIVisitTimeout  PHY SMI visit time out
 * @retval kStatus_PHY_AutoNegotiateFail  PHY auto negotiate fail
 */
status_t PHY_LAN8720A_Init(phy_handle_t *handle, const phy_config_t *config);

/*!
 * @brief PHY Write function. This function writes data over the SMI to
 * the specified PHY register. This function is called by all PHY interfaces.
 *
 * @param handle  PHY device handle.
 * @param phyReg  The PHY register.
 * @param data    The data written to the PHY register.
 * @retval kStatus_Success     PHY write success
 * @retval kStatus_PHY_SMIVisitTimeout  PHY SMI visit time out
 */
status_t PHY_LAN8720A_Write(phy_handle_t *handle, uint32_t phyReg, uint32_t data);

/*!
 * @brief PHY Read function. This interface reads data over the SMI from the
 * specified PHY register. This function is called by all PHY interfaces.
 *
 * @param handle   PHY device handle.
 * @param phyReg   The PHY register.
 * @param dataPtr  The address to store the data read from the PHY register.
 * @retval kStatus_Success  PHY read success
 * @retval kStatus_PHY_SMIVisitTimeout  PHY SMI visit time out
 */
status_t PHY_LAN8720A_Read(phy_handle_t *handle, uint32_t phyReg, uint32_t *dataPtr);

/*!
 * @brief Gets the PHY link status.
 *
 * @param handle   PHY device handle.
 * @param status   The link up or down status of the PHY.
 *         - true the link is up.
 *         - false the link is down.
 * @retval kStatus_Success   PHY gets link status success
 * @retval kStatus_PHY_SMIVisitTimeout  PHY SMI visit time out
 */
status_t PHY_LAN8720A_GetLinkStatus(phy_handle_t *handle, bool *status);

/*!
 * @brief Gets the PHY link speed and duplex.
 *
 * @param handle   PHY device handle.
 * @param speed    The address of PHY link speed.
 * @param duplex   The link duplex of PHY.
 * @retval kStatus_Success   PHY gets link speed and duplex success
 * @retval kStatus_PHY_SMIVisitTimeout  PHY SMI visit time out
 */
status_t PHY_LAN8720A_GetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t *speed, phy_duplex_t *duplex);

/* @} */

#if defined(__cplusplus)
}
#endif

/*! @}*/

#endif /* _FSL_PHYLAN8720A_H_ */
