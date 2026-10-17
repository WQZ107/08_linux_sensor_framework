#ifndef __MPU6050_LINUX_H__
#define __MPU6050_LINUX_H__
int MPU6050_Linux_Init(void);
int MPU6050_Linux_ReadAccel(float *ax, float *ay, float *az);
#endif
