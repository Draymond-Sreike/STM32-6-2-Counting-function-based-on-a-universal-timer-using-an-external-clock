#include "stm32f10x.h"                  // Device header

static uint16_t time;

void timerInit()
{
/*********GPIO初始化配置************/
	//这里为A0即将作为外部中断的输入口作准备
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	//GPIO_StructInit(&GPIO_InitStruct);//这里不是写这个函数！！！这个函数是初始化上面那个结构体的，不需要用！
	GPIO_Init(GPIOA, &GPIO_InitStruct); //这里应该写的是这个初始化函数，初始化GPIO，而非结构体里面的成员
	
	//该部分定时器我们选择定时器2，其是挂载在APB1总线上的，故这里选择的是APB1来开启RCC内部时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	//选择定时器为外部通过ETR进行出发，并将该触发供到TIM2，即定时器2上
	//通过ETR引脚的外部时钟模式2进行配置
	//第二个参数TIM_ExtTRGPrescaler是外部触发预分频器，因为我们不需要所以配TIM_ExtTRGPSC_OFF
	//第三个参数TIM_ExtTRGPolarity是外部触发极性选择
		//TIM_ExtTRGPolarity_Inverted是反向，即低电平或下降沿有效
			//这里设置后会发现，当红外对射模块被遮挡后离开的瞬间增加1
			//因为遮挡瞬间是产生上升沿的只有当遮挡物离开后产生了下降沿才会触发定时器计数
		//TIM_ExtTRGPolarity_NonInverted是正向，即高电平或上升沿有效
			//这里设置后会发现，当红外对射模块被遮挡瞬间才增加1，因为遮挡瞬间是产生上升沿的
	//第四个参数ExtTRGFilter是外部触发滤波，其设置将对应以频率f采样N个点，其对应关系见参考手册STM32F10XXX参考手册
//	//我们这里暂时不需要用到滤波器，选择该形参要求范围的一个值即可，这里写0x00，即无滤波
//	TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0x00);
//但是经过调试后，发现如果不滤波的话计数器的值的变化会很不稳定，所以这里还是适当地进行滤波处理,并且我们选择上升沿触发，即遮挡瞬间计数+1
	TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0x0A);
	
/*********时基单元参数配置************/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	//选择滤波的取样点相关参数进行配置，该项参数配置对该工程影响不打，随机取TIM_CKD_DIV1
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	//选择计数器的计数模式为向上计数，即从0开始递增计数
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	//配置自动重装载寄存器的参数，该参数配为：当计数器计数10次时进行重装
	TIM_TimeBaseInitStruct.TIM_Period = (10 - 1);
	//配置预分频器的参数，该参数配为分频1倍，即不分频，保持原频率
	//此时配合自动重装载寄存器的参数配置，可以使得计数器计外部时钟变化10次就进行重装载，并申请中断
	//即此配置下计数器每接收外部时钟变化10次申请一次中断
	TIM_TimeBaseInitStruct.TIM_Prescaler = (1 - 1);
	//通用寄存器中没有重复次数计数器，该参数配0即可
	TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
	//TIM_TimeBaseStructInit(&TIM_TimeBaseInitStruct); //此处犯错，不是使用这个函数！！！
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct); //应该用该函数！！！同时该函数执行完成后会有一个副作用：中断标志位会被置SET
	//后续如果不将该位清0，那么正式中断一开始就会导致马上执行一次中断函数，因此我们需要赶在正式中断开始前手动把中断标志位置0
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);	//此函数完成把中断标志位置0的工作
	
//至此，定时器2的时基单元参数配置完成

/*********中断输出控制参数配置************/
	//选择定时器2，并设置为“更新中断”模式（暂时不是太理解这个配置的作用）
	//此配置将打通定时器2与NVIC的连通
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

/*********NVIC输出控制参数配置************/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 3;
	NVIC_Init(&NVIC_InitStruct);//该语句适配完成后正式开启中断
	//如果此时中断标志位为SET，则不用动计数器开启，定时器2马上就会向CPU申请一次中断去执行中断函数
	//由此可体现出在TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct)完成后
	//执行TIM_ClearFlag(TIM2, TIM_FLAG_Update)的必要性
	
//运行控制，打开CNT计数器开始计数，
	TIM_Cmd(TIM2, ENABLE);
}

uint16_t getTimer2Counter()
{
	return TIM_GetCounter(TIM2);
}

uint16_t getTimer2Time()
{
	return time;
}

void TIM2_IRQHandler()
{
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		time++;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}	
