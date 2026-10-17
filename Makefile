CROSS_COMPILE = arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -O2 -std=gnu99 -Iinclude
LDFLAGS = -lpthread -lm
TARGET = sensor_framework
VPATH = src
OBJS = main.o ringbuf.o thread_sensor.o thread_gps.o serial.o gps_nmea.o collector.o thread_process.o thread_store.o mpu6050_linux.o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
