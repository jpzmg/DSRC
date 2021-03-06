/************************************************************/
//文件名：mpu6050.c
//功能:测试linux下iic读写mpu6050程序
//使用说明: (1)
//          (2)
//          (3)
//          (4)
//作者:huangea
//日期:2016-10-03
/************************************************************/
//包含头文件
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include "mpu6050.h"
//****************************************  
// 定义MPU6050内部地址  
//****************************************  
#define	SMPLRT_DIV	0x19	  //陀螺仪采样率，典型值：0x07(125Hz)
#define	CONFIG	0x1A    	  //低通滤波频率，典型值：0x06(5Hz)  
#define	GYRO_CONFIG	0x1B      //陀螺仪自检及测量范围，典型值：0x18(不自检，2000deg/s)
#define	ACCEL_CONFIG	0x1C  //加速计自检、测量范围及高通滤波频率，典型值：0x01(不自检，2G，5Hz) 
#define	ACCEL_XOUT_H	0x3B
#define	ACCEL_XOUT_L	0x3C  //x轴加速度
#define	ACCEL_YOUT_H	0x3D
#define	ACCEL_YOUT_L	0x3E  //y轴加速度 
#define	ACCEL_ZOUT_H	0x3F
#define	ACCEL_ZOUT_L	0x40  //z轴加速度
#define	TEMP_OUT_H	0x41
#define	TEMP_OUT_L	0x42
#define	GYRO_XOUT_H	0x43
#define	GYRO_XOUT_L	0x44      //x轴角速度  
#define	GYRO_YOUT_H	0x45
#define	GYRO_YOUT_L	0x46      //y轴角速度
#define	GYRO_ZOUT_H	0x47
#define	GYRO_ZOUT_L	0x48      //z轴角速度
#define	PWR_MGMT_1	0x6B      //电源管理，典型值：0x00(正常启用) 
#define	WHO_AM_I	0x75	  //IIC地址寄存器(默认数值0x68，只读)
#define	SlaveAddress	0xD0	
#define Address 0x68                  //MPU6050地址
#define I2C_RETRIES   0x0701
#define I2C_TIMEOUT   0x0702
#define I2C_SLAVE     0x0703       //IIC从器件的地址设置
#define I2C_BUS_MODE   0x0780

typedef unsigned char uint8;
static int fd = -1;//函数声明
//float q_bias[3];
float gyro_x, gyro_y, gyro_z;
float accel_x, accel_y, accel_z;
static uint8 i2c_write(int fd, uint8 reg, uint8 val);
static uint8 i2c_read(int fd, uint8 reg, uint8 *val);
static short GetData(unsigned char REG_Address);
#define GYRO 9.78833f
#define PI 3.14159265358979f

//MPU6050初始化
uint8 MPU6050_Init(void)
{
	fd = open("/dev/i2c-0", O_RDWR);    // open file and enable read and  write	
	if(fd< 0)
	{		
		perror("Can't open /dev/MPU6050 \n"); // open i2c dev file fail		
		exit(1);	
	}	
	printf("open /dev/i2c-0 success !\n");   // open i2c dev file succes	
	if(ioctl(fd, I2C_SLAVE, Address)<0) {    //set i2c address 		
		printf("fail to set i2c device slave address!\n");
		close(fd);
		return -1;
	}
	printf("set slave address to 0x%x success!\n", Address);
	i2c_write(fd,PWR_MGMT_1,0X00);
	i2c_write(fd,SMPLRT_DIV,0X07);
	i2c_write(fd,CONFIG,0X06);
	i2c_write(fd,GYRO_CONFIG, 0x18);
	i2c_write(fd,ACCEL_CONFIG,0X00);
	
	return(1);
}

//MPU6050 wirte byte
static uint8 i2c_write(int fd, uint8 reg, uint8 val)
{	
	int retries;
	uint8 data[2];
	data[0] = reg;
	data[1] = val;
	for(retries=5; retries; retries--) {
		if(write(fd, data, 2)==2)
			return 0;
		usleep(1000*10);
	}
	return -1;
}

//MPU6050 read byte
static uint8 i2c_read(int fd, uint8 reg, uint8 *val)
{
	int retries;
	for(retries=5; retries; retries--)
		if(write(fd, &reg, 1)==1)
			if(read(fd, val, 1)==1)
				return 0;
	return -1;
}

//get data
static short GetData(unsigned char REG_Address)
{	
	unsigned char H,L;
	i2c_read(fd, REG_Address, &H);
	i2c_read(fd, REG_Address + 1, &L);
	return (((short)H)<<8)+L;
}

