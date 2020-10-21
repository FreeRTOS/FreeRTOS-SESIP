/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_enet_mdio.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void ENET_MDIO_Init(mdio_handle_t *handle);

static status_t ENET_MDIO_Write(mdio_handle_t *handle, uint32_t phyAddr, uint32_t devAddr, uint32_t data);

static status_t ENET_MDIO_Read(mdio_handle_t *handle, uint32_t phyAddr, uint32_t devAddr, uint32_t *dataPtr);

/*!
 * @brief Get the ENET instance from peripheral base address.
 *
 * @param base ENET peripheral base address.
 * @return ENET instance.
 */
extern uint32_t ENET_GetInstance(ENET_Type *base);

/*******************************************************************************
 * Variables
 ******************************************************************************/

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
/*! @brief Pointers to enet clocks for each instance. */
extern clock_ip_name_t s_enetClock[];
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

const mdio_operations_t lpc_enet_ops = {.mdioInit     = ENET_MDIO_Init,
                                        .mdioWrite    = ENET_MDIO_Write,
                                        .mdioRead     = ENET_MDIO_Read,
                                        .mdioWriteExt = NULL,
                                        .mdioReadExt  = NULL};

/*******************************************************************************
 * Code
 ******************************************************************************/

static void ENET_MDIO_Init(mdio_handle_t *handle)
{
    mdio_resource_t *resource = (mdio_resource_t *)&handle->resource;
    ENET_Type *base           = (ENET_Type *)resource->base;
    uint32_t instance         = ENET_GetInstance(base);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Set SMI first. */
    CLOCK_EnableClock(s_enetClock[instance]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
    ENET_SetSMI(base);
}

status_t ENET_MDIO_Write(mdio_handle_t *handle, uint32_t phyAddr, uint32_t devAddr, uint32_t data)
{
    mdio_resource_t *resource = (mdio_resource_t *)&handle->resource;
    ENET_Type *base           = (ENET_Type *)resource->base;

    ENET_StartSMIWrite(base, phyAddr, devAddr, data);
    while (ENET_IsSMIBusy(base))
    {
    }

    return kStatus_Success;
}

status_t ENET_MDIO_Read(mdio_handle_t *handle, uint32_t phyAddr, uint32_t devAddr, uint32_t *dataPtr)
{
    assert(dataPtr);

    mdio_resource_t *resource = (mdio_resource_t *)&handle->resource;
    ENET_Type *base           = (ENET_Type *)resource->base;

    ENET_StartSMIRead(base, phyAddr, devAddr);
    while (ENET_IsSMIBusy(base))
    {
    }
    *dataPtr = ENET_ReadSMIData(base);

    return kStatus_Success;
}
