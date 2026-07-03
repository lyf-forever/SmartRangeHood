#include "motor.h"
#include "gpio.h"
#include "tim.h"
#include <string.h>

/***********************
*项目名：电机驱动源文件
*作者：Lyf
*闲鱼号：tb43915564
*修改日期：2026/2/1
*项目已申请版权，请勿倒卖！
*当前市面基本是HAL库编写的电机驱动，本驱动为手敲标准库，如果需要hal库，请查看相关资料
************************/ 

#if MOTOR_IS_USE  /* 使用电机 */
#if (MOTOR_TYPE == 0) /* 无刷直流电机BLDC */

/*-----------------------------------------------------------
 * 全局变量
 *----------------------------------------------------------*/
volatile int32_t overflow = 0;		/* 溢出计数器 */
volatile float   speed = 0.0f;     	/* 电机实际转速 */

/* MOTOR_DRIVE_TIM_CH1输出GPIO */
static const periph_gpio_t motorDrv_tim_ch_gpio = {
    .port  = MOTOR_DRIVE_TIM_CH_PORT,
    .pin   = MOTOR_DRIVE_TIM_CH_PIN,
    .mode  = GPIO_Mode_AF_PP
},

/* MOTOR_DRIVE_TIM_CH1N互补输出GPIO */
motorDrv_tim_chn_gpio = {
    .port  = MOTOR_DRIVE_TIM_CHN_PORT,
    .pin   = MOTOR_DRIVE_TIM_CHN_PIN,
    .mode  = GPIO_Mode_AF_PP
},

/* 电机运行控制GPIO */
motor_ctrl_gpio = {
    .port  = MOTOR_CTRL_PORT,
    .pin   = MOTOR_CTRL_PIN,
    .mode  = GPIO_Mode_Out_PP,
    .speed = GPIO_Speed_50MHz,
    .initial_level = lowLevel
},

/* TIM_ENCODER_CH1输入GPIO */
tim_encoder_chA_gpio = {
    .port  = TIM_ENCODER_CHA_PORT,
    .pin   = TIM_ENCODER_CHA_PIN,
    .mode  = GPIO_Mode_IPD,
    .speed = GPIO_Speed_50MHz,
},

/* TIM_ENCODER_CH2输入GPIO */
tim_encoder_chB_gpio = {
    .port  = TIM_ENCODER_CHB_PORT,
    .pin   = TIM_ENCODER_CHB_PIN,
    .mode  = GPIO_Mode_IPD,
    .speed = GPIO_Speed_50MHz,
};

/* 频率200Hz TIM用于速度计算 */
static TIM_Config_t speedcacl_tim_cfg = {
    .TIMx  = SpeedCacl_TIM,
    .Mode  = TIM_MODE_BASIC,
    .Prescaler = 14400 - 1, 
    .Period    = 72000000/14400/SpeedCacl_TIM_Freq - 1,     
    .PrePrio   = 5,
    .SubPrio   = 0
};

motor_dir_t motor_dir = straight;     /* 电机转动方向定义 */

/*
 * @brief: 直流有刷电机驱动 - MOTOR_DRIVE_TIM
 * @param: uint16_t arr - 自动重装载值
 * @param: uint16_t psc - 预分频值
 * @param: uint16_t ccr - 初始比较值(占空比)
 * @param: uint16_t dtg - 死区时间
 * @return: none
 */
