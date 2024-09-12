#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "usb_device.h"
#include "usb2pwm.h"
#include "usb2gpio.h"
#include <unistd.h>
#include <errno.h>
#include <sys/io.h>

#define GPIO_PUPD_UP                  0x01    //使能上拉
#define GPIO_SUCCESS                          (0)   //函数执行成功  
#define AXIS_X 0
#define AXIS_Y 3
#define AXIS_Z 4

//#define SPEED_X 
//#define SPEED_Y

static inline uint8_t gpio_bit_read(uint16_t addr, uint8_t bit){
	uint8_t value = 0;
	ioperm(addr,2,1);
	value = inb(addr);
	ioperm(addr,2,0);
	value = value & (1 << bit);
	value = value >> bit;
	return value;
}

//单轴运动
int generate_square_wave(float move,int dir,int axis)
{
    DEVICE_INFO DevInfo;
    int DevHandle[10];
    bool state;
    int ret;
    //扫描查找设备
    ret = USB_ScanDevice(DevHandle);
    if(ret <= 0){
        printf("No device connected!\n");
        return 0;
    }
    //打开设备
    state = USB_OpenDevice(DevHandle[0]);
    if(!state){
        printf("Open device error!\n");
        return 0;
    }
    //获取固件信息
    char FuncStr[256]={0};
    state = DEV_GetDeviceInfo(DevHandle[0],&DevInfo,FuncStr);
    if(!state){
        printf("Get device infomation error!\n");
        return 0;
    }else{
        printf("Firmware Info:\n");
        printf("    Name:%s\n",DevInfo.FirmwareName);
        printf("    Build Date:%s\n",DevInfo.BuildDate);
        printf("    Version:v%d.%d.%d\n",(DevInfo.FirmwareVersion>>24)&0xFF,(DevInfo.FirmwareVersion>>16)&0xFF,DevInfo.FirmwareVersion&0xFFFF);
        printf("    Version:v%d.%d.%d\n",(DevInfo.HardwareVersion>>24)&0xFF,(DevInfo.HardwareVersion>>16)&0xFF,DevInfo.HardwareVersion&0xFFFF);
        printf("    Functions:%08X\n",DevInfo.Functions);
        printf("    Functions:%s\n",FuncStr);
    }
    
    //初始化GPIO引脚并配置为上拉输出
    ret = GPIO_SetOutput(DevHandle[0],0x000e,GPIO_PUPD_UP);  
    if(ret != GPIO_SUCCESS){  
        printf("配置引脚失败\n");  
        //return;  
    }  
    
    //写入GPIO电平
    //para2:每一位数据对应一个引脚，当该位为1时影响引脚输出，当该位为0时不影响引脚输出
    //para3:每一位数据对应一个引脚，当该位为1且para2对应的该位为1时输出高电平，当该位为0且para2对应的该位为1时输出低电平
    if(dir)
    {
        ret = GPIO_Write(DevHandle[0],0x000e,0x000e);  
    }
    else
    {
        ret = GPIO_Write(DevHandle[0],0x000e,0x0000);  
    }
    if(ret != GPIO_SUCCESS){  
        printf("控制引脚失败\n");  
        //return;  
    }  
    
    //配置PWM相关参数并初始化PWM通道0的频率为10KHz，出于未知原因PWM通道1和2的频率为5KHz
    PWM_CONFIG PWMConfig;
    PWMConfig.ChannelMask = 0x19;//初始化PWM0,1,2通道
    for(int i=0;i<8;i++){
        PWMConfig.Polarity[i] = 1;//将所有PWM通道都设置为正极性
    }
    for(int i=0;i<8;i++){
        PWMConfig.Precision[i] = 100;//将所有通道的占空比调节精度都设置为1%
    }
    for(int i=0;i<8;i++){
        PWMConfig.Prescaler[i] = 168;//将所有通道的预分频器都设置为10，则PWM输出频率为168MHz/(PWMConfig.Precision*PWMConfig.Prescaler)
    }
    for(int i=0;i<8;i++){
        PWMConfig.Pulse[i] = PWMConfig.Precision[i]*50/100;//将所有通道的占空比都设置为50%
    }
    //初始化PWM
    ret = PWM_Init(DevHandle[0],&PWMConfig);
    if(ret != PWM_SUCCESS){
        printf("Initialize pwm faild!\n");
        return ret;
    }else{
        printf("Initialize pwm sunccess!\n");
    }
    //配置相位,根据Precision值来设置，最大值不能大于Precision
    unsigned short Phase[8]={0};
    Phase[0] = 0;
    Phase[1] = 0;
    Phase[2] = 0;
    Phase[3] = 0;
    ret = PWM_SetPhase(DevHandle[0],PWMConfig.ChannelMask,Phase);
    if(ret != PWM_SUCCESS){
        printf("Set pwm phase faild!\n");
        return ret;
    }else{
        printf("Set pwm phase sunccess!\n");
    }

    //启动PWM,RunTimeOfUs代表输出PWM的us数，即一共输出RunTimeOfUs/100个脉冲
    int RunTimeOfUs=move*100000;
    if(axis==0||axis==3||axis==4)
    {
        ret = PWM_Start(DevHandle[0],(0x01<<axis),RunTimeOfUs);
    }
    else
    {
        printf("error axis\n");
        return 0;
    } 
    if(ret != PWM_SUCCESS){
        printf("Start pwm faild!\n");
        return ret;
    }else{
        printf("Start pwm sunccess!\n");
    }

    //改变PWM波形占空比
    /*uint16_t Plse[8];
    for(int i=0;i<100;i+=5){
        for(int j=0;j<8;j++){
            Plse[j] = i;
        }
        ret = PWM_SetPulse(DevHandle[0],PWMConfig.ChannelMask,Plse);
        if(ret != PWM_SUCCESS){
            printf("Start pwm faild!\n");
            return ret;
        }
    }
    printf("Set pulse success!\n");*/

    //停止PWM
    /*ret = PWM_Stop(DevHandle[0],PWMConfig.ChannelMask);
    if(ret != PWM_SUCCESS){
        printf("Stop pwm faild!\n");
        return ret;
    }else{
        printf("Stop pwm sunccess!\n");
    }*/

    //关闭设备
    USB_CloseDevice(DevHandle[0]);
	return 0;
}

int Move_Stop()
{
    DEVICE_INFO DevInfo;
    int DevHandle[10];
    bool state;
    int ret;
    //扫描查找设备
    ret = USB_ScanDevice(DevHandle);
    if(ret <= 0){
        printf("No device connected!\n");
        return 0;
    }
    //打开设备
    state = USB_OpenDevice(DevHandle[0]);
    if(!state){
        printf("Open device error!\n");
        return 0;
    }

    ret = PWM_Stop(DevHandle[0],0x19);
    if(ret != PWM_SUCCESS){
        printf("Stop pwm faild!\n");
        return ret;
    }else{
        printf("Stop pwm sunccess!\n");
    }
    return 0;
}

int main()
{
    generate_square_wave(0,1,AXIS_X);
    while(1)
    {
        if(gpio_bit_read(0xA07,1)==1)
        {
            Move_Stop();
            break;
        }
    }
}