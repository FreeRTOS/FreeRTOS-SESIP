################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../source/main.c \
../source/semihost_hardfault.c 

OBJS += \
./source/main.o \
./source/semihost_hardfault.o 

C_DEPS += \
./source/main.d \
./source/semihost_hardfault.d 


# Each subdirectory must supply rules for building sources it contributes
source/%.o: ../source/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_LPC54018 -D__USE_CMSIS -DMXL12835F -DCPU_LPC54018JET180=1 -DSERIAL_PORT_TYPE_UART=1 -DFSL_RTOS_FREE_RTOS -DCPU_LPC54018JET180_cm4 -DXIP_IMAGE -DW25Q128JVFM -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -D__MCUXPRESSO -DDEBUG -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/board" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/phy" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/mdio" -I"/Users/ejacobus/gitroot/SESIP_Demo/source" -I"/Users/ejacobus/gitroot/SESIP_Demo" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/drivers" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/device" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/CMSIS" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/FreeRTOS/FreeRTOS-Kernel/include" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM4F" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/utilities" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/component/serial_manager" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/component/lists" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/component/uart" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/FreeRTOS/FreeRTOS-Plus-TCP/include" -I"/Users/ejacobus/gitroot/SESIP_Demo/lib/FreeRTOS/FreeRTOS-Plus-TCP/portable/Compiler/GCC" -O0 -fno-common -g3 -Wall -c  -ffunction-sections  -fdata-sections  -ffreestanding  -fno-builtin -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


