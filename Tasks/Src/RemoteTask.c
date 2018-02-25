/**
  ******************************************************************************
  * File Name          : RemoteTask.c
  * Description        : 遥控器处理任务
  ******************************************************************************
  *
  * Copyright (c) 2018 Team TPP-Shanghai Jiao Tong University
  * All rights reserved.
  *
  ******************************************************************************
  */
#include "includes.h"
uint8_t rc_data[18];
RC_Ctl_t RC_CtrlData;
InputMode_e inputmode = REMOTE_INPUT; 
FunctionMode_e functionmode = NORMAL;
ChassisSpeed_Ref_t ChassisSpeedRef; 
RemoteSwitch_t g_switch1;
extern RampGen_t frictionRamp ;  //摩擦轮斜坡
extern RampGen_t LRSpeedRamp ;   //键盘速度斜坡
extern RampGen_t FBSpeedRamp  ;   

float rotate_speed;

double AMUD1AngleTarget = 0.0;
double AMUD2AngleTarget = 0.0;
double AMFBAngleTarget = 0.0;
double WINDAngleTarget = 0.0;
double AMSIDEAngleTarget = 0.0;

#define ANGLE_STEP 10
#define IGNORE_RANGE 50
#define STEPMODE 1
#define ROTATE_FACTOR 0.02

//遥控器控制量初始化
void RemoteTaskInit()
{
	LRSpeedRamp.SetScale(&LRSpeedRamp, MOUSE_LR_RAMP_TICK_COUNT);
	FBSpeedRamp.SetScale(&FBSpeedRamp, MOUSR_FB_RAMP_TICK_COUNT);
	LRSpeedRamp.ResetCounter(&LRSpeedRamp);
	FBSpeedRamp.ResetCounter(&FBSpeedRamp);
	
	//机械臂电机目标物理角度值
	AMUD1AngleTarget = 0.0;
	AMUD2AngleTarget = 0.0;
	AMFBAngleTarget = 0.0;
	AMSIDEAngleTarget = 0.0;
	WINDAngleTarget = 0.0;
	/*底盘速度初始化*/
	ChassisSpeedRef.forward_back_ref = 0.0f;
	ChassisSpeedRef.left_right_ref = 0.0f;
	ChassisSpeedRef.rotate_ref = 0.0f;
}

//摇杆控制量解算
void RemoteControlProcess(Remote *rc)
{
	int16_t channel0 = (rc->ch0 - (int16_t)REMOTE_CONTROLLER_STICK_OFFSET) * STICK_TO_CHASSIS_SPEED_REF_FACT; //右横
	int16_t channel1 = (rc->ch1 - (int16_t)REMOTE_CONTROLLER_STICK_OFFSET) * STICK_TO_CHASSIS_SPEED_REF_FACT; //右纵
	int16_t channel2 = (rc->ch2 - (int16_t)REMOTE_CONTROLLER_STICK_OFFSET) * STICK_TO_CHASSIS_SPEED_REF_FACT; //左横
	int16_t channel3 = (rc->ch3 - (int16_t)REMOTE_CONTROLLER_STICK_OFFSET) * STICK_TO_CHASSIS_SPEED_REF_FACT; //左纵
	
	if(WorkState == NORMAL_STATE)
	{
		ChassisSpeedRef.forward_back_ref = channel1;
		ChassisSpeedRef.left_right_ref   = channel0;
		
		rotate_speed = channel2 * ROTATE_FACTOR;
		
		if(STEPMODE==0)
		{
		if(channel3 > IGNORE_RANGE) AMSIDEAngleTarget = ANGLE_STEP;
		else if(channel3 < -IGNORE_RANGE) AMSIDEAngleTarget =- ANGLE_STEP;
		else AMSIDEAngleTarget =0;
		}
		else
		{
		if(channel3 > IGNORE_RANGE) AMSIDEAngleTarget += ANGLE_STEP/20;
		else if(channel3 < -IGNORE_RANGE) AMSIDEAngleTarget -= ANGLE_STEP/20;
		}
	}
	if(WorkState == GETBULLET_STATE || WorkState == BYPASS_STATE)
	{
		ChassisSpeedRef.forward_back_ref = 0;
		ChassisSpeedRef.left_right_ref   = channel0;
		
		if(STEPMODE==0)
		{
		if(channel3 > IGNORE_RANGE) {AMUD1AngleTarget=ANGLE_STEP;AMUD2AngleTarget=ANGLE_STEP;}
		else if(channel3 < -IGNORE_RANGE) {AMUD1AngleTarget=-ANGLE_STEP;AMUD2AngleTarget=-ANGLE_STEP;}
		else {AMUD1AngleTarget =0;AMUD2AngleTarget =0;}
		if(channel1 > IGNORE_RANGE) AMFBAngleTarget = ANGLE_STEP;
		else if(channel1 < -IGNORE_RANGE) AMFBAngleTarget =- ANGLE_STEP;
		else AMFBAngleTarget =0;
		if(channel2 > IGNORE_RANGE) WINDAngleTarget = ANGLE_STEP;
		else if(channel2 < -IGNORE_RANGE) WINDAngleTarget =- ANGLE_STEP;
		else WINDAngleTarget =0;
		}
		else
		{
		if(channel3 > IGNORE_RANGE) {AMUD1AngleTarget+=ANGLE_STEP/20;AMUD2AngleTarget-=ANGLE_STEP/20;}
		else if(channel3 < -IGNORE_RANGE) {AMUD1AngleTarget-=ANGLE_STEP/20;AMUD2AngleTarget-=ANGLE_STEP/20;}
		if(channel1 > IGNORE_RANGE) AMFBAngleTarget += ANGLE_STEP/20;
		else if(channel1 < -IGNORE_RANGE) AMFBAngleTarget -= ANGLE_STEP/20;
		if(channel2 > IGNORE_RANGE) WINDAngleTarget += ANGLE_STEP/20;
		else if(channel2 < -IGNORE_RANGE) WINDAngleTarget -= ANGLE_STEP/20;
		}
	}
}

