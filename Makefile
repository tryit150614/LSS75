TOOLCHAIN_PATH = $(HOME)/tools/gcc-arm-none-eabi-7-2018-q2-update/bin/

PREFIX = arm-none-eabi-

CC = $(TOOLCHAIN_PATH)/$(PREFIX)gcc
CXX = $(TOOLCHAIN_PATH)/$(PREFIX)g++
CP = $(TOOLCHAIN_PATH)/$(PREFIX)objcopy
RM=rm -rf

MACROS_VALUE = HSE_VALUE=16000000

# Assembler, Compiler and Linker flags and linker script settings
LINK_SCRIPT=STM32L451REIx_FLASH.ld
LINKER_FLAGS=-lm -mthumb -mhard-float -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -Wl,--gc-sections -T$(LINK_SCRIPT) -static  -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group -specs=nano.specs -specs=nosys.specs  -Wl,-cref "-Wl,-Map=$(BUILD_DIR)/cpluplusTest.map" -Wl,--defsym=malloc_getpagesize_P=0x1000
ASSEMBLER_FLAGS=-c -g -O0 -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -mhard-float -specs=nano.specs -x assembler-with-cpp  -IDrivers/CMSIS/Include $(DEFS) -IDrivers/CMSIS/Device/ST/STM32L4xx/Include -IDrivers/STM32L4xx_HAL_Driver/Inc $(INCLUDES)
COMPILER_FLAGS=-c -g -std=gnu99  -pedantic -Werror -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -O0 -Wall -ffunction-sections -fdata-sections -mthumb -mhard-float -specs=nano.specs $(DEFS) -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32L4xx/Include -IDrivers/STM32L4xx_HAL_Driver/Inc $(INCLUDES)
COMPILER_FLAGS += -Wformat-security -Wformat-nonliteral  -Wno-format-extra-args -D$(MACROS_VALUE)

CXXCOMPILER_FLAGS=-std=gnu++11 -fno-threadsafe-statics -c -g -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -O0 -Wall -ffunction-sections -fdata-sections -mthumb -mhard-float -specs=nano.specs -fno-exceptions -fno-rtti $(DEFS) -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32L4xx/Include -IDrivers/STM32L4xx_HAL_Driver/Inc $(INCLUDES)
CXXCOMPILER_FLAGS += -Wformat-security -Wformat-nonliteral -Wno-format-extra-args -D$(MACROS_VALUE)

DEFS =  \
-DUSE_FULL_LL_DRIVER \
-DUSE_HAL_DRIVER \

HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

######################################
# target
######################################
TARGET = LSS75


######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og


#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

#######################################
# STLINK tools
#######################################
#ST Tools
STLINK_DIR = ~/tools/STM32Cube/STM32CubeProgrammer/bin/
STLINK_CLI = STM32_Programmer.sh
STLINK_FLAGS = -c port=SWD mode=UR --erase all --write
STLINK_RESET_FLAGS = -c port=SWD -Rst
STLINK_STARTADDR = 0x8000000

######################################
# source
######################################
SRC =  \
$(wildcard ./*.cpp) \
$(wildcard Src/*.cpp) \
$(wildcard Src/*/*.cpp) \
$(wildcard Src/*.c) \
$(wildcard Src/*/*.c) \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_utils.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_exti.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_gpio.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_adc.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_dma.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c_ex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc_ex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash_ex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash_ramfunc.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_gpio.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dma.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dma_ex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr_ex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_cortex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_exti.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_lpuart.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_rcc.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_usart.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_tim.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_tim_ex.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_rtc.c \
Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rng.c \
Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
Middlewares/Third_Party/FreeRTOS/Source/list.c \
Middlewares/Third_Party/FreeRTOS/Source/queue.c \
Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
Middlewares/Third_Party/FreeRTOS/Source/timers.c \
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c

SRCSASM =  \
startup_stm32l451xx.s

OBJS := $(SRC:%.c=$(BUILD_DIR)/%.o) $(SRCSASM:%.s=$(BUILD_DIR)/%.o)
OBJS := $(OBJS:%.S=$(BUILD_DIR)/%.o)
OBJS := $(OBJS:%.cpp=$(BUILD_DIR)/%.o)

INCLUDES =  \
-IInc \
-IDrivers/STM32L4xx_HAL_Driver/Inc \
-IDrivers/STM32L4xx_HAL_Driver/Inc/Legacy \
-IMiddlewares/Third_Party/FreeRTOS/Source/include \
-IMiddlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
-IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
-IDrivers/CMSIS/Device/ST/STM32L4xx/Include \
-IDrivers/CMSIS/Include


###############
# Build project
# Major targets
###############
all: buildelf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

buildelf: $(OBJS)
	$(CXX) -o "$(BUILD_DIR)/$(TARGET).elf" $(OBJS) $(LINKER_FLAGS)

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/$(TARGET).elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

#clean:
#	$(RM) $(OBJS) "$(BUILD_DIR)/$(TARGET).elf" "$(BUILD_DIR)/$(TARGET).map"

clean:
	$(RM) $(BUILD_DIR)/*

##################
# Specific targets
##################
$(BUILD_DIR)/Src/main.o: Src/main.c
	@mkdir -p $(dir $@) 2> /dev/null
	$(CXX) $(CXXCOMPILER_FLAGS) Src/main.c -o $(BUILD_DIR)/Src/main.o

##################
# Implicit targets
##################
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@) 2> /dev/null
	$(CC) $(COMPILER_FLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@) 2> /dev/null
	$(CXX) $(CXXCOMPILER_FLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@) 2> /dev/null
	$(CC) $(ASSEMBLER_FLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@) 2> /dev/null
	$(CC) $(ASSEMBLER_FLAGS) $< -o $@

#######################################
# Write program to board
#######################################
download:
	$(STLINK_DIR)/$(STLINK_CLI) $(STLINK_FLAGS) $(BUILD_DIR)/$(TARGET).bin $(STLINK_STARTADDR)
	$(STLINK_DIR)/$(STLINK_CLI) $(STLINK_RESET_FLAGS)
rst:
	$(STLINK_DIR)/$(STLINK_CLI) $(STLINK_RESET_FLAGS)
#######################################
# Rebuild (clean and make)
#######################################
rebuild: clean all