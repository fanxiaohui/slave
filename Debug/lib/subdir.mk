################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lib/crc.c \
../lib/log.c \
../lib/uart.c \
../lib/udp.c 

OBJS += \
./lib/crc.o \
./lib/log.o \
./lib/uart.o \
./lib/udp.o 

C_DEPS += \
./lib/crc.d \
./lib/log.d \
./lib/uart.d \
./lib/udp.d 


# Each subdirectory must supply rules for building sources it contributes
lib/%.o: ../lib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	arm-linux-gnueabihf-gcc -I"/home/wangbo/workspace/slave/lib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