//键鼠控制量解算
void MouseKeyControlProcess(Mouse *mouse, Key *key)
{
	static uint16_t forward_back_speed = 0;
	static uint16_t left_right_speed = 0;
	if(WorkState == NORMAL_STATE)
	{
		VAL_LIMIT(mouse->x, -150, 150); 
		VAL_LIMIT(mouse->y, -150, 150); 

		//speed mode: normal speed/high speed 
		if(key->v & 0x10)//Shift
		{
			forward_back_speed =  LOW_FORWARD_BACK_SPEED;
			left_right_speed = LOW_LEFT_RIGHT_SPEED;
		}
		else if(key->v == 32)//Ctrl
		{
			forward_back_speed =  MIDDLE_FORWARD_BACK_SPEED;
			left_right_speed = MIDDLE_LEFT_RIGHT_SPEED;
		}
		else
		{
			forward_back_speed =  NORMAL_FORWARD_BACK_SPEED;
			left_right_speed = NORMAL_LEFT_RIGHT_SPEED;
		}
		//movement process
		if(key->v & 0x01)  // key: w
		{
			ChassisSpeedRef.forward_back_ref = forward_back_speed* FBSpeedRamp.Calc(&FBSpeedRamp);
			
		}
		else if(key->v & 0x02) //key: s
		{
			ChassisSpeedRef.forward_back_ref = -forward_back_speed* FBSpeedRamp.Calc(&FBSpeedRamp);
			
		}
		else
		{
			ChassisSpeedRef.forward_back_ref = 0;
			FBSpeedRamp.ResetCounter(&FBSpeedRamp);
		}
		if(key->v & 0x04)  // key: d
		{
			ChassisSpeedRef.left_right_ref = -left_right_speed* LRSpeedRamp.Calc(&LRSpeedRamp);
			
		}
		else if(key->v & 0x08) //key: a
		{
			ChassisSpeedRef.left_right_ref = left_right_speed* LRSpeedRamp.Calc(&LRSpeedRamp);
			
		}
		else
		{
			ChassisSpeedRef.left_right_ref = 0;
			LRSpeedRamp.ResetCounter(&LRSpeedRamp);
		}

		/*裁判系统离线时的功率限制方式*/
//		if(JUDGE_State == OFFLINE)
//		{
//			if(abs(ChassisSpeedRef.forward_back_ref) + abs(ChassisSpeedRef.left_right_ref) > 500)
//			{
//				if(ChassisSpeedRef.forward_back_ref > 325)
//				{
//				ChassisSpeedRef.forward_back_ref =  325 +  (ChassisSpeedRef.forward_back_ref - 325) * 0.15f;
//				}
//				else if(ChassisSpeedRef.forward_back_ref < -325)
//				{
//				ChassisSpeedRef.forward_back_ref =  -325 +  (ChassisSpeedRef.forward_back_ref + 325) * 0.15f;
//				}
//				if(ChassisSpeedRef.left_right_ref > 300)
//				{
//				ChassisSpeedRef.left_right_ref =  300 +  (ChassisSpeedRef.left_right_ref - 300) * 0.15f;
//				}
//				else if(ChassisSpeedRef.left_right_ref < -300)
//				{
//				ChassisSpeedRef.left_right_ref =  -300 +  (ChassisSpeedRef.left_right_ref + 300) * 0.15f;
//				}
//			}

//			if ((mouse->x < -2.6) || (mouse->x > 2.6))
//			{
//				if(abs(ChassisSpeedRef.forward_back_ref) + abs(ChassisSpeedRef.left_right_ref) > 400)
//				{
//					if(ChassisSpeedRef.forward_back_ref > 250){
//					 ChassisSpeedRef.forward_back_ref =  250 +  (ChassisSpeedRef.forward_back_ref - 250) * 0.15f;
//					}
//					else if(ChassisSpeedRef.forward_back_ref < -250)
//					{
//						ChassisSpeedRef.forward_back_ref =  -250 +  (ChassisSpeedRef.forward_back_ref + 250) * 0.15f;
//					}
//					if(ChassisSpeedRef.left_right_ref > 250)
//					{
//					 ChassisSpeedRef.left_right_ref =  250 +  (ChassisSpeedRef.left_right_ref - 250) * 0.15f;
//					}
//					else if(ChassisSpeedRef.left_right_ref < -250)
//					{
//						ChassisSpeedRef.left_right_ref =  -250 +  (ChassisSpeedRef.left_right_ref + 250) * 0.15f;
//					}
//				}
//			}
//		}
	}
}


/*拨杆数据处理*/   
void GetRemoteSwitchAction(RemoteSwitch_t *sw, uint8_t val)
{
	static uint32_t switch_cnt = 0;

	sw->switch_value_raw = val;
	sw->switch_value_buf[sw->buf_index] = sw->switch_value_raw;

	//value1 value2的值其实是一样的
	//value1高4位始终为0
	sw->switch_value1 = (sw->switch_value_buf[sw->buf_last_index] << 2)|
	(sw->switch_value_buf[sw->buf_index]);

	sw->buf_end_index = (sw->buf_index + 1)%REMOTE_SWITCH_VALUE_BUF_DEEP;

	sw->switch_value2 = (sw->switch_value_buf[sw->buf_end_index]<<4)|sw->switch_value1;	

	//如果两次数据一样，即没有更新数据，拨杆不动
	if(sw->switch_value_buf[sw->buf_index] == sw->switch_value_buf[sw->buf_last_index])
	{
		switch_cnt++;	
	}
	else
	{
		switch_cnt = 0;
	}
	//如果拨杆维持了一定时间，即连续来了40帧一样的数据，则把拨杆数据写入switch_long_value
	if(switch_cnt >= 40)
	{
		sw->switch_long_value = sw->switch_value_buf[sw->buf_index]; 	
	}
	//指向下一个缓冲区
	sw->buf_last_index = sw->buf_index;
	sw->buf_index++;		
	if(sw->buf_index == REMOTE_SWITCH_VALUE_BUF_DEEP)
	{
		sw->buf_index = 0;	
	}			
}


