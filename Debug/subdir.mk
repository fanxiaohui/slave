################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../AM335X_2.c \
../aws.c \
../bd.c \
../global.c \
../modbus_polling.c \
../rkt.c 

OBJS += \
./AM335X_2.o \
./aws.o \
./bd.o \
./global.o \
./modbus_polling.o \
./rkt.o 

C_DEPS += \
./AM335X_2.d \
./aws.d \
./bd.d \
./global.d \
./modbus_polling.d \
./rkt.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	arm-linux-gnueabihf-gcc -I"/home/wangbo/workspace/slave/lib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


