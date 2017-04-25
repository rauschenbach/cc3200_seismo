//------------------------------------------------------------------------------
// Class implements base COM-port functions

#include <stdio.h>
#include "ComPort.h"
#pragma hdrstop
#pragma package(smart_init)

//------------------------------------------------------------------------------
//
CComPort::CComPort(CONNECTION_PARAMETERS CP) : m_ConectionParams(CP), m_Initialized(false)
{
    char buf[128];
    sprintf(buf, "\\\\.\\COM%d", m_ConectionParams.COMPort);

    m_PortName = std::string(buf);
};

//------------------------------------------------------------------------------
//
CComPort::~CComPort()
{
    if (m_Initialized) {
        //Reset();
        CloseHandle(m_Handle);
    }
};

//------------------------------------------------------------------------------
// Инициализация порта.
bool CComPort::Init()
{
	m_Handle = CreateFile(m_PortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (m_Handle != INVALID_HANDLE_VALUE) {
		m_BytesSent = m_BytesReceived = 0;
        Reset();
		if (SetupDCB()) {
			SetupTimeouts();
			SetQueueSizes(1024, 1024);
			m_Initialized = true;
		}
    }
    return m_Initialized;
}

//------------------------------------------------------------------------------
// Сбрасывает порт и очищает все буферы.
void CComPort::Reset()
{
	PurgeComm(m_Handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	ClearCommBreak(m_Handle);
}

//------------------------------------------------------------------------------
// Устанавливает размеры In и Out буферов драйвера.

void CComPort::SetQueueSizes(ushort InQueueSize, ushort OutQueueSize)
{
	SetupComm(m_Handle, InQueueSize, OutQueueSize);
}

//------------------------------------------------------------------------------
// Инициализация DCB.

bool CComPort::SetupDCB()
{
	bool Result = false;
	AnsiString ConnectString;
	m_DCBParams = (DCB*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DCB));
	try {
		ConnectString="baud="+IntToStr(m_ConectionParams.PortSpeed)+" parity=N data=8 stop=2";
		m_DCBParams->DCBlength = sizeof(DCB);
		Result = BuildCommDCB(ConnectString.c_str(), m_DCBParams);
		SetCommState(m_Handle, m_DCBParams);
	}
	__finally {
		HeapFree(GetProcessHeap(), 0, m_DCBParams);
	}
	return Result;
}

//------------------------------------------------------------------------------
// Инициализация COMMTIMEOUTS.

void CComPort::SetupTimeouts()
{
	m_Timeouts.ReadIntervalTimeout = 10;
	m_Timeouts.ReadTotalTimeoutMultiplier = m_Timeouts.ReadTotalTimeoutConstant = 0;
	m_Timeouts.WriteTotalTimeoutMultiplier = m_Timeouts.WriteTotalTimeoutConstant = 0;
	SetCommTimeouts(m_Handle, &m_Timeouts);
}

//------------------------------------------------------------------------------
// Возвращает число байт ожидающих чтения в приемнном буффере драйвера
// В случае ошибки вызова ClearCommError возвращает -1.

ulong CComPort::InputCount()
{
	ulong Errors;
	COMSTAT CS;
	return (ClearCommError(m_Handle, &Errors, &CS)) ? CS.cbInQue : -1;
}

//------------------------------------------------------------------------------
// Читает BytesToRead байт из In буфера драйвера в pBuffer.
// Если BytesToRead == 0 то производится чтения всего приемного буфера.
// Возвращает число реально прочитанных байт.
ulong CComPort::Read(char* pBuffer, uint BytesToRead, uint Timeout)
{
	ulong BytesRead;
        DWORD Mask = 0;
	DWORD WaitStatus;
	bool LastEmptyRead = false;
	char* pIterator = pBuffer;
	uint TotalBytesRead = 0, WaitRetrains = 0;
	if (Timeout == 0)   Timeout = m_ConectionParams.MaxTimeout;
	
	memset(&m_Overlapped,0,sizeof(m_Overlapped));
	m_Overlapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	try {
		SetCommMask(m_Handle, EV_RXCHAR);
		do {
			WaitCommEvent(m_Handle, &Mask, &m_Overlapped);
			if (GetLastError() == ERROR_IO_PENDING) {
				if (WaitForSingleObject(m_Overlapped.hEvent, Timeout) == WAIT_OBJECT_0) {
					int BytesInBuffer = InputCount();
					if (BytesInBuffer > 0) {                                                           /////vvvvv:
						ReadFile(m_Handle, pIterator, ((BytesToRead == 0) ? BytesInBuffer : BytesToRead - TotalBytesRead), &BytesRead, &m_Overlapped);
						if(WaitForSingleObject(m_Overlapped.hEvent,Timeout) == WAIT_OBJECT_0)  {
							GetOverlappedResult(m_Handle,&m_Overlapped,&BytesRead,FALSE);
							TotalBytesRead += BytesRead;
							pIterator += BytesRead;
							LastEmptyRead = false;
						}
					} else BytesRead = 0;
				} else BytesRead = 0;
			} else
				BytesRead = 0;
			if (BytesRead == 0) {
				 if (LastEmptyRead) WaitRetrains++;
				 LastEmptyRead = true;
			}
			BytesRead = 0; Mask = 0;
			ResetEvent(m_Overlapped.hEvent);
			if (WaitRetrains > 3) break;
		} while (TotalBytesRead < BytesToRead);
	} catch (...) {
		TotalBytesRead = 0;
	}
	CloseHandle(m_Overlapped.hEvent);
	m_BytesReceived+=TotalBytesRead;
	return TotalBytesRead;
}

//------------------------------------------------------------------------------
ulong CComPort::ReadUntilTimeout(char* pBuffer, int AnswerWaitTimeOut)
{
    ulong BytesRead, Mask = 0;
    DWORD WaitStatus;
    bool FirstbyteReaded = false;
    bool LastEmptyRead = false;
    char* pIterator = pBuffer;
    uint TotalBytesRead = 0, WaitRetrains = 0;
    memset(&m_Overlapped,0,sizeof(m_Overlapped));
    m_Overlapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
    try {
        SetCommMask(m_Handle, EV_RXCHAR);
        do {
            WaitCommEvent(m_Handle, &Mask, &m_Overlapped);
            if (GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(m_Overlapped.hEvent, (!FirstbyteReaded) ? AnswerWaitTimeOut : m_ConectionParams.MaxTimeout*2) == WAIT_OBJECT_0) {
                    int BytesInBuffer = InputCount();
                    if (BytesInBuffer > 0) {
                        FirstbyteReaded = true;
                        ReadFile(m_Handle, pIterator, BytesInBuffer, &BytesRead, &m_Overlapped);
                        if(WaitForSingleObject(m_Overlapped.hEvent, m_ConectionParams.MaxTimeout) == WAIT_OBJECT_0)  {
                            GetOverlappedResult(m_Handle,&m_Overlapped,&BytesRead,FALSE);
                            TotalBytesRead += BytesRead;
                            pIterator += BytesRead;
                            LastEmptyRead = false;
                        }
                    } else BytesRead = 0;
                } else BytesRead = 0;
            } else
                BytesRead = 0;
            if (BytesRead == 0) {
                 if (LastEmptyRead) WaitRetrains++;
                 LastEmptyRead = true;
            }
            BytesRead = 0; Mask = 0;
            ResetEvent(m_Overlapped.hEvent);
        } while (WaitRetrains < 3);
    } catch (...) {
        TotalBytesRead = 0;
    }
    CloseHandle(m_Overlapped.hEvent);
    return TotalBytesRead;
}

//------------------------------------------------------------------------------
// Записывает BytesToWrite байт из pBuffer в Out буфер драйвера.
// Возвращает число реально записанных байт.
ulong CComPort::Write(char* pBuffer, uint BytesToWrite)
{
   ulong Result = 0, BytesWritten;
    memset(&m_Overlapped, 0, sizeof(m_Overlapped));
    m_Overlapped.hEvent = CreateEvent(NULL, false, false, NULL);
    Reset();
    try {
        WriteFile(m_Handle, pBuffer, BytesToWrite, &BytesWritten, &m_Overlapped);
        if (GetLastError() == ERROR_IO_PENDING) {
            if(WaitForSingleObject(m_Overlapped.hEvent, m_ConectionParams.MaxTimeout) == WAIT_OBJECT_0) {
               GetOverlappedResult(m_Handle, &m_Overlapped, &BytesWritten, false);
               Result = (BytesWritten == BytesToWrite) ? BytesWritten : 0;
            } else Result = 0;
        } else Result = 0;
    } catch (...) {
		Result = 0;
	}
	CloseHandle(m_Overlapped.hEvent);
	m_BytesSent+=Result;
	return Result;
}

//------------------------------------------------------------------------------


