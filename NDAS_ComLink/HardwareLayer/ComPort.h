//------------------------------------------------------------------------------

#ifndef ComPortH
#define ComPortH

#include <string>
#include <Windows.h>
#include "SharedData.h"

//------------------------------------------------------------------------------
#pragma pack(4)
class CComPort {
  private:
    bool m_Initialized;
    ulong m_BytesReceived;
    ulong m_BytesSent;
     std::string m_PortName;
    CONNECTION_PARAMETERS m_ConectionParams;
    HANDLE m_Handle;
    DCB *m_DCBParams;
    COMMTIMEOUTS m_Timeouts;
    OVERLAPPED m_Overlapped;
    bool SetupDCB();
    void SetQueueSizes(ushort InQueueSize, ushort OutQueueSize);
    void SetupTimeouts();
  public:
     CComPort(CONNECTION_PARAMETERS CP);
    ~CComPort();
    bool Init();
    void Reset();
    ulong InputCount();
    ulong Read(char *pBuffer, uint BytesToRead = 0, uint Timeout = 0);
    ulong ReadUntilTimeout(char *pBuffer, int AnswerWaitTimeOut);
    ulong Write(char *pBuffer, uint BytesToWrite);
    HANDLE GetHandle() {
	return m_Handle;
    };
    ulong GetBytesSent() {
	return m_BytesSent;
    };
    ulong GetBytesReceived() {
	return m_BytesReceived;
    };
};

//------------------------------------------------------------------------------
//

#endif
