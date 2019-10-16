################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/Socket.c \
../src/Socket100.c \
../src/Socket110.c \
../src/Socket120.c \
../src/Socket122.c \
../src/Socket130.c \
../src/Socket140.c \
../src/Socket150.c \
../src/Socket151.c \
../src/Socket152.c \
../src/Socket153.c \
../src/Socket154.c \
../src/Socket155.c \
../src/Socket156.c 

OBJS += \
./src/Socket.o \
./src/Socket100.o \
./src/Socket110.o \
./src/Socket120.o \
./src/Socket122.o \
./src/Socket130.o \
./src/Socket140.o \
./src/Socket150.o \
./src/Socket151.o \
./src/Socket152.o \
./src/Socket153.o \
./src/Socket154.o \
./src/Socket155.o \
./src/Socket156.o 

C_DEPS += \
./src/Socket.d \
./src/Socket100.d \
./src/Socket110.d \
./src/Socket120.d \
./src/Socket122.d \
./src/Socket130.d \
./src/Socket140.d \
./src/Socket150.d \
./src/Socket151.d \
./src/Socket152.d \
./src/Socket153.d \
./src/Socket154.d \
./src/Socket155.d \
./src/Socket156.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-linux-gnueabihf-gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


