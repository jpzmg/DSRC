#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "timer_queue.h"
#include "mpu6050.h"
#include "TimerTask.h"
#include "list.h"
#include "CarSta.h"
#include "TxOpts.h"
#include "llc-test-tx.h"
//传感器检测周期，1000ms(本来是想50ms检测一次的)
#define MPU6050_PERIOD 100
#define BROADCAST_PERIOD 100
#define NEIGEBOR_PERIOD  5000
#define EXPIRETIME 5.0f


typedef struct CwMessage
{
    char protocol;
    uint16_t length;
    unsigned char data[2048];
    uint32_t seq;
    uint8_t hop;
}CwMessage;

//鐢�32浣嶅瓧鑺傝褰曢┚椹剁姸鎬佹儏鍐�
static uint32_t Drive_status = 0x00000000;

static int datalop;

static struct timer mpu6050_timer;

static struct timer broadcast_timer;

static struct timer neighbor_timer;

extern struct LLCTx *pDev;
extern tTxOpts *pTxOpts;
extern list_t Car_list;
extern CStatus CarS;


extern int packetmessage(CwMessage *message);
extern void packetandroidmessage(CwMessage *message);

extern int Tx_SendAtRate(tTxOpts * pTxOpts, int packetlength);

union float2char
{
	float d;
	unsigned char data[4];
};

union double2char
{
	double d;
	unsigned char data[8];
};

union uint32_t2char
{
	uint32_t d;
	unsigned char data[4];
};


float q_bias[3];

float gyro_x, gyro_y, gyro_z;
float accel_x, accel_y, accel_z;
#define GYRO 9.78833f
#define PI 3.14159265358979f

static struct CarStatus carstatus;
static struct Accel acl;
static CwMessage WsmMessage;

static int NUM_brake_A = 0;
static int NUM_brake_B = 0;
static int NUM_brake_C = 0;
static int NUM_brake_D = 0;

static int NUM_speedup_A = 0;
static int NUM_speedup_B = 0;
static int NUM_speedup_C = 0;
static int NUM_speedup_D = 0;


static int NUM_turn_A = 0;
static int NUM_turn_B = 0;
static int NUM_turn_C = 0;
static int NUM_turn_D = 0;

static int NUM_Rollover_A = 0;
static int NUM_Rollover_B = 0;
static int NUM_Rollover_C = 0;
static int NUM_Rollover_D = 0;

static Sensor *Mpu6050Sensor = NULL;
/* 设置定时发送SIGALRM 
*	参数，秒，微妙，处理函数设置
*/


float dt = 0.05;//dt鐨勫彇鍊间负kalman婊ゆ尝鍣ㄩ噰鏍锋椂闂�,s涓哄崟浣�
float Angle_xoz, Gyro_y;//瑙掑害鍜岃閫熷害
float PP_xoz[2][2]={{1, 0},
				{0, 1}};
float Pdot_xoz[4] = { 0, 0, 0, 0};
float Q_angle = 0.001, Q_gyro = 0.003; //瑙掑害鏁版嵁缃俊搴︼紝瑙掗�熷害鏁版嵁缃俊搴�
float R_angle_xoz = 0.5, C_0_xoz = 1;
float Q_bias_xoz, Angle_err_xoz, PCt_0_xoz, PCt_1_xoz, E_xoz, K_0_xoz, K_1_xoz, t_0_xoz, t_1_xoz;

//鍗″皵鏇兼护娉紝鍙傝�冪綉绔欏拰鏂囩珷
//http://www.cnblogs.com/dchipnau/p/5310088.html
//https://wenku.baidu.com/view/dcfaca33c850ad02de8041d0.html

//XOZ
void Kalman_Filter_XOZ(float Gyro,float angle_m)	//瑙掗�熷害锛屽姞閫熷害
{
//楠屼及璁�
	Q_bias_xoz = q_bias[1];
	Angle_xoz += (Gyro - Q_bias_xoz) * dt;           //涓婁竴鍒昏搴﹀姞涓婏紙瑙掗�熷害-璇樊锛�*鏃堕棿
//鍗忔柟宸煩闃电殑棰勬祴
	Pdot_xoz[0]=Q_angle - PP_xoz[0][1] - PP_xoz[1][0];
	Pdot_xoz[1]= - PP_xoz[1][1];
	Pdot_xoz[2]= - PP_xoz[1][1];
	Pdot_xoz[3]= Q_gyro;
	
	PP_xoz[0][0] += Pdot_xoz[0] * dt;   
	PP_xoz[0][1] += Pdot_xoz[1] * dt; 
	PP_xoz[1][0] += Pdot_xoz[2] * dt;
	PP_xoz[1][1] += Pdot_xoz[3] * dt;  
	// 	閫氳繃鍗″皵鏇煎鐩婅繘琛屼慨鏁�	
	Angle_err_xoz = angle_m - Angle_xoz;
	
	PCt_0_xoz = C_0_xoz * PP_xoz[0][0];
	PCt_1_xoz = C_0_xoz * PP_xoz[1][0];
	
	E_xoz = R_angle_xoz + C_0_xoz * PCt_0_xoz;
	
	K_0_xoz = PCt_0_xoz / E_xoz;
	K_1_xoz = PCt_1_xoz / E_xoz;
	
	t_0_xoz = PCt_0_xoz;
	t_1_xoz = C_0_xoz * PP_xoz[0][1];
//鏇存柊鍗忔柟宸樀
	PP_xoz[0][0] -= K_0_xoz * t_0_xoz;
	PP_xoz[0][1] -= K_0_xoz * t_1_xoz;
	PP_xoz[1][0] -= K_1_xoz * t_0_xoz;
	PP_xoz[1][1] -= K_1_xoz * t_1_xoz;
		
	Angle_xoz	+= K_0_xoz * Angle_err_xoz;
	Q_bias_xoz	+= K_1_xoz * Angle_err_xoz;	
	Gyro_y   = Gyro - Q_bias_xoz;	
}