void TIM_motorDrive_Init(uint16_t arr, uint16_t psc, uint16_t ccr, uint16_t dtg)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStruct;   /* 定时器时基结构体定义 */
    TIM_OCInitTypeDef  TIM_OCInitStruct;           /* 定时器输出比较结构体定义 */
    TIM_BDTRInitTypeDef  TIM_BDTRInitStruct;       /* 定时器BDTR结构体定义 */

    RCC_APB2PeriphClockCmd(RCC_MOTOR_DRIVE_TIM, ENABLE);   // 使能MOTOR_DRIVE_TIM时钟
    io_set(&motorDrv_tim_ch_gpio);     // CH1输出引脚
    io_set(&motorDrv_tim_chn_gpio);    // CH1N互补输出引脚
    io_set(&motor_ctrl_gpio);          // 电机使能控制IO初始化

    /* 初始化MOTOR_DRIVE_TIM时基单元 */
    TIM_TimeBaseStruct.TIM_Period = arr; 					             // 设置在下一个更新事件装入自动重装载寄存器周期的值
	TIM_TimeBaseStruct.TIM_Prescaler = psc; 			                 // 设置用来作为TIMx时钟频率除数的预分频值 
	TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV4; 	             // CKD[1:0] = 10, tDTS = 4 * tCK_INT
	TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;             // TIM向上计数模式
	TIM_TimeBaseInit(MOTOR_DRIVE_TIM, &TIM_TimeBaseStruct); 	         // 根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

    /* 初始化MOTOR_DRIVE_TIM输出比较单元 */
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;                       // 选择定时器输出比较模式:TIM脉冲宽度调制(PWM)模式1
	TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High; 	             // 输出极性:TIM输出比较极性高
	TIM_OCInitStruct.TIM_OCNPolarity = TIM_OCPolarity_High; 	         // 互补输出极性:TIM输出比较极性高
 	TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable; 	         // 比较输出使能
	TIM_OCInitStruct.TIM_OutputNState = TIM_OutputNState_Enable;         // 互补输出使能
	TIM_OCInitStruct.TIM_Pulse = ccr;							         // 设置初始占空比
	TIM_OC1Init(MOTOR_DRIVE_TIM, &TIM_OCInitStruct);  				     // 根据指定的参数初始化MOTOR_DRIVE_TIM OC1
	TIM_OC1PreloadConfig(MOTOR_DRIVE_TIM, TIM_OCPreload_Enable);  	     // 使能MOTOR_DRIVE_TIM在CCR上的预装载寄存器

    /* BDTR初始化配置 */
    TIM_BDTRStructInit(&TIM_BDTRInitStruct);                             // 将BDTR参数初始化为默认值
	TIM_BDTRInitStruct.TIM_DeadTime = dtg;								 // 设置死区时间
	TIM_BDTRInitStruct.TIM_OSSRState = TIM_OSSRState_Enable;			 // OSSR位设置为1
	TIM_BDTRInitStruct.TIM_OSSIState = TIM_OSSIState_Disable;			 // OSSI位设置为0
    TIM_BDTRInitStruct.TIM_Break = TIM_Break_Disable;					 // 注意：在本例程中没有用到刹车
    TIM_BDTRInitStruct.TIM_BreakPolarity = TIM_BreakPolarity_Low; 		 // BKIN低电平触发刹车
    TIM_BDTRInitStruct.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable; // 使能AOE位，刹车后自动恢复输出
    TIM_BDTRConfig(MOTOR_DRIVE_TIM, &TIM_BDTRInitStruct);				 // BDTR初始化

    /*使能MOTOR_DRIVE_TIM的通道输出、使能MOTOR_DRIVE_TIM */ 
	TIM_CtrlPWMOutputs(MOTOR_DRIVE_TIM, ENABLE);	// 使能MOE位
	TIM_Cmd(MOTOR_DRIVE_TIM, ENABLE);  		        // 使能/启动 MOTOR_DRIVE_TIM										
}

/* 控制电机停止转动 */
void motor_stop(void)
{
    TIM_CCxCmd(MOTOR_DRIVE_TIM, MOTOR_DRIVE_TIM_CH, TIM_CCx_Disable);   // 关闭CH1通道PWM输出
    TIM_CCxNCmd(MOTOR_DRIVE_TIM, MOTOR_DRIVE_TIM_CH, TIM_CCxN_Disable); // 关闭CH1N通道PWM输出
    io_reset_bit(&motor_ctrl_gpio);   // 禁用H桥驱动电路
}

/* 控制电机启动转动 */
void motor_start(void)
{
    io_set_bit(&motor_ctrl_gpio);     // 启用H桥驱动电路
}

/* 电机转动方向控制 */
void motor_dir_ctrl(motor_dir_t dir)
{
    /* 先关闭定时器PWM输出 */
    TIM_CCxCmd(MOTOR_DRIVE_TIM, MOTOR_DRIVE_TIM_CH, TIM_CCx_Disable);    // 关闭CH1通道PWM输出
    TIM_CCxNCmd(MOTOR_DRIVE_TIM, MOTOR_DRIVE_TIM_CH, TIM_CCxN_Disable);  // 关闭CH1N通道PWM输出

    /* 根据预期控制方向来使能 通道输出/互补通道输出 */
    if(dir == straight) {
        TIM_CCxNCmd(MOTOR_DRIVE_TIM, MOTOR_DRIVE_TIM_CH, TIM_CCxN_Enable);   // 开启CH1N互补通道PWM输出
    } else if(dir == invert) {
        TIM_CCxCmd(MOTOR_DRIVE_TIM, MOTOR_DRIVE_TIM_CH, TIM_CCx_Enable);     // 开启CH1通道PWM输出
    }
}

/* 电机状态初始化 */
void motor_init(void)
{
    motor_dir_ctrl(motor_dir);
    motor_stop();
    motor_start();
}

/* 电机转速调节 */
void motor_speedCtrl(uint16_t ccr) 
{
    if(ccr <= 1000) {     /* 限幅在1000以内 */
        TIM_SetCompare1(MOTOR_DRIVE_TIM, ccr);   // 设置比较值来调整占空比
    } 
}

/* 电机控制 */
void motor_setPWMDuty(float para) 
{
    int val = (int)para;

    if (val >= 0) {
        motor_dir_ctrl(straight);         /* 正转 */
        motor_speedCtrl(val);
    } else {
        motor_dir_ctrl(invert);           /* 反转 */
        motor_speedCtrl(-val);
    }
}


/* ==============================编码器测速部分code======================================= */
/* TIM_ENCODER 编码器功能初始化 */
void TIM_motorEncoder_Init(uint16_t arr, uint16_t psc)
{
    TIM_TimeBaseInitTypeDef    TIM_TimeBaseInitStruct;
    TIM_ICInitTypeDef   TIM_ICInitStruct;
    NVIC_InitTypeDef   NVIC_InitStruct;

    /* 使能TIM_ENCODER时钟和配置相关GPIO */
    RCC_APB1PeriphClockCmd(RCC_TIM_ENCODER, ENABLE);  // 使能TIM_ENCODER时钟
    io_set(&tim_encoder_chA_gpio);    // 初始化TIM_ENCODER_CH1
    io_set(&tim_encoder_chB_gpio);    // 初始化TIM_ENCODER_CH2

    /* 配置TIM_ENCODER参数 */
    TIM_TimeBaseInitStruct.TIM_Period = arr;                        // 自动重载值
    TIM_TimeBaseInitStruct.TIM_Prescaler = psc;                     // 分频系数
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;        // 时钟不分割（分频）
    TIM_TimeBaseInit(TIM_ENCODER, &TIM_TimeBaseInitStruct);         // 初始化TIM_ENCODER的时基单元

    /* 配置编码器，此处选择4倍频，需要TI1(A相)和TI2(B相)进行计数 */
    TIM_ICInitStruct.TIM_Channel = TIM_ENCODER_A_CH;				// 选择CH1			
	TIM_ICInitStruct.TIM_ICFilter = 10;							    // 输入滤波器
    TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_Rising; 	    // 选择是否反相，不反相
    TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;	// 选择输入通道，对应寄存器CCIS[1:0]
    TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;			    // 不分频，每一个边沿触发一次捕获
	TIM_ICInit(TIM_ENCODER,&TIM_ICInitStruct);					    // 初始化TIM_ENCODER输入捕获接口
	
	TIM_ICInitStruct.TIM_Channel = TIM_ENCODER_B_CH;				// 选择CH2			
	TIM_ICInitStruct.TIM_ICFilter = 10;							    // 输入滤波器
    TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_Rising; 	    // 选择是否反相，不反相
    TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;	// 选择输入通道，对应寄存器CCIS[1:0]
    TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;			    // 不分频，每一个边沿触发一次捕获
	TIM_ICInit(TIM_ENCODER,&TIM_ICInitStruct);						// 初始化TIM_ENCODER输入捕获接口

    /* 配置编码器接口，选择TI1和TI2同时计数 */
	TIM_EncoderInterfaceConfig(TIM_ENCODER, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	
	/* 配置TIM_ENCODER中断，因为计数器为65536，可能会溢出所以配置中断 */
	NVIC_InitStruct.NVIC_IRQChannel = TIM_ENCODER_IRQn;  			// TIM_ENCODER中断
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 6;  		// 抢占优先级
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;  			    // 子优先级0级
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE; 				    // IRQ通道被使能
	NVIC_Init(&NVIC_InitStruct);  								    // 根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器
	
	TIM_ITConfig(TIM_ENCODER, TIM_IT_Update, ENABLE); 				// 使能指定的TIM_ENCODER中断，允许更新中断

	TIM_Cmd(TIM_ENCODER, ENABLE);  								    // 使能TIM_ENCODER
}

/* 定时器SpeedCacl_TIM初始化，用于计算转速 */
void TIM_motorSpeedCalc_Init(void) 
{
    TIM_GeneralInit(&speedcacl_tim_cfg);
}

#if 1
/* 获取编码器计数值 */
int32_t get_encoder_value(void)
{
    /* 直接以有符号32位计算，类型一致，无隐式转换 */
    return (int32_t)TIM_GetCounter(TIM_ENCODER) + (overflow * 65536);
}
#else
/* 获取编码器计数值 */
int get_encoder_value(void)
{
	u32 buffer;
	buffer = TIM_GetCounter(TIM_ENCODER) + (overflow * 65536);
	return buffer;
}
#endif

/* 获取计数方向 */
uint8_t TIM_GetDirection(TIM_TypeDef* TIMx)
{
    return (TIMx->CR1 & TIM_CR1_DIR) ? 1 : 0;	/* 在编码器模式下，定时器的计数方向变为只读 */
}

#if 1
/**
 * @brief  获取电机转速（RPM）
 * @param  encoder_value  当前编码器计数值（32 位扩展值，已处理溢出）
 * @param  ms             采样周期（毫秒），要求主调函数以 200Hz 频率调用本函数
 * @return 一阶低通滤波后的转速（rpm）
 */
float motor_getSpeed(int32_t encoder_value, uint16_t ms)
{
    static float  filtered_speed = 0.0f;       // 一阶滤波输出
    static uint16_t tick = 0;                  // 时基计数器（1kHz 调用的次数）
    static int32_t last_encoder;               // 上一次采样时刻的编码器值
    //static bool  is_first = true;            // 首次调用标记
    static float  speedBuff[10] = {0};         // 速度采样缓冲
    static uint8_t buf_idx = 0;                // 缓冲区索引

    // 递增时基，当未到采样时刻时直接返回上次滤波结果
    tick++;
    if (tick < ms) { 
        return filtered_speed;
    }
    tick = 0;   // 精确重置计数

    // 计算本次脉冲增量
    int32_t delta = encoder_value - last_encoder;
    last_encoder = encoder_value;

    // 计算原始速度（RPM）
    // 编码器 11 线，4 倍频，减速比 30
    const float pulse_per_rev = 11.0f * 4.0f * 30.0f;   // 1320
    float raw_speed = (float)delta * (1000.0f / ms) * 60.0f / pulse_per_rev;

    // 存入缓冲区
    speedBuff[buf_idx++] = raw_speed;

    // 当缓冲区满（10 次）时，进行中值滤波 + 一阶低通
    if (buf_idx >= 10) {
        buf_idx = 0;

        // 冒泡排序（10 个元素）
        float sorted[10];
        memcpy(sorted, speedBuff, sizeof(sorted));
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 9 - i; j++) {
                if (sorted[j] > sorted[j + 1]) {
                    float tmp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = tmp;
                }
            }
        }

        // 去除最小 2 个和最大 2 个，取中间 6 个平均
        float sum = 0.0f;
        for (int i = 2; i < 8; i++) {
            sum += sorted[i];
        }
        float median_avg = sum / 6.0f;

        // 一阶低通滤波：Y(n) = q * X(n) + (1-q) * Y(n-1)
        const float q = 0.48f;
        filtered_speed = q * median_avg + (1.0f - q) * filtered_speed;
    }

    return filtered_speed;
}
#else
/* 获取电机转速。ms=50表示每50ms计算一次速度，即采样精度为50ms*/
float motor_getSpeed(int encoder_value,u16 ms)
{
	u8 i = 0, j = 0;
    float temp = 0.0;
	static float speed = 0;									/* 需要为静态变量，因为一阶滤波算法需要上次的滤波值 */
    static uint8_t sp_count = 0, k = 0;
    static float speed_arr[10] = {0.0};                     // 存储速度进行滤波数组 
	static int old_value = 0, now_value = 0;

    if (sp_count == ms)                                     // 计算一次速度 
    {
		now_value = encoder_value;							// 记录当前编码器值
		
        // 计算转速，30为减速比，4倍频，11线
		
		/* 1000/ms指在这个ms时间内获得了xx脉冲变化值，用xx变化值/ms得到1ms的脉冲变化值, 再乘以1000得到1s的变化值 */
        speed_arr[k++] = (float)((now_value - old_value) * ((1000 / ms) * 60.0) / 30 / (11*4)); 
		old_value = now_value;								// 保存当前计数值
		
        /* 累计10次速度值，利用冒泡排序，后面做中值滤波 */
        if (k == 10)
        {
            for (i = 10; i >= 1; i--)                       
            {
                for (j = 0; j < (i - 1); j++) 
                {
                    if (speed_arr[j] > speed_arr[j + 1])    /* 数值比较 */
                    { 
                        temp = speed_arr[j];                /* 数值换位 */
                        speed_arr[j] = speed_arr[j + 1];
                        speed_arr[j + 1] = temp;
                    }
                }
            }
            
            temp = 0.0f;
            
            for (i = 2; i < 8; i++)                         /* 去掉最高最低的数据 */
            {
                temp += speed_arr[i];                       /* 将中间数值累加 */
            }
            
            temp = (float)(temp / 6);                       /* 求速度平均值 */
            
            /* 一阶低通滤波
             * 公式为：Y(n)= qX(n) + (1-q)Y(n-1)
             * 其中X(n)为本次采样值，Y(n-1)为上次滤波输出值，Y(n)为本次滤波输出值，q为滤波系数
             * q值越小，上一次输出对本次输出的影响越大，输出越平稳，但是对速度变化的响应也就越慢
             */
            speed = (float)(((float)0.48 * temp) + (speed * (float)0.52));
            k = 0;
        }
        sp_count = 0;
    }
    sp_count ++;
	return speed;
}
#endif

/* 注意：TIM_ENCODER_IRQHandler和SpeedCacl_TIM_IRQHandler已移至app_tasks.c中
 * 原因：中断服务程序需要访问FreeRTOS信号量，统一放在应用层文件中管理 */

/* ==============================编码器测速部分code======================================= */

#elif (MOTOR_TYPE == 1) /* 有刷直流电机BDC */

#endif /* #if (MOTOR_TYPE == 0) */

#endif /* #if MOTOR_IS_USE */




