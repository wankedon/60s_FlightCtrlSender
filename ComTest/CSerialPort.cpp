#include "CSerialPort.h"
//  
/// COPYRIGHT NOTICE  
/// Copyright (c) 2009, 华中科技大学tickTick Group  （版权声明）  
/// All rights reserved. 
///   
/// @file    SerialPort.cpp    
/// @brief   串口通信类的实现文件  
///  
/// 本文件为串口通信类的实现代码  
///  
/// @version 1.0    
/// @author  卢俊   
/// @E-mail：lujun.hust@gmail.com  
/// @date    2010/03/19
///   
///  
///  修订说明：  

#include "CSerialPort.h"  
#include <process.h>  
#include <iostream>  

bool CSerialPort::s_bExit = false; //线程退出标志
const UINT SLEEP_TIME_INTERVAL = 5;//当串口无数据时,sleep至下次查询间隔的时间,单位:秒 

CSerialPort::CSerialPort(void)
	: m_hListenThread(INVALID_HANDLE_VALUE)
{
	m_hComm = INVALID_HANDLE_VALUE;
	m_hListenThread = INVALID_HANDLE_VALUE;
	InitializeCriticalSection(&m_csCommunicationSync);
}

CSerialPort::~CSerialPort(void)
{
	CloseListenTread();
	ClosePort();
	DeleteCriticalSection(&m_csCommunicationSync);
}

bool CSerialPort::InitPort(UINT portNo /*= 1*/, UINT baud /*= CBR_9600*/, char parity /*= 'N'*/,
	UINT databits /*= 8*/, UINT stopsbits /*= 1*/, DWORD dwCommEvents /*= EV_RXCHAR*/)
{
	char szDCBparam[50];// 临时变量,将制定参数转化为字符串形式,以构造DCB结构
	sprintf_s(szDCBparam, "baud=%d parity=%c data=%d stop=%d", baud, parity, databits, stopsbits);
	if (!openPort(portNo))//打开指定串口,该函数内部已经有临界区保护,上面请不要加保护
	{
		return false;
	}
	EnterCriticalSection(&m_csCommunicationSync);//进入临界段
	BOOL bIsSuccess = TRUE;//是否有错误发生
	/** 在此可以设置输入输出的缓冲区大小,如果不设置,则系统会设置默认值.
	*  自己设置缓冲区大小时,要注意设置稍大一些,避免缓冲区溢出
	*/
	/*if (bIsSuccess )
	{
	bIsSuccess = SetupComm(m_hComm,10,10);
	}*/
	/** 设置串口的超时时间,均设为0,表示不使用超时限制 */
	COMMTIMEOUTS  CommTimeouts;
	CommTimeouts.ReadIntervalTimeout = 0;
	CommTimeouts.ReadTotalTimeoutMultiplier = 0;
	CommTimeouts.ReadTotalTimeoutConstant = 0;
	CommTimeouts.WriteTotalTimeoutMultiplier = 0;
	CommTimeouts.WriteTotalTimeoutConstant = 0;
	if (bIsSuccess)
	{
		bIsSuccess = SetCommTimeouts(m_hComm, &CommTimeouts);
	}
	DCB  dcb;
	if (bIsSuccess)
	{
		// 将ANSI字符串转换为UNICODE字符串  
		DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, NULL, 0);
		wchar_t* pwText = new wchar_t[dwNum];
		if (!MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, pwText, dwNum))
		{
			bIsSuccess = TRUE;
		}
		/** 获取当前串口配置参数,并且构造串口DCB参数 */
		bIsSuccess = GetCommState(m_hComm, &dcb) && BuildCommDCB(pwText, &dcb);
		dcb.fRtsControl = RTS_CONTROL_ENABLE;//开启RTS flow控制
		delete[] pwText;//释放内存空间
	}
	if (bIsSuccess)
	{
		bIsSuccess = SetCommState(m_hComm, &dcb);//使用DCB参数配置串口状态
	}
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);//清空串口缓冲区
	LeaveCriticalSection(&m_csCommunicationSync);//离开临界段
	return bIsSuccess == TRUE;
}

bool CSerialPort::InitPort(UINT portNo, const LPDCB& plDCB)
{
	if (!openPort(portNo))//打开指定串口,该函数内部已经有临界区保护,上面请不要加保护
	{
		return false;
	}
	EnterCriticalSection(&m_csCommunicationSync);//进入临界段
	if (!SetCommState(m_hComm, plDCB))//配置串口参数
	{
		return false;
	}
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);//清空串口缓冲区
	LeaveCriticalSection(&m_csCommunicationSync);//离开临界段
	return true;
}