float Angle_yoz, Gyro_x;//瑙掑害鍜岃閫熷害
float PP_yoz[2][2]={{1, 0},
				{0, 1}};
float Pdot_yoz[4] = { 0, 0, 0, 0};
float R_angle_yoz = 0.5, C_0_yoz = 1;
float Q_bias_yoz, Angle_err_yoz, PCt_0_yoz, PCt_1_yoz, E_yoz, K_0_yoz, K_1_yoz, t_0_yoz, t_1_yoz;

//鍗″皵鏇兼护娉紝鍙傝�冪綉绔欏拰鏂囩珷
//http://www.cnblogs.com/dchipnau/p/5310088.html
//https://wenku.baidu.com/view/dcfaca33c850ad02de8041d0.html

//XOZ
void Kalman_Filter_YOZ(float Gyro,float angle_m)	//瑙掗�熷害锛屽姞閫熷害
{
//楠屼及璁�
	Q_bias_yoz = q_bias[0];
	Angle_yoz += (Gyro - Q_bias_yoz) * dt;           //涓婁竴鍒昏搴﹀姞涓婏紙瑙掗�熷害-璇樊锛�*鏃堕棿
//鍗忔柟宸煩闃电殑棰勬祴
	Pdot_yoz[0]=Q_angle - PP_yoz[0][1] - PP_yoz[1][0];
	Pdot_yoz[1]= - PP_yoz[1][1];
	Pdot_yoz[2]= - PP_yoz[1][1];
	Pdot_yoz[3]= Q_gyro;
	
	PP_yoz[0][0] += Pdot_yoz[0] * dt;   
	PP_yoz[0][1] += Pdot_yoz[1] * dt; 
	PP_yoz[1][0] += Pdot_yoz[2] * dt;
	PP_yoz[1][1] += Pdot_yoz[3] * dt;  
	// 	閫氳繃鍗″皵鏇煎鐩婅繘琛屼慨鏁�	
	Angle_err_yoz = angle_m - Angle_yoz;
	
	PCt_0_yoz = C_0_yoz * PP_yoz[0][0];
	PCt_1_yoz = C_0_yoz * PP_yoz[1][0];
	
	E_yoz = R_angle_yoz + C_0_yoz * PCt_0_yoz;
	
	K_0_yoz = PCt_0_yoz / E_yoz;
	K_1_yoz = PCt_1_yoz / E_yoz;
	
	t_0_yoz = PCt_0_yoz;
	t_1_yoz = C_0_yoz * PP_yoz[0][1];
//鏇存柊鍗忔柟宸樀
	PP_yoz[0][0] -= K_0_yoz * t_0_yoz;
	PP_yoz[0][1] -= K_0_yoz * t_1_yoz;
	PP_yoz[1][0] -= K_1_yoz * t_0_yoz;
	PP_yoz[1][1] -= K_1_yoz * t_1_yoz;
		
	Angle_yoz	+= K_0_yoz * Angle_err_yoz;
	Q_bias_yoz	+= K_1_yoz * Angle_err_yoz;	
	Gyro_x   = Gyro - Q_bias_yoz;	
}

//鍒硅溅鏃跺�欙紝浣滅敤鍔涗綔鐢ㄥ湪accel_y鐨勫弽鍚�
static void Detection_car_brake(float accy)
{
	unsigned char NUM_brake_N1 = 8;  //rank 1  The number of Samples
	unsigned char NUM_brake_N2 = 4;  //rank 2
	unsigned char NUM_brake_N3 = 2;  //rank 3
	unsigned char NUM_brake_N4 = 1;   //rank 4
	
	if(accy > 4.0f){//4.0
		if(NUM_brake_D < NUM_brake_N4 - 1) //Vehicle speedup/brake fast
			NUM_brake_D++;
		else{
			NUM_brake_D = 0;
			carstatus.brake_rand = 4;
		}
	}else if(accy > 2.5f){//2.5
		NUM_brake_D = 0;
		if(NUM_brake_C < NUM_brake_N3 - 1) //Vehicle speedup/brake fast
			NUM_brake_C++;
		else{
			NUM_brake_C = 0;
			carstatus.brake_rand = 3;
		}
	}else if(accy > 1.5f){//1.5
	NUM_brake_D = 0;
	NUM_brake_C = 0;
		if(NUM_brake_B < NUM_brake_N2 - 1) //Vehicle speedup/brake normally
			NUM_brake_B++;
		else{
			NUM_brake_B = 0;
			carstatus.brake_rand = 2;
		}

	}else if(accy > 1.0f){//1.0
	NUM_brake_D = 0;
	NUM_brake_C = 0;
	NUM_brake_B = 0;
		if(NUM_brake_A < NUM_brake_N1 - 1) ////vehicle speedup/brake slow
			NUM_brake_A++;
		else{
			NUM_brake_A = 0;
			carstatus.brake_rand = 1;
		}

	}else{  //Maybe brake or accelerate, Y is vehicle move direction
		NUM_brake_D = 0;
		NUM_brake_C = 0;
		NUM_brake_B = 0;
		NUM_brake_A = 0;
		carstatus.brake_rand = 0;
	}
}

