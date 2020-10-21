################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include source/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: lpc54018iotmodule_freertos_hello.axf

# Tool invocations
lpc54018iotmodule_freertos_hello.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	arm-none-eabi-gcc -nostdlib -L"/Users/ejacobus/gitroot/SESIP_Demo/lib/nxp/libs" -Xlinker -Map="lpc54018iotmodule_freertos_hello.map" -Xlinker --gc-sections -Xlinker -print-memory-usage -Xlinker --sort-section=alignment -Xlinker --cref -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -T lpc54018iotmodule_freertos_hello_Debug.ld -o "lpc54018iotmodule_freertos_hello.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) lpc54018iotmodule_freertos_hello.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size "lpc54018iotmodule_freertos_hello.axf"; # arm-none-eabi-objcopy -v -O binary "lpc54018iotmodule_freertos_hello.axf" "lpc54018iotmodule_freertos_hello.bin" ; # checksum -p LPC54018 -d "lpc54018iotmodule_freertos_hello.bin";
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
