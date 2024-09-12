#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "usb_device.h"
#include "usb2pwm.h"
#include "usb2gpio.h"
#include <unistd.h>
#include <errno.h>

#define GPIO_PUPD_UP                  0x01    //使能上拉
#define GPIO_SUCCESS                          (0)   //函数执行成功  
#define AXIS_X 0
#define AXIS_Y 3
#define AXIS_Z 4

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
    
    //配置PWM相关参数并初始化PWM通道0,3,4的频率为10KHz
    PWM_CONFIG PWMConfig;
    PWMConfig.ChannelMask = 0x19;//初始化PWM0,3,4通道
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
    int RunTimeOfUs=move*10000;
    if(axis==0||axis==3||axis==4)
    {
        ret = PWM_Start(DevHandle[0],0x01<<axis,RunTimeOfUs);
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
    return 0;
}

int main() {
    int axis;
    float distance;
    int direction;

    // 提示用户选择轴
    printf("Select axis (0 for X, 3 for Y, 4 for Z): ");
    if (scanf("%d", &axis) != 1 || (axis != AXIS_X && axis != AXIS_Y && axis != AXIS_Z)) {
        printf("Invalid axis selection.\n");
        return EXIT_FAILURE;
    }

    // 提示用户输入距离
    printf("Enter distance to move (in units, must be greater than 0): ");
    if (scanf("%f", &distance) != 1 || distance <= 0) {
        printf("Invalid distance. It must be a positive number greater than 0.\n");
        return EXIT_FAILURE;
    }

    // 提示用户选择方向
    printf("Enter direction (1 for forward, 0 for backward): ");
    if (scanf("%d", &direction) != 1 || (direction != 0 && direction != 1)) {
        printf("Invalid direction. It must be 1 for forward or 0 for backward.\n");
        return EXIT_FAILURE;
    }

    // 调用函数生成方波
    int ret = generate_square_wave(distance, direction, axis);
    if (ret != 0) {
        printf("Error generating square wave.\n");
        return EXIT_FAILURE;
    }

    printf("Operation completed successfully.\n");

    return EXIT_SUCCESS;
}