//鍔犻�� 浣滅敤鍔涗綔鐢ㄥ湪accy姝ｅ悜
static void Detection_speedup(float accy)
{
	unsigned char NUM_speedup_N1 = 8;  //rank 1  The number of Samples
	unsigned char NUM_speedup_N2 = 4;  //rank 2
	unsigned char NUM_speedup_N3 = 2;  //rank 3
	unsigned char NUM_speedup_N4 = 1;   //rank 4
	if(accy < -4.0f){//4.0
		if(NUM_speedup_D < NUM_speedup_N4 - 1) //Vehicle speedup/brake fast
			NUM_speedup_D++;
		else{
			NUM_speedup_D = 0;
			carstatus.speedup_rand = 4;

		}
	}else if(accy < -2.5f){//2.5
		NUM_speedup_D = 0;
		if(NUM_speedup_C < NUM_speedup_N3 - 1) //Vehicle speedup/brake fast
			NUM_speedup_C++;
		else{
			NUM_speedup_C = 0;
			carstatus.speedup_rand = 3;
		}

	}else if(accy < -1.5f){//1.5
		NUM_speedup_D = 0;
		NUM_speedup_C = 0;
		if(NUM_speedup_B < NUM_speedup_N2 - 1) //Vehicle speedup/brake normally
			NUM_speedup_B++;
		else{
			NUM_speedup_B = 0;
			carstatus.speedup_rand = 2;
		}

	}else if(accy < -1.0f){//1.0
		NUM_speedup_D = 0;
		NUM_speedup_C = 0;
		NUM_speedup_B = 0;
		if(NUM_speedup_A < NUM_speedup_N1 - 1) ////vehicle speedup/brake slow
			NUM_speedup_A++;
		else{
			//what to do, communication to Android ?
			NUM_speedup_A = 0;
			carstatus.speedup_rand = 1;
		}

	}else{  //Maybe brake or accelerate, Y is vehicle move direction
		NUM_speedup_D = 0;
		NUM_speedup_C = 0;
		NUM_speedup_B = 0;
		NUM_speedup_A = 0;
		carstatus.speedup_rand = 0;
	}
}

//宸﹁浆鍙宠浆灏辨槸鍥寸粫gyro_z杞存棆杞紝宸﹁浆灏辨槸璐熺殑锛屽彸杞氨鏄鐨�
static void Detection_turn(float Zgyro){
	unsigned char NUM_turn_N1 = 8;
	unsigned char NUM_turn_N2 = 4;
	unsigned char NUM_turn_N3 = 2;
	unsigned char NUM_turn_N4 = 1;
	
	if(Zgyro <= 0){//鍙宠浆
		if(Zgyro < -0.45f){//0.45
			if(NUM_turn_D < NUM_turn_N4 -1){
				NUM_turn_D ++;
			}else{
				NUM_turn_D = 0;
				carstatus.turn_rand = 0x14;
			}
		}else if(Zgyro < -0.3f){//0.3
			if(NUM_turn_C < NUM_turn_N3 -1){
				NUM_turn_C ++;
			}else{
				NUM_turn_C = 0;
				carstatus.turn_rand = 0x13;		
			}
		}else if(Zgyro < -0.15f){//0.15
			if(NUM_turn_B < NUM_turn_N2 -1){
				NUM_turn_B ++;
			}else{
				NUM_turn_B = 0;
				carstatus.turn_rand= 0x12;	
			}
		}else if(Zgyro < -0.1f){//0.1
			if(NUM_turn_A < NUM_turn_N1 -1){
				NUM_turn_A ++;
			}else{
				NUM_turn_A = 0;
				carstatus.turn_rand = 0x11;	
			}
		}else{
			NUM_turn_A = 0;
			NUM_turn_B = 0;
			NUM_turn_C = 0;
			NUM_turn_D = 0;
			carstatus.turn_rand = 0;
		}
	}else{//宸﹁浆
        if(Zgyro > 0.45f){
			if(NUM_turn_D < NUM_turn_N4 -1){
				NUM_turn_D ++;
			}else{
				NUM_turn_D = 0;
				carstatus.turn_rand = 0x24;
			}
		}else if(Zgyro > 0.3f){
			if(NUM_turn_C < NUM_turn_N3 -1){
				NUM_turn_C ++;
			}else{
				NUM_turn_C = 0;
				carstatus.turn_rand = 0x23;
			}
		}else if(Zgyro > 0.15f){
			if(NUM_turn_B < NUM_turn_N2 -1){
				NUM_turn_B ++;
			}else{
				NUM_turn_B = 0;
				carstatus.turn_rand = 0x22;
			}
		}else if(Zgyro > 0.1f){
			if(NUM_turn_A < NUM_turn_N1 -1){
				NUM_turn_A ++;
			}else{
				NUM_turn_A = 0;
				carstatus.turn_rand = 0x21;
			}
		}else{
			NUM_turn_A = 0;
			NUM_turn_B = 0;
			NUM_turn_C = 0;
			NUM_turn_D = 0;
			carstatus.turn_rand = 0;
		}		
	}
}

