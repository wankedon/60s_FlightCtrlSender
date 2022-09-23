#include "CSerialPort.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

using namespace std;

#pragma pack(1)
struct FlightCtrlCode
{
	uint8_t syncCodeFirst;    //同步码  
	uint8_t syncCodeSecond;   //同步码  
	uint8_t year;             //年 2000年为0
	uint8_t month;            //月
	uint8_t day;              //日
	uint8_t hour;             //时
	uint8_t minute;           //分
	uint16_t msec;            //毫秒
	int longitude;            //经度
	int latitude;             //纬度
	int16_t altitude;         //海拔高度
	int16_t pitch;            //俯仰角
	int16_t roll;		      //滚转角
	uint16_t yaw;		      //偏航角
	uint16_t horSpeed;        //水平速度
	int16_t verSpeed;          //向上速度
	uint16_t trackDir;         //航迹方向
	uint8_t GPSFlag;           //GPS有效标志
	unsigned int GPSSec;       //GPS周内秒
	int16_t relaAltitude;       //相对起飞点高度
	int16_t ewSpeed;            //东西速度
	int16_t snSpeed;            //南北速度
	int16_t baroAltitude;       //气压高度
	uint8_t reserved[54];       //备用
	uint8_t  xorCheck;          //异或校验
	uint8_t  sumCheck;          //和校验
};
#pragma pack()

void createFlightCtrlData(std::vector<char>& flightData)
{
	FlightCtrlCode code;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	code.syncCodeFirst = 0xEE;
	code.syncCodeSecond = 0X90;
	code.year = wtm.wYear - 2000;
	code.month = wtm.wMonth;
	code.day = wtm.wDay;
	code.hour = wtm.wHour;
	code.minute = wtm.wMinute;
	code.msec = wtm.wSecond * 1000 + wtm.wMilliseconds;
	code.longitude = 120.1691 / (180 / (pow(2, 31) - 1));
	code.latitude = 35.97 / (90 / (pow(2, 31) - 1));
	code.altitude = 87.4 / 0.25;
	code.pitch = 1.26 / 0.01;
	code.roll = -3.03 / 0.01;
	code.yaw = 284.95 / 0.01;
	code.horSpeed = 350 / 0.1;
	code.verSpeed = 360 / 0.1;
	code.trackDir = 194.96 / 0.01;
	code.GPSFlag = 1;
	code.GPSSec = wtm.wSecond;
	code.relaAltitude = 100;
	code.ewSpeed = 1000;
	code.snSpeed = 1100;
	code.baroAltitude = 25;
	code.xorCheck = 55;
	code.sumCheck = 66;
	flightData.reserve(sizeof(FlightCtrlCode));
	memcpy(flightData.data(), &code, sizeof(FlightCtrlCode));
}

int main()
{
	CSerialPort mySerialPort;
	unsigned int  portNo = 3;
	unsigned int baud = 115200;
	if (!mySerialPort.InitPort(portNo, baud))
	{
		std::cout << "initPort fail !" << std::endl;
		system("pause");
		return 0;
	}
	else
	{
		std::cout << "initPort success !" << std::endl;
		std::cout << "portNo: " << portNo << " baud: " << baud << std::endl;
	}
	if (!mySerialPort.OpenListenThread())
	{
		std::cout << "OpenListenThread fail !" << std::endl;
		system("pause");
		return 0;
	}
	else
	{
		std::cout << "OpenListenThread success !" << std::endl;
	}

	std::vector<char> flightData;
	createFlightCtrlData(flightData);
	int i = 0;
	while (true)
	{
		if (mySerialPort.WriteData(flightData.data(), flightData.size()))
		{
			std::cout << "Send FlightCtrlInfo success " << ++i << std::endl;
		}
		else
		{
			std::cout << "----Send FlightCtrlInfo fail !" << std::endl;
		}
		Sleep(500);
	}
	system("pause");
	return 0;
}