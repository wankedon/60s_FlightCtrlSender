#include "CSerialPort.h"
//  
/// COPYRIGHT NOTICE  
/// Copyright (c) 2009, ���пƼ���ѧtickTick Group  ����Ȩ������  
/// All rights reserved. 
///   
/// @file    SerialPort.cpp    
/// @brief   ����ͨ�����ʵ���ļ�  
///  
/// ���ļ�Ϊ����ͨ�����ʵ�ִ���  
///  
/// @version 1.0    
/// @author  ¬��   
/// @E-mail��lujun.hust@gmail.com  
/// @date    2010/03/19
///   
///  
///  �޶�˵����  

#include "CSerialPort.h"  
#include <process.h>  
#include <iostream>  

bool CSerialPort::s_bExit = false; //�߳��˳���־
const UINT SLEEP_TIME_INTERVAL = 5;//������������ʱ,sleep���´β�ѯ�����ʱ��,��λ:�� 

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
	char szDCBparam[50];// ��ʱ����,���ƶ�����ת��Ϊ�ַ�����ʽ,�Թ���DCB�ṹ
	sprintf_s(szDCBparam, "baud=%d parity=%c data=%d stop=%d", baud, parity, databits, stopsbits);
	if (!openPort(portNo))//��ָ������,�ú����ڲ��Ѿ����ٽ�������,�����벻Ҫ�ӱ���
	{
		return false;
	}
	EnterCriticalSection(&m_csCommunicationSync);//�����ٽ��
	BOOL bIsSuccess = TRUE;//�Ƿ��д�����
	/** �ڴ˿���������������Ļ�������С,���������,��ϵͳ������Ĭ��ֵ.
	*  �Լ����û�������Сʱ,Ҫע�������Դ�һЩ,���⻺�������
	*/
	/*if (bIsSuccess )
	{
	bIsSuccess = SetupComm(m_hComm,10,10);
	}*/
	/** ���ô��ڵĳ�ʱʱ��,����Ϊ0,��ʾ��ʹ�ó�ʱ���� */
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
		// ��ANSI�ַ���ת��ΪUNICODE�ַ���  
		DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, NULL, 0);
		wchar_t* pwText = new wchar_t[dwNum];
		if (!MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, pwText, dwNum))
		{
			bIsSuccess = TRUE;
		}
		/** ��ȡ��ǰ�������ò���,���ҹ��촮��DCB���� */
		bIsSuccess = GetCommState(m_hComm, &dcb) && BuildCommDCB(pwText, &dcb);
		dcb.fRtsControl = RTS_CONTROL_ENABLE;//����RTS flow����
		delete[] pwText;//�ͷ��ڴ�ռ�
	}
	if (bIsSuccess)
	{
		bIsSuccess = SetCommState(m_hComm, &dcb);//ʹ��DCB�������ô���״̬
	}
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);//��մ��ڻ�����
	LeaveCriticalSection(&m_csCommunicationSync);//�뿪�ٽ��
	return bIsSuccess == TRUE;
}

bool CSerialPort::InitPort(UINT portNo, const LPDCB& plDCB)
{
	if (!openPort(portNo))//��ָ������,�ú����ڲ��Ѿ����ٽ�������,�����벻Ҫ�ӱ���
	{
		return false;
	}
	EnterCriticalSection(&m_csCommunicationSync);//�����ٽ��
	if (!SetCommState(m_hComm, plDCB))//���ô��ڲ���
	{
		return false;
	}
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);//��մ��ڻ�����
	LeaveCriticalSection(&m_csCommunicationSync);//�뿪�ٽ��
	return true;
}

void CSerialPort::ClosePort()
{
	//����д��ڱ��򿪣��ر���
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hComm);
		m_hComm = INVALID_HANDLE_VALUE;
	}
}