static void Detection_turn_over(float axzAng){
	unsigned char NUM_turn_N1 = 4;
	unsigned char NUM_turn_N2 = 2;
	unsigned char NUM_turn_N3 = 1;;
	printf("test turn over AxzAngle[0] = %f\n", fabs(fabs(axzAng)-90));
	if(fabs(fabs(axzAng)-90) > 35.0f ){	// 125.0f
		if(NUM_Rollover_D < NUM_turn_N3 - 1){
			NUM_Rollover_D ++;
		}else{
			NUM_Rollover_D = 0;
			carstatus.Rollover_rand = 4;
		}

	}else if(fabs(fabs(axzAng)-90) > 25.0f ){//35	115.0f
		NUM_Rollover_D = 0;
		if(NUM_Rollover_C < NUM_turn_N3 - 1){
			NUM_Rollover_C ++;
		}else{
			NUM_Rollover_C = 0;
			carstatus.Rollover_rand = 3;
		}
	}else if (fabs(fabs(axzAng)-90) > 20.0f ){//30 110.0f
		NUM_Rollover_D = 0;
		NUM_Rollover_C = 0;
		if(NUM_Rollover_B < NUM_turn_N2 - 1){
			NUM_Rollover_B ++;
		}else{
			NUM_Rollover_B = 0;
			carstatus.Rollover_rand = 2;
		}
	}else if (fabs(fabs(axzAng)-90) > 15.0f ){//25 105.0f 
	//}else if ( (90 - fabs(nbs_tmp->AxzAngle[0]) )>25.0 ){
		NUM_Rollover_B = 0;
		NUM_Rollover_C = 0;
		NUM_Rollover_D = 0;
		if(NUM_Rollover_A < NUM_turn_N1 - 1){
			NUM_Rollover_A ++;
		}else{
			NUM_Rollover_A = 0;
			carstatus.Rollover_rand = 1;
		}
	}else {
		NUM_Rollover_A = 0;
		NUM_Rollover_B = 0;
		NUM_Rollover_C = 0;
		NUM_Rollover_D = 0;
		carstatus.Rollover_rand = 0;
	}
	//printf("in Dto,Rollover_rand:%d\n",Rollover_rand);
}

//鍦ㄨ繖閲屽仛濮挎�佹娴嬶紵50ms妫�娴嬩竴娆�
/*绱ф�ユ秷鎭彂閫�
 *  0x24 urgent turn over message
 *	0x25 urgent brake message
 *	0x26 urgent turn message
 *	0x27 纰版挒(娌″啓)
 *	0x28 urgent fatigue driving message(鐤插姵椹鹃┒锛屾病鏈夊啓)
 *  0x29 鎬ュ姞閫�
*/
void mpu6050_handler(void *arg)
{
	float angle_xoz, angle_yoz;
	float accy;
	int packetlength;
	if(arg == NULL){
		perror("arg is NULL");
		return ;
	}
		
	Sensor *sen;
	sen = (Sensor *)arg;
	GetSensorData(sen);
	gyro_x = 2000 * sen->gyro_x/32768;//瑙掗�熷害
	gyro_y = 2000 * sen->gyro_y/32768;
	gyro_z = 2000 * sen->gyro_z/32768;//杩欎釜鏈夊亸缃��

	accel_x = 2 * GYRO * sen->accel_x / 32768;
	accel_y = 2 * GYRO * sen->accel_y / 32768;
	accel_z = 2 * GYRO * sen->accel_z / 32768;

	angle_xoz = atan2(accel_z, accel_x) * 57.3f;//XOZ
	angle_yoz = atan2(accel_z, accel_y) * 57.3f;//YOZ 

	Kalman_Filter_XOZ(gyro_y, angle_xoz);//gyro_z杩欎釜鍋忕疆灏变笉鐢ㄧ浜�
	Kalman_Filter_YOZ(gyro_x, angle_yoz);//濡傛灉鏄寜鐓х洰鍓嶇殑鏀剧疆鎽嗙潃杞﹀瓙涓婏紝YOZ骞抽潰搴旇鍙樺寲涓嶅ぇ
	//闄ら潪鍦ㄤ笂鍧″拰涓嬪潯瀵艰嚧閲嶅姏鍔犻�熷害鍦ㄨ繖浜涙柟闈㈢殑鍒嗛噺
	
//accy搴旂粨鏋滀竴鐩撮兘鏄鐨�
	accy = GYRO*sin(fabs(fabs(angle_yoz)-90)/57.3f);//姹傞噸鍔涘姞閫熷害鍦╝ccel_y涓婄殑鍒嗛噺
	
//	if(accel_x * accx >=0)
//		signGx =1;
//	else signGx =-1;

//	if(accel_y * accy >=0)
//		signGy =1;
//	else signGy =-1;

//		if(accel_z * accz >=0)
//			signGz =1;
//		else signGz =-1;

//	accel_x += accx;
	//棣栧厛angle_yoz鑾峰緱鐨勫�兼槸璐熺殑锛屾墍浠ュ幓缁濆鍊硷紝鐒跺悗濡傛灉澶т簬90搴﹁〃绀轰笂鍧�
	//灏忎簬90搴﹁〃绀轰笅鍧★紝涓婂潯鐨勮瘽锛岄噸鍔涘湪accel_y鐨勬鍚戞湁鍔犱綔鐢ㄥ姏锛屾墍浠ヨ鍑忔帀
	//鍙嶆鍚岀悊
	if(fabs(angle_yoz) >= 90)
		accel_y -= accy;
	else
		accel_y += accy;

	gyro_z = gyro_z - q_bias[2];//闄ゅ幓瑙掗�熷害鍋忕疆锛屽墿涓嬬殑灏辨槸绮剧‘鍊硷紝瑙掗�熷害锛� 搴�/s
	gyro_z = gyro_z/57.3f;//鍒ゆ柇rad/s,鍙傝�冮倱蹇犲笀鍏勮鏂�
	
	Detection_car_brake(accel_y);
	Detection_speedup(accel_y);
	Detection_turn(gyro_z);
	Detection_turn_over(Angle_xoz);

	acl.x = accel_x;
	acl.y = accel_y;
	acl.z = accel_z;
	SetAccel(&acl);

//姣忎竴娆¤溅杈嗙姸鎬佹洿鏂伴兘瑕佹洿鏂癈arS
	SetCarStatus(&carstatus);

	if(CarS.valid){
		Drive_status |= IS_LOCATED;
	}else{
		Drive_status &= NOT_LOCATED;
	}
	if(CarS.location.clat == 0x4E){
		Drive_status |= IS_NORTH_LAT;//鍖楃含
	}else{
		Drive_status &= NOT_NORTH_LAT;//鍗楃含
	}
	if(CarS.location.clon == 0x45){
		Drive_status |= IS_EAST_LON; //涓滅粡
	}else{
		Drive_status &= NOT_EAST_LON;//瑗跨粡
	}
	//澶勭悊涓�浜涚獊鍙戜簨浠�
	switch (carstatus.brake_rand)
	{
		case 0:
		    Drive_status &= NOT_BRAKE;
			Drive_status &= CLEAR_BRAKE_RANK;
			Drive_status |= SET_BRAKE_RANK_0;
			break;
		case 1:
			Drive_status |= IS_BRAKE;
			Drive_status &= CLEAR_BRAKE_RANK;
			Drive_status |= SET_BRAKE_RANK_1;
			break;
		case 2:
			Drive_status |= IS_BRAKE;
			Drive_status &= CLEAR_BRAKE_RANK;
			Drive_status |= SET_BRAKE_RANK_2;
			break;
		case 3:
			Drive_status |= IS_BRAKE;
			Drive_status &= CLEAR_BRAKE_RANK;
			Drive_status |= SET_BRAKE_RANK_3;
			break;
		case 4:
			Drive_status |= IS_BRAKE;
			Drive_status &= CLEAR_BRAKE_RANK;
			Drive_status |= SET_BRAKE_RANK_4;
			break;
		default:
			break;
	}
	if((Drive_status & 0x00001c00) >= 0x00000c00){
	    union double2char d2c;
	    union float2char f2c;
		union uint32_t2char i2c;
		memset(&WsmMessage, 0, sizeof(struct CwMessage));
		WsmMessage.protocol = 0x25;
		WsmMessage.seq = pDev->SeqNum;
		WsmMessage.hop = 5; //杩欓噷鍥犱负鏄揣鎬ヤ俊鎭紝鎵�浠ラ噰鐢ㄥ璺冲箍鎾嚭鍘�
		WsmMessage.length = 59;
		memcpy(WsmMessage.data, CarS.plate, 9);
		d2c.d = CarS.location.latitude;
		memcpy(&WsmMessage.data[9], d2c.data, 8);
		d2c.d = CarS.location.longitude;
		memcpy(&WsmMessage.data[17], d2c.data, 8);
		f2c.d = CarS.location.speed;
		memcpy(&WsmMessage.data[25], f2c.data, 4);
		f2c.d = CarS.location.bearing;
		memcpy(&WsmMessage.data[29], f2c.data, 4);
		f2c.d = accel_x;
		memcpy(&WsmMessage.data[33], f2c.data, 4);
		f2c.d = accel_y;
		memcpy(&WsmMessage.data[37], f2c.data, 4);
		f2c.d = accel_z;
		memcpy(&WsmMessage.data[41], f2c.data, 4);
		//45~52 娴锋嫈楂樺害
		i2c.d = Drive_status;
		memcpy(&WsmMessage.data[53], i2c.data, 4);
		packetlength = packetmessage(&WsmMessage);
		Tx_SendAtRate(pTxOpts, packetlength);
	}
	switch (carstatus.turn_rand)
	{
		case 0:	
			Drive_status &= NOT_TURN_LEFT;
			Drive_status &= NOT_TURN_RIGHT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_0;
			break;
		case 0x11:
            Drive_status |= IS_TURN_LEFT;
			Drive_status &= NOT_TURN_RIGHT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_1;
			break;
		case 0x12:
			Drive_status |= IS_TURN_LEFT;
			Drive_status &= NOT_TURN_RIGHT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_2;
			break;
		case 0x13:
			Drive_status |= IS_TURN_LEFT;
			Drive_status &= NOT_TURN_RIGHT;			
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_3;
			break;
		case 0x14:
			//i can't deal with left or right situation, because it is not yet completed
			Drive_status |= IS_TURN_LEFT;
			Drive_status &= NOT_TURN_RIGHT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_4;
			break;
		case 0x21:
			//i can't deal with left or right situation, because it is not yet completed
			Drive_status |= IS_TURN_RIGHT;
			Drive_status &= NOT_TURN_LEFT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_1;
			break;
		case 0x22:
			//i can't deal with left or right situation, because it is not yet completed
			Drive_status |= IS_TURN_RIGHT;
			Drive_status &= NOT_TURN_LEFT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_2;
			break;
		case 0x23:
			//i can't deal with left or right situation, because it is not yet completed
			Drive_status |= IS_TURN_RIGHT;
			Drive_status &= NOT_TURN_LEFT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_3;
			break;		
		case 0x24:
			//i can't deal with left or right situation, because it is not yet completed
			Drive_status |= IS_TURN_RIGHT;
			Drive_status &= NOT_TURN_LEFT;
			Drive_status &= CLEAR_TURN_RANK;
			Drive_status |= SET_TURN_RANK_4;
			break;
		default:break;
	}
	if((Drive_status & 0x0000E000) >= 0x00000600){
		union double2char d2c;
	    union float2char f2c;
		union uint32_t2char i2c;
		memset(&WsmMessage, 0, sizeof(struct CwMessage));
		WsmMessage.protocol = 0x26;
		WsmMessage.seq = pDev->SeqNum;
		WsmMessage.hop = 5; //杩欓噷鍥犱负鏄揣鎬ヤ俊鎭紝鎵�浠ラ噰鐢ㄥ璺冲箍鎾嚭鍘�
		WsmMessage.length = 59;
		memcpy(WsmMessage.data, CarS.plate, 9);
		d2c.d = CarS.location.latitude;
		memcpy(&WsmMessage.data[9], d2c.data, 8);
		d2c.d = CarS.location.longitude;
		memcpy(&WsmMessage.data[17], d2c.data, 8);
		f2c.d = CarS.location.speed;
		memcpy(&WsmMessage.data[25], f2c.data, 4);
		f2c.d = CarS.location.bearing;
		memcpy(&WsmMessage.data[29], f2c.data, 4);
		f2c.d = accel_x;
		memcpy(&WsmMessage.data[33], f2c.data, 4);
		f2c.d = accel_y;
		memcpy(&WsmMessage.data[37], f2c.data, 4);
		f2c.d = accel_z;
		memcpy(&WsmMessage.data[41], f2c.data, 4);
		//45~52 娴锋嫈楂樺害
		i2c.d = Drive_status;
		memcpy(&WsmMessage.data[53], i2c.data, 4);
		packetlength = packetmessage(&WsmMessage);
		Tx_SendAtRate(pTxOpts, packetlength);
	}
	switch(carstatus.Rollover_rand)
	{
		case 0:
			Drive_status &= NOT_TURN_OVER;
			Drive_status &= CLEAR_TURN_OVER_RANK;
			Drive_status |= SET_TURN_OVER_RANK_0;
		    break;
		case 1:
			Drive_status |= IS_TURN_OVER;
			Drive_status &= CLEAR_TURN_OVER_RANK;
			Drive_status |= SET_TURN_OVER_RANK_1;
		    break;
		case 2:
			Drive_status |= IS_TURN_OVER;
			Drive_status &= CLEAR_TURN_OVER_RANK;
			Drive_status |= SET_TURN_OVER_RANK_2;
		    break;
		case 3:
			Drive_status |= IS_TURN_OVER;
			Drive_status &= CLEAR_TURN_OVER_RANK;
			Drive_status |= SET_TURN_OVER_RANK_3;
		    break;
		case 4:
			Drive_status |= IS_TURN_OVER;
			Drive_status &= CLEAR_TURN_OVER_RANK;
			Drive_status |= SET_TURN_OVER_RANK_4;
		    break;		
		default:break;		
    }
	if((Drive_status & 0x00070000) >= 0x00020000){
		union double2char d2c;
	    union float2char f2c;
		union uint32_t2char i2c;
		memset(&WsmMessage, 0, sizeof(struct CwMessage));
		WsmMessage.protocol = 0x24;
		WsmMessage.seq = pDev->SeqNum;
		WsmMessage.hop = 5; //杩欓噷鍥犱负鏄揣鎬ヤ俊鎭紝鎵�浠ラ噰鐢ㄥ璺冲箍鎾嚭鍘�
		WsmMessage.length = 59;
		memcpy(WsmMessage.data, CarS.plate, 9);
		d2c.d = CarS.location.latitude;
		memcpy(&WsmMessage.data[9], d2c.data, 8);
		d2c.d = CarS.location.longitude;
		memcpy(&WsmMessage.data[17], d2c.data, 8);
		f2c.d = CarS.location.speed;
		memcpy(&WsmMessage.data[25], f2c.data, 4);
		f2c.d = CarS.location.bearing;
		memcpy(&WsmMessage.data[29], f2c.data, 4);
		f2c.d = accel_x;
		memcpy(&WsmMessage.data[33], f2c.data, 4);
		f2c.d = accel_y;
		memcpy(&WsmMessage.data[37], f2c.data, 4);
		f2c.d = accel_z;
		memcpy(&WsmMessage.data[41], f2c.data, 4);
		//45~52 娴锋嫈楂樺害
		i2c.d = Drive_status;
		memcpy(&WsmMessage.data[53], i2c.data, 4);
		packetlength = packetmessage(&WsmMessage);
		Tx_SendAtRate(pTxOpts, packetlength);
	}
	switch(carstatus.speedup_rand)
	{
		case 0:
			Drive_status &= CLEAR_SPEEDUP_RANK;
			Drive_status |= SET_SPEEDUP_RANK_0;
		break;
		case 1:
			Drive_status &= CLEAR_SPEEDUP_RANK;
			Drive_status |= SET_SPEEDUP_RANK_1;
		break;
		case 2:
			Drive_status &= CLEAR_SPEEDUP_RANK;
			Drive_status |= SET_SPEEDUP_RANK_2;
		break;
		case 3:
			Drive_status &= CLEAR_SPEEDUP_RANK;
			Drive_status |= SET_SPEEDUP_RANK_3;
		break;
		case 4:
			Drive_status &= CLEAR_SPEEDUP_RANK;
			Drive_status |= SET_SPEEDUP_RANK_4;
		break;
		default:break;
	}
	if((Drive_status & 0x00E00000) >= 0x00600000){
		union double2char d2c;
		union float2char f2c;
		union uint32_t2char i2c;
		memset(&WsmMessage, 0, sizeof(struct CwMessage));
		WsmMessage.protocol = 0x29;
		WsmMessage.seq = pDev->SeqNum;
		WsmMessage.hop = 5; //杩欓噷鍥犱负鏄揣鎬ヤ俊鎭紝鎵�浠ラ噰鐢ㄥ璺冲箍鎾嚭鍘�
		WsmMessage.length = 59;
		memcpy(WsmMessage.data, CarS.plate, 9);
		d2c.d = CarS.location.latitude;
		memcpy(&WsmMessage.data[9], d2c.data, 8);
		d2c.d = CarS.location.longitude;
		memcpy(&WsmMessage.data[17], d2c.data, 8);
		f2c.d = CarS.location.speed;
		memcpy(&WsmMessage.data[25], f2c.data, 4);
		f2c.d = CarS.location.bearing;
		memcpy(&WsmMessage.data[29], f2c.data, 4);
		f2c.d = accel_x;
		memcpy(&WsmMessage.data[33], f2c.data, 4);
		f2c.d = accel_y;
		memcpy(&WsmMessage.data[37], f2c.data, 4);
		f2c.d = accel_z;
		memcpy(&WsmMessage.data[41], f2c.data, 4);
		//45~52 娴锋嫈楂樺害
		i2c.d = Drive_status;
		memcpy(&WsmMessage.data[53], i2c.data, 4);
		packetlength = packetmessage(&WsmMessage);
		Tx_SendAtRate(pTxOpts, packetlength);

	}
	//姣忔妫�娴嬮兘鏈夊懆鏈熶俊鎭彂閫佺粰android
	union double2char d2c;
	union float2char f2c;
	union uint32_t2char i2c;
	memset(&WsmMessage, 0, sizeof(struct CwMessage));
	WsmMessage.protocol = 0x56;
	WsmMessage.seq = pDev->SeqNum;
	WsmMessage.hop = 1; //杩欓噷鍥犱负鏄揣鎬ヤ俊鎭紝鎵�浠ラ噰鐢ㄥ璺冲箍鎾嚭鍘�
	WsmMessage.length = 59;
	memcpy(WsmMessage.data, CarS.plate, 9);
	d2c.d = CarS.location.latitude;
	memcpy(&WsmMessage.data[9], d2c.data, 8);
	d2c.d = CarS.location.longitude;
	memcpy(&WsmMessage.data[17], d2c.data, 8);
	f2c.d = CarS.location.speed;
	memcpy(&WsmMessage.data[25], f2c.data, 4);
	f2c.d = CarS.location.bearing;
	memcpy(&WsmMessage.data[29], f2c.data, 4);
	f2c.d = accel_x;
	memcpy(&WsmMessage.data[33], f2c.data, 4);
	f2c.d = accel_y;
	memcpy(&WsmMessage.data[37], f2c.data, 4);
	f2c.d = accel_z;
	memcpy(&WsmMessage.data[41], f2c.data, 4);
	//45~52 娴锋嫈楂樺害
	i2c.d = Drive_status;
	memcpy(&WsmMessage.data[53], i2c.data, 4);
	//send to android
	packetandroidmessage(&WsmMessage);

	timer_set_timeout(&mpu6050_timer, MPU6050_PERIOD);
}



void mpu6050_start(void)
{
	MPU6050_Init();
	if(mpu6050_timer.used)
		return;
	
  	if((Mpu6050Sensor = (Sensor *)malloc(sizeof(Sensor) * 1)) < 0){
		printf("malloc error, cannot read Mpu6050 data\n");
		return ;
	}

	Load_Calibration_Parameter(q_bias);
	
	timer_init(&mpu6050_timer, &mpu6050_handler, Mpu6050Sensor);

	mpu6050_handler(Mpu6050Sensor);  

}

//濮挎�佷笌骞挎挱鏈夌偣鍐茬獊
void broadcast_handler(void *tmp)
{
	union double2char d2c;
	union float2char f2c;
	union uint32_t2char i2c;
	int packetlength;
	memset(&WsmMessage, 0, sizeof(struct CwMessage));
	WsmMessage.protocol = 0x22;
	WsmMessage.seq = pDev->SeqNum;
	WsmMessage.hop = 1; //杩欓噷鍥犱负鏄懆鏈熷箍鎾俊鎭紝鍙渶瑕佷竴璺�
	WsmMessage.length = 59; //data + crc + 0x0d
	memcpy(WsmMessage.data, CarS.plate, 9);
	d2c.d = CarS.location.latitude;
	memcpy(&WsmMessage.data[9], d2c.data, 8);
	d2c.d = CarS.location.longitude;
	memcpy(&WsmMessage.data[17], d2c.data, 8);
	f2c.d = CarS.location.speed;
	memcpy(&WsmMessage.data[25], f2c.data, 4);
	f2c.d = CarS.location.bearing;
	memcpy(&WsmMessage.data[29], f2c.data, 4);
	f2c.d = accel_x;
	memcpy(&WsmMessage.data[33], f2c.data, 4);
	f2c.d = accel_y;
	memcpy(&WsmMessage.data[37], f2c.data, 4);
	f2c.d = accel_z;
	memcpy(&WsmMessage.data[41], f2c.data, 4);
	//45~52 娴锋嫈楂樺害
	i2c.d = Drive_status;
	memcpy(&WsmMessage.data[53], i2c.data, 4);
	packetlength = packetmessage(&WsmMessage);
	Tx_SendAtRate(pTxOpts, packetlength);

	timer_set_timeout(&broadcast_timer, BROADCAST_PERIOD);

}


void broadcast_start(void)
{
	if(broadcast_timer.used)
		return;
	
	timer_init(&broadcast_timer, &broadcast_handler, &Drive_status);

	broadcast_handler(&Drive_status);  

}

//濮挎�佷笌骞挎挱鏈夌偣鍐茬獊
void neighbor_handler(void *datalop)
{
	list_t *pos;
	list_t *tmp;
	LocalStatu *lstmp;
	time_t t;
	t = time(NULL);
	list_foreach_safe(pos, tmp, &Car_list){
	    lstmp = (LocalStatu *)pos;
		if(difftime(lstmp->status.Newtime, t) > EXPIRETIME)
			neigh_delete(lstmp);
	}
	
	timer_set_timeout(&neighbor_timer, NEIGEBOR_PERIOD);

}

void neighbortable_start(void)
{
	if(neighbor_timer.used)
		return;
	
	timer_init(&neighbor_timer, &neighbor_handler, &datalop);

	neighbor_handler(&datalop);  

}



void mpu6050_stop(void)
{
    MPU6050_exit();
	timer_remove(&mpu6050_timer);
	free(Mpu6050Sensor);
	Mpu6050Sensor = NULL;
}

void broadcast_stop(void)
{
	timer_remove(&broadcast_timer);
}

void neighbor_stop(void)
{
	timer_remove(&neighbor_timer);
}

//mpu6050_start之后调用
struct CarStatus *GetCS(void)
{
	return &carstatus;
}

Sensor *GetSensor(void)
{
	if(Mpu6050Sensor != NULL)
		return Mpu6050Sensor;
	else
		return NULL;
}

uint32_t GetDriveStatus(void)
{
	return Drive_status;
}