void CSerialPort::ClosePort()
{
	//如果有串口被打开，关闭它
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hComm);
		m_hComm = INVALID_HANDLE_VALUE;
	}
}

bool CSerialPort::openPort(UINT portNo)
{
	EnterCriticalSection(&m_csCommunicationSync);//进入临界段
	char szPort[50];//把串口的编号转换为设备名
	sprintf_s(szPort, "COM%d", portNo);
	/** 打开指定的串口 */
	m_hComm = CreateFileA(szPort,  /** 设备名,COM1,COM2等 */
		GENERIC_READ | GENERIC_WRITE, /** 访问模式,可同时读写 */
		0,                            /** 共享模式,0表示不共享 */
		NULL,                         /** 安全性设置,一般使用NULL */
		OPEN_EXISTING,                /** 该参数表示设备必须存在,否则创建失败 */
		0,
		0);
	if (m_hComm == INVALID_HANDLE_VALUE)//如果打开失败，释放资源并返回
	{
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}
	LeaveCriticalSection(&m_csCommunicationSync);//退出临界区
	return true;
}

bool CSerialPort::OpenListenThread()
{
	if (m_hListenThread != INVALID_HANDLE_VALUE)//检测线程是否已经开启了
	{
		return false;//线程已经开启
	}
	s_bExit = false;
	UINT threadId;//线程ID
	//开启串口数据监听线程
	m_hListenThread = (HANDLE)_beginthreadex(NULL, 0, ListenThread, this, 0, &threadId);
	if (!m_hListenThread)
	{
		return false;
	}
	//设置线程的优先级,高于普通线程
	if (!SetThreadPriority(m_hListenThread, THREAD_PRIORITY_ABOVE_NORMAL))
	{
		return false;
	}
	return true;
}

bool CSerialPort::CloseListenTread()
{
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		s_bExit = true;//通知线程退出
		Sleep(10);//等待线程退出
		CloseHandle(m_hListenThread);//置线程句柄无效
		m_hListenThread = INVALID_HANDLE_VALUE;
	}
	return true;
}

UINT CSerialPort::GetBytesInCOM()
{
	DWORD dwError = 0; //错误码
	COMSTAT  comstat;//COMSTAT结构体,记录通信设备的状态信息
	memset(&comstat, 0, sizeof(COMSTAT));
	UINT BytesInQue = 0;
	/** 在调用ReadFile和WriteFile之前,通过本函数清除以前遗留的错误标志 */
	if (ClearCommError(m_hComm, &dwError, &comstat))
	{
		BytesInQue = comstat.cbInQue;//获取在输入缓冲区中的字节数
	}
	return BytesInQue;
}

UINT WINAPI CSerialPort::ListenThread(void* pParam)
{
	CSerialPort* pSerialPort = reinterpret_cast<CSerialPort*>(pParam);//得到本类的指针
	while (!pSerialPort->s_bExit)//线程循环,轮询方式读取串口数据
	{
		UINT BytesInQue = pSerialPort->GetBytesInCOM();
		if (BytesInQue == 0)//如果串口输入缓冲区中无数据,则休息一会再查询
		{
			Sleep(SLEEP_TIME_INTERVAL);
			continue;
		}
		/** 读取输入缓冲区中的数据并输出显示 */
		char cRecved = 0x00;
		do
		{
			cRecved = 0x00;
			if (pSerialPort->ReadChar(cRecved) == true)
			{
				std::cout << cRecved;
				continue;
			}
		} while (--BytesInQue);
	}
	return 0;
}

bool CSerialPort::ReadChar(char& cRecved)
{
	BOOL  bResult = TRUE;
	DWORD BytesRead = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	EnterCriticalSection(&m_csCommunicationSync);//临界区保护
	/** 从缓冲区读取一个字节的数据 */
	bResult = ReadFile(m_hComm, &cRecved, 1, &BytesRead, NULL);
	if ((!bResult))
	{
		DWORD dwError = GetLastError();//获取错误码,可以根据该错误码查出错误原因
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);//清空串口缓冲区
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}
	LeaveCriticalSection(&m_csCommunicationSync);//离开临界区
	return (BytesRead == 1);
}

bool CSerialPort::WriteData(/*unsigned*/ char* pData, unsigned int length)
{
	BOOL   bResult = TRUE;
	DWORD  BytesToSend = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	EnterCriticalSection(&m_csCommunicationSync);//临界区保护
	/** 向缓冲区写入指定量的数据 */
	bResult = WriteFile(m_hComm, pData, length, &BytesToSend, NULL);
	if (!bResult)
	{
		DWORD dwError = GetLastError();
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);//清空串口缓冲区
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}
	LeaveCriticalSection(&m_csCommunicationSync);//离开临界区
	return true;
}