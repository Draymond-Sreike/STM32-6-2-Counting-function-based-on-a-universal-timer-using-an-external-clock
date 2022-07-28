#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"

int main()
{
	OLED_Init();
	timerInit();
	OLED_ShowString(1, 1, "Time:");
	OLED_ShowString(2, 1, "Counter:");
	while(1)
	{
		OLED_ShowNum(1, 6, getTimer2Time(),5);		//显示计数器的中断更新值，计数器获取外部时钟10次申请一次中断
		OLED_ShowNum(2, 9, getTimer2Counter(),5);	//显示计数器从0到9的装载过程值，记录的是计数器每次获得外部时钟的变化
	}
}