static void I2C_Receive14Bytes(uint8 *anbt_i2c_data_buffer)
{
	i2c_read(fd, 0x3B, &anbt_i2c_data_buffer[0]);
	i2c_read(fd, 0x3C, &anbt_i2c_data_buffer[1]);
	i2c_read(fd, 0x3D, &anbt_i2c_data_buffer[2]);
	i2c_read(fd, 0x3E, &anbt_i2c_data_buffer[3]);
	i2c_read(fd, 0x3F, &anbt_i2c_data_buffer[4]);
	i2c_read(fd, 0x40, &anbt_i2c_data_buffer[5]);
	i2c_read(fd, 0x41, &anbt_i2c_data_buffer[6]);
	i2c_read(fd, 0x42, &anbt_i2c_data_buffer[7]);
	i2c_read(fd, 0x43, &anbt_i2c_data_buffer[8]);
	i2c_read(fd, 0x44, &anbt_i2c_data_buffer[9]);
	i2c_read(fd, 0x45, &anbt_i2c_data_buffer[10]);
	i2c_read(fd, 0x46, &anbt_i2c_data_buffer[11]);
	i2c_read(fd, 0x47, &anbt_i2c_data_buffer[12]);
	i2c_read(fd, 0x48, &anbt_i2c_data_buffer[13]);
}

void Cal_MPU6050_Data(int *cal_data)   
{
	unsigned char i,j;
	unsigned char mpu6050_cal_data_buffer[14];
	signed short int mpu6050_cal_data[100][7];
	int mpu6050_cal_sum[7];
	//
	for(i=0;i<7;i++) mpu6050_cal_sum[i]=0;
	
	//
	for(j=0;j<100;j++)
	{
		I2C_Receive14Bytes(mpu6050_cal_data_buffer);	//鍦嗙偣鍗氬＋:璇诲嚭瀵勫瓨鍣ㄥ��		
		for(i=0;i<7;i++) mpu6050_cal_data[j][i]=(((signed short int)mpu6050_cal_data_buffer[i*2]) << 8) | mpu6050_cal_data_buffer[i*2+1];
	}
	//
	for(i=0;i<7;i++)
	{
		for(j=0;j<100;j++) mpu6050_cal_sum[i]=mpu6050_cal_sum[i]+(int)mpu6050_cal_data[j][i];
	}
	//3搴旇鏄俯搴﹀�硷紝鎵�浠ヤ笉绠′簡锛岃繖閲屽氨鏄娆¤鍙栨暟鎹眰骞冲潎鍊�
	//杩欎釜搴旇绠楁槸缁熻闆舵紓
	cal_data[0]=mpu6050_cal_sum[4]/100;
	cal_data[1]=mpu6050_cal_sum[5]/100;
	cal_data[2]=mpu6050_cal_sum[6]/100;
	cal_data[3]=mpu6050_cal_sum[0]/100;
	cal_data[4]=mpu6050_cal_sum[1]/100;
	cal_data[5]=mpu6050_cal_sum[2]/100;
}


void Load_Calibration_Parameter(float *q_bias)
{
	int cal_par[12];
	signed short int Q_BIAS[6];
	
	Cal_MPU6050_Data(cal_par);	   //鑷垜鏍″噯杩欎釜杩樻槸寰堥渶瑕佺殑
	//
	Q_BIAS[0] = (signed short int)cal_par[0];		//闄�铻轰华鏍￠獙鍙傛暟
	Q_BIAS[1] = (signed short int)cal_par[1];     	//闄�铻轰华鏍￠獙鍙傛暟
	Q_BIAS[2] = (signed short int)cal_par[2];     	//闄�铻轰华鏍￠獙鍙傛暟
	
	Q_BIAS[3] = (signed short int)cal_par[3];		//鍔犻�熷害鏍￠獙鍙傛暟
	Q_BIAS[4] = (signed short int)cal_par[4];     	//鍔犻�熷害鏍￠獙鍙傛暟
	Q_BIAS[5] = (signed short int)cal_par[5];     	//鍔犻�熷害鏍￠獙鍙傛暟

	q_bias[0] = 2000 * Q_BIAS[0]/32768;	// 瑙掑姞閫熷害鍋忕疆鍊�
	q_bias[1] = 2000 * Q_BIAS[1]/32768;
	q_bias[2] = 2000 * Q_BIAS[2]/32768;
}


void GetSensorData(Sensor *sensordata)
{
	if(sensordata != NULL){
		sensordata->accel_x = GetData(ACCEL_XOUT_H);
		sensordata->accel_y = GetData(ACCEL_YOUT_H);
		sensordata->accel_z = GetData(ACCEL_ZOUT_H);
		sensordata->gyro_x = GetData(GYRO_XOUT_H);
		sensordata->gyro_y = GetData(GYRO_YOUT_H);
		sensordata->gyro_z = GetData(GYRO_ZOUT_H);
	}
}

uint8 MPU6050_exit(void)
{
	close(fd);
	return(1);
}