//遥控器数据解算
void RemoteDataProcess(uint8_t *pData)
{
	if(pData == NULL)
	{
			return;
	}
	//遥控器 11*4 + 2*2 = 48，需要 6 Bytes
	//16位，只看低11位
	RC_CtrlData.rc.ch0 = ((int16_t)pData[0] | ((int16_t)pData[1] << 8)) & 0x07FF; 
	RC_CtrlData.rc.ch1 = (((int16_t)pData[1] >> 3) | ((int16_t)pData[2] << 5)) & 0x07FF;
	RC_CtrlData.rc.ch2 = (((int16_t)pData[2] >> 6) | ((int16_t)pData[3] << 2) |
											 ((int16_t)pData[4] << 10)) & 0x07FF;
	RC_CtrlData.rc.ch3 = (((int16_t)pData[4] >> 1) | ((int16_t)pData[5]<<7)) & 0x07FF;
	
	//16位，只看最低两位
	RC_CtrlData.rc.s1 = ((pData[5] >> 4) & 0x000C) >> 2;
	RC_CtrlData.rc.s2 = ((pData[5] >> 4) & 0x0003);

	//鼠标需要 8 Bytes
	RC_CtrlData.mouse.x = ((int16_t)pData[6]) | ((int16_t)pData[7] << 8);
	RC_CtrlData.mouse.y = ((int16_t)pData[8]) | ((int16_t)pData[9] << 8);
	RC_CtrlData.mouse.z = ((int16_t)pData[10]) | ((int16_t)pData[11] << 8);    

	RC_CtrlData.mouse.press_l = pData[12];
	RC_CtrlData.mouse.press_r = pData[13];
	
	//键盘需要 2 Bytes = 16 bits ，每一位对应一个键
	RC_CtrlData.key.v = ((int16_t)pData[14]) | ((int16_t)pData[15] << 8);

	//输入状态设置
	if(RC_CtrlData.rc.s2 == 1) inputmode = REMOTE_INPUT; 
	else if(RC_CtrlData.rc.s2 == 3) inputmode = KEY_MOUSE_INPUT; 
	else inputmode = STOP; 
	
	//功能状态设置
	if(RC_CtrlData.rc.s1 == 1) functionmode = NORMAL; 
	else if(RC_CtrlData.rc.s1 == 3) functionmode = GET; 
	else functionmode = AUTO; 
	
	/*左上角拨杆状态（RC_CtrlData.rc.s1）获取*/	//用于遥控器发射控制
	GetRemoteSwitchAction(&g_switch1, RC_CtrlData.rc.s1);
	
	switch(inputmode)
	{
		case REMOTE_INPUT:               
		{
			__HAL_TIM_SET_COMPARE(&BYPASS_TIM, TIM_CHANNEL_1,1300);//方便调试，暂时加入，值为1000时不转动
			if(WorkState != STOP_STATE && WorkState != PREPARE_STATE)
			{ 
				RemoteControlProcess(&(RC_CtrlData.rc));
			}
		}break;
		case KEY_MOUSE_INPUT:              
		{
			__HAL_TIM_SET_COMPARE(&BYPASS_TIM, TIM_CHANNEL_1,1000);//方便调试，暂时加入，值为1000时不转动
			if(WorkState == NORMAL_STATE)
			{ 
				MouseKeyControlProcess(&RC_CtrlData.mouse,&RC_CtrlData.key);
			}
		}break;
		case STOP:               
		{
			 
		}break;
	}
}

//初始化遥控器串口DMA接收
void InitRemoteControl(){
	if(HAL_UART_Receive_DMA(&RC_UART, rc_data, 18) != HAL_OK){
			Error_Handler();
	} 
	RemoteTaskInit();
}

//遥控器串口中断入口函数，从此处开始执行
uint8_t rc_first_frame = 0;
uint8_t rc_update = 0;
uint8_t rc_cnt = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
	if(UartHandle == &RC_UART){
		rc_update = 1;
	}
	else if(UartHandle == &MANIFOLD_UART)
	{
		//manifoldUartRxCpltCallback();  //妙算信号数据解算
		#ifdef DEBUG_MODE
		ctrlUartRxCpltCallback();
		#endif
	}
	else if(UartHandle == &JUDGE_UART)
	{
		judgeUartRxCpltCallback();  //裁判系统数据解算
	}
}   