bool CSerialPort::openPort(UINT portNo)
{
	EnterCriticalSection(&m_csCommunicationSync);//�����ٽ��
	char szPort[50];//�Ѵ��ڵı��ת��Ϊ�豸��
	sprintf_s(szPort, "COM%d", portNo);
	/** ��ָ���Ĵ��� */
	m_hComm = CreateFileA(szPort,  /** �豸��,COM1,COM2�� */
		GENERIC_READ | GENERIC_WRITE, /** ����ģʽ,��ͬʱ��д */
		0,                            /** ����ģʽ,0��ʾ������ */
		NULL,                         /** ��ȫ������,һ��ʹ��NULL */
		OPEN_EXISTING,                /** �ò�����ʾ�豸�������,���򴴽�ʧ�� */
		0,
		0);
	if (m_hComm == INVALID_HANDLE_VALUE)//�����ʧ�ܣ��ͷ���Դ������
	{
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}
	LeaveCriticalSection(&m_csCommunicationSync);//�˳��ٽ���
	return true;
}

bool CSerialPort::OpenListenThread()
{
	if (m_hListenThread != INVALID_HANDLE_VALUE)//����߳��Ƿ��Ѿ�������
	{
		return false;//�߳��Ѿ�����
	}
	s_bExit = false;
	UINT threadId;//�߳�ID
	//�����������ݼ����߳�
	m_hListenThread = (HANDLE)_beginthreadex(NULL, 0, ListenThread, this, 0, &threadId);
	if (!m_hListenThread)
	{
		return false;
	}
	//�����̵߳����ȼ�,������ͨ�߳�
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
		s_bExit = true;//֪ͨ�߳��˳�
		Sleep(10);//�ȴ��߳��˳�
		CloseHandle(m_hListenThread);//���߳̾����Ч
		m_hListenThread = INVALID_HANDLE_VALUE;
	}
	return true;
}

UINT CSerialPort::GetBytesInCOM()
{
	DWORD dwError = 0; //������
	COMSTAT  comstat;//COMSTAT�ṹ��,��¼ͨ���豸��״̬��Ϣ
	memset(&comstat, 0, sizeof(COMSTAT));
	UINT BytesInQue = 0;
	/** �ڵ���ReadFile��WriteFile֮ǰ,ͨ�������������ǰ�����Ĵ����־ */
	if (ClearCommError(m_hComm, &dwError, &comstat))
	{
		BytesInQue = comstat.cbInQue;//��ȡ�����뻺�����е��ֽ���
	}
	return BytesInQue;
}

UINT WINAPI CSerialPort::ListenThread(void* pParam)
{
	CSerialPort* pSerialPort = reinterpret_cast<CSerialPort*>(pParam);//�õ������ָ��
	while (!pSerialPort->s_bExit)//�߳�ѭ��,��ѯ��ʽ��ȡ��������
	{
		UINT BytesInQue = pSerialPort->GetBytesInCOM();
		if (BytesInQue == 0)//����������뻺������������,����Ϣһ���ٲ�ѯ
		{
			Sleep(SLEEP_TIME_INTERVAL);
			continue;
		}
		/** ��ȡ���뻺�����е����ݲ������ʾ */
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
	EnterCriticalSection(&m_csCommunicationSync);//�ٽ�������
	/** �ӻ�������ȡһ���ֽڵ����� */
	bResult = ReadFile(m_hComm, &cRecved, 1, &BytesRead, NULL);
	if ((!bResult))
	{
		DWORD dwError = GetLastError();//��ȡ������,���Ը��ݸô�����������ԭ��
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);//��մ��ڻ�����
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}
	LeaveCriticalSection(&m_csCommunicationSync);//�뿪�ٽ���
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
	EnterCriticalSection(&m_csCommunicationSync);//�ٽ�������
	/** �򻺳���д��ָ���������� */
	bResult = WriteFile(m_hComm, pData, length, &BytesToSend, NULL);
	if (!bResult)
	{
		DWORD dwError = GetLastError();
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);//��մ��ڻ�����
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}
	LeaveCriticalSection(&m_csCommunicationSync);//�뿪�ٽ���
	return true;
}