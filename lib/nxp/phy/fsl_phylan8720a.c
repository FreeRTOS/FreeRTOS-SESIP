/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_phylan8720a.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief Defines the timeout macro. */
#define PHY_TIMEOUT_COUNT 500000

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

const phy_operations_t phylan8720a_ops = {.phyInit            = PHY_LAN8720A_Init,
                                          .phyWrite           = PHY_LAN8720A_Write,
                                          .phyRead            = PHY_LAN8720A_Read,
                                          .getLinkStatus      = PHY_LAN8720A_GetLinkStatus,
                                          .getLinkSpeedDuplex = PHY_LAN8720A_GetLinkSpeedDuplex,
                                          .enableLoopback     = NULL};

/*******************************************************************************
 * Code
 ******************************************************************************/

status_t PHY_LAN8720A_Init(phy_handle_t *handle, const phy_config_t *config)
{
    uint32_t reg;
    uint32_t idReg = 0;
    uint32_t delay = PHY_TIMEOUT_COUNT;
    bool status    = false;

    /* Init MDIO interface. */
    MDIO_Init(handle->mdioHandle);

    /* assign phy address. */
    handle->phyAddr = config->phyAddr;

    /* Initialization after PHY stars to work. */
    while ((idReg != PHY_CONTROL_ID1) && (delay != 0))
    {
        MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_ID1_REG, &idReg);
        delay--;
    }

    if (!delay)
    {
        return 11;//kStatus_Fail;
    }
    delay = PHY_TIMEOUT_COUNT;

    /* Reset PHY and wait until completion. */
    MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG, PHY_BCTL_RESET_MASK);
    do
    {
        MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG, &reg);
    } while (delay-- && reg & PHY_BCTL_RESET_MASK);

    if (!delay)
    {
        return 12;//kStatus_Fail;
    }

    /* Set the ability. */
    MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_AUTONEG_ADVERTISE_REG, (PHY_ALL_CAPABLE_MASK | 0x1U));

    /* Start Auto negotiation and wait until auto negotiation completion */
    MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG,
               (PHY_BCTL_AUTONEG_MASK | PHY_BCTL_RESTART_AUTONEG_MASK));
    delay = PHY_TIMEOUT_COUNT;
    do
    {
        MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_SEPCIAL_CONTROL_REG, &reg);
        delay--;
    } while (delay && ((reg & PHY_SPECIALCTL_AUTONEGDONE_MASK) == 0));

    if (!delay)
    {
        return kStatus_PHY_AutoNegotiateFail;
    }

    /* Waiting a moment for phy stable. */
    for (delay = 0; delay < PHY_TIMEOUT_COUNT; delay++)
    {
        __ASM("nop");
        PHY_GetLinkStatus(handle, &status);
        if (status)
        {
            break;
        }
    }

    return kStatus_Success;
}

status_t PHY_LAN8720A_Write(phy_handle_t *handle, uint32_t phyReg, uint32_t data)
{
    return MDIO_Write(handle->mdioHandle, handle->phyAddr, phyReg, data);
}

status_t PHY_LAN8720A_Read(phy_handle_t *handle, uint32_t phyReg, uint32_t *dataPtr)
{
    return MDIO_Read(handle->mdioHandle, handle->phyAddr, phyReg, dataPtr);
}

status_t PHY_LAN8720A_GetLinkStatus(phy_handle_t *handle, bool *status)
{
    uint32_t reg;
    status_t result = kStatus_Success;

    /* Read the basic status register. */
    result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_BASICSTATUS_REG, &reg);
    if (result == kStatus_Success)
    {
        if (reg & PHY_BSTATUS_LINKSTATUS_MASK)
        {
            /* link up. */
            *status = true;
        }
        else
        {
            *status = false;
        }
    }
    return result;
}

status_t PHY_LAN8720A_GetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t *speed, phy_duplex_t *duplex)
{
    assert(duplex);
    assert(speed);

    uint32_t reg;
    status_t result = kStatus_Success;

    /* Read the control two register. */
    result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_SEPCIAL_CONTROL_REG, &reg);
    if (result == kStatus_Success)
    {
        if (reg & PHY_SPECIALCTL_DUPLEX_MASK)
        {
            /* Full duplex. */
            *duplex = kPHY_FullDuplex;
        }
        else
        {
            /* Half duplex. */
            *duplex = kPHY_HalfDuplex;
        }

        if (reg & PHY_SPECIALCTL_100SPEED_MASK)
        {
            /* 100M speed. */
            *speed = kPHY_Speed100M;
        }
        else
        { /* 10M speed. */
            *speed = kPHY_Speed10M;
        }
    }
    return result;
}
