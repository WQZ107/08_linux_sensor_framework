CROSS_COMPILE = arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
SQLITE_DIR ?= /home/wu/sqlite-arm
CFLAGS = -Wall -O2 -std=gnu99 -Iinclude -I$(SQLITE_DIR)/include
LDFLAGS = -lpthread -lm -L$(SQLITE_DIR)/lib -lsqlite3 -Wl,-rpath,/home/root/sqlite-arm/lib
TARGET = sensor_framework
VPATH = src
OBJS = main.o ringbuf.o thread_sensor.o thread_gps.o serial.o gps_nmea.o collector.o thread_process.o thread_store.o mpu6050_linux.o database.o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
