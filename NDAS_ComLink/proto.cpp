#include <stdio.h>
#include "proto.h"
#include "my_cmd.h"
#include "utils.h"
#include "my_defs.h"
#include "ComPort.h"
#include "main.h"


#define SLEEP_MODEM_RX           250
#define SLEEP_MODEM_TIMEOUT      500

static UART_COUNTS_t counts;
static CComPort *CommPort = NULL;
extern CRITICAL_SECTION cs;


////////////////////////////////////////////////////////////////////////////////
// ����� ������� ������ � ������ ��� � �����������������
static void LogOutStr(char *txt)
{
    frmMain->LogOut(txt);
}


static void PrintModemCmd(char *txt, bool req = false)
{
    int i;
    char buf[1024];
    String s;

    // ��������� � �����
    memset(buf, 0, sizeof(buf));
    memcpy(buf, txt, strlen(txt));

    // ������ ������� ������
    for (i = 0; i < strlen(txt); i++) {
	if (buf[i] == '\x0D' || buf[i] == '\x0A') {
	    buf[i] = 0;
	    break;
	}
    }
    if (!req)
	s = "Send modem command: " + String(buf);
    else
	s = "Read modem answer: " + String(buf);
    LogOutStr(s.c_str());
}




static void PrintCommandResult(char *cmd, int res)
{
    char buf[256];

    sprintf(buf, "%s %s", res == 0 ? "Tx OK:" : "Tx Err:", cmd);
    LogOutStr(buf);
}

// ����������� ����
bool PortInit(int num, u32 baud)
{
    AnsiString temp;
    struct CONNECTION_PARAMETERS params;

    temp = "COM" + IntToStr(num);

    //��������� ���� � ���������� ���������
    params.COMPort = num;
    params.PortSpeed = baud;
    params.MaxFullRetrains = 5;
    params.MaxTimeout = 50;


    // ������� ���� ���� �� ����
    if (CommPort != NULL) {
	delete CommPort;
	CommPort = NULL;
    }

    CommPort = new CComPort(params);
    Sleep(1);
    CommPort->Init();

    return true;
}

// ������� ����
bool PortClose(void)
{
    if (CommPort != NULL) {
	delete CommPort;
	CommPort = NULL;
    }

    return true;
}


// ������ � ����
static int PortWrite(char *buf, int len)
{
    int num;
    if (CommPort != NULL) {
	num = CommPort->Write(buf, len);
	counts.tx_pack++;
    } else {
	return -1;
    }
    return num;
}


// ������ �� �����
static int PortRead(char *buf, int len)
{
    int num;

    // ������ pfujkjdrf �� �� �������� ������
    num = CommPort->Read(buf, len);
    if (num < len && num != sizeof(HEADER_t)) {
	CommPort->Reset();
	counts.rx_cmd_err++;
    }
    counts.rx_pack++;
    return num;
}


// ������ �� �����
static int PortReadToTimeout(char *buf, int t)
{
    int num;

    num = CommPort->ReadUntilTimeout(buf, t);
    if (num < 0) {		// ��� �������� ������
	CommPort->Reset();
	counts.rx_cmd_err++;
	return -1;		// ��� �����
    }
    counts.rx_pack++;
    CommPort->Reset();
    return num;
}

////////////////////////////////////////////////////////////////////////////////
// ����� � ������ ������
////////////////////////////////////////////////////////////////////////////////
/* ������� ������� - len: ����� ���� �����!!! */
bool SendSomeCommand(u8 cmd, void *data, int dataSize)
{
    u8 buf[1024];
    HEADER_t hdr;
    int num;
    bool res = false;

    do {
	// ������������ ����� �������
	if (dataSize > 512 || dataSize <= 0) {
	    break;
	}

	hdr.ndas = 0x444E;
	hdr.crc16 = 0;		/* CRC16 ������� ����� HEADER_t ���� ���� */
	hdr.dev_id = DEV_ADDR; // ���������;
	hdr.pack_type = PACK_REQUEST;
	hdr.cmd = cmd;		/* ������� */
	hdr.ver = DEV_VERSION;
	hdr.size = dataSize - sizeof(HEADER_t);
	hdr.rsvd0 = 0;


	// � ������� ���� ������ - ��������� ��
	if (data != NULL && dataSize >= sizeof(HEADER_t)) {
	    memcpy((u8 *) data, &hdr, sizeof(HEADER_t));
	}
     //	add_crc16(buf);		// ����������� �����

	num = PortWrite((u8*)data, dataSize);
	if (num != dataSize) {
	    break;		// ���� �� ������
	}

	res = true;		// ��� ��
    } while (0);

    return res;
}

/**
 * �������� ����� �� �����
 */
bool ReadAnyData(void *data, int len)
{
    int num;
    bool res = false;
    

    do {
	if (len == 0 || data == NULL)
	    break;

	// ���� ������
	num = PortRead((u8 *) data, len);
	if (num < len && num != sizeof(HEADER_t)) {
	    break;
	}


        if (test_crc16((c8 *) data)) {
	    counts.rx_crc_err++;
	    break;
	} else {
	    res = true;
	}
    } while (0);


    return res;
}
//---------------------------------------------------------------------------
/**
 * �������� ����� �� �����
 */
bool ReadAnyDataToTimeout(void *data, int time)
{
    int num;
    bool res = false;

    do {
	if (time == 0 || data == NULL)
	    break;

	// ���� ������
	num = PortReadToTimeout((u8 *) data, time);
	if (num <= 0) {
	    break;
	}

	if (test_crc16((c8 *) data)) {
	    counts.rx_crc_err++;
	    break;
	} else {
	    res = true;
	}
    } while (0);

    return res;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///                          ���� �������                                                                               //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------
// �������� ������ - new
int StatusGet(STATUS_h* st)
{
    int res = -1;
    char str[256];
    HEADER_t hdr;

    EnterCriticalSection(&cs);
    do {
	if (SendSomeCommand(CMD_QUERY_STAT, &hdr, sizeof(HEADER_t)) == false) {
	    break;
	}


	// ������ SHORT_STATUS_LEN ���� �������
	if (ReadAnyData(st, sizeof(STATUS_h)) == true) {
	    res = 0;
	}

    } while (0);
    LeaveCriticalSection(&cs);

    return res;
}
//---------------------------------------------------------------------------
// �������� �������� �������
int CountsGet(UART_COUNTS_t* cnt)
{
    int res = -1;
    char str[256];
    HEADER_t hdr;

    EnterCriticalSection(&cs);
    do {
	if (SendSomeCommand(CMD_GET_COUNT, &hdr, sizeof(HEADER_t)) == false) {
	    break;
	}


	// ������ ��������
	if (ReadAnyData(cnt, sizeof(UART_COUNTS_t)) == true) {
	    res = 0;
	}

    } while (0);
    LeaveCriticalSection(&cs);

    return res;
}



//---------------------------------------------------------------------------
// �������� ��������� ����� �����������
int  ApplyParams(ADCP_h* par)
{
    int res = -1;
    char buf[255];

    EnterCriticalSection(&cs);
    do {
	if (SendSomeCommand(CMD_WRITE_PARAMETERS, par, sizeof(ADCP_h)) == false) {
	    break;
	}
        frmMain->PrintLog("Send: Apply Params:", (u8 *)par, sizeof(ADCP_h));

	// ������ ������ ����� � ��� �� ���������
	if (ReadAnyData(par, sizeof(HEADER_t)) == true) {
	    res = 0;
	}

    } while (0);
    LeaveCriticalSection(&cs);
    sprintf(buf, "�������� ��������� ���, res = %d: ", res);
    frmMain->PrintLog(buf, (u8 *)par, sizeof(HEADER_t));

    return res;
}
//---------------------------------------------------------------------------
// ������ ������� ��������� ���������
// � ����������� ������ - 2 ����, �.�. � ��� ���� ��������
// ����� ������ �������� �� ����
int StartDevice(void)
{
    c8 buf[1024];
    int num, res = -1;
    START_PREVIEW_h start;
    HEADER_t hdr;


    EnterCriticalSection(&cs);
    do {
        // �������� �����
        start.time_stamp = get_sec_ticks() * TIMER_NS_DIVIDER;
	if (SendSomeCommand(CMD_START_PREVIEW, &start, sizeof(start)) == false) {
	    break;
	}
         Sleep(50);

         memset(&hdr, 0, sizeof(hdr));
	// ������ ������ ����� � ��� �� ���������
	if (ReadAnyData(&hdr, sizeof(HEADER_t)) == true) {
                if(hdr.pack_type == PACK_ACK) {
	            res = 0;
                   }
	}
    } while (0);
    LeaveCriticalSection(&cs);
    sprintf(buf, "Start, res = %d: ", res);
    frmMain->PrintLog(buf, (u8 *)&hdr, sizeof(HEADER_t));

    return res;
}
//---------------------------------------------------------------------------
// ������� ����� ������ ��������� ��� ������ 5 ����
int DataGet(void *p)
{
    int res = -1;
    int len;
    HEADER_t hdr;
    ADS131_DATA_h *pack;

    if (p == NULL)
	return -1;

    pack = (ADS131_DATA_h *) p;

    EnterCriticalSection(&cs);
    do {
	if (SendSomeCommand(CMD_QUERY_PREVIEW, &hdr, sizeof(HEADER_t)) == false) {
	    break;
	}


	// ������ ������ ����� � ��� �� ���������
	if (ReadAnyDataToTimeout(pack, sizeof(ADS131_DATA_h)) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}
//---------------------------------------------------------------------------
//  �������: ���� ��������� - new
int StopDevice(void *par)
{
    c8 buf[1024];
    int num, res = -1;
    HEADER_t hdr;

    EnterCriticalSection(&cs);
    do {
	if (SendSomeCommand(CMD_STOP_ACQUIRING, &hdr, sizeof(HEADER_t)) == false) {
	    break;
	}
//        frmMain->PrintLog("Send \"Stop\"", (u8 *)&hdr, sizeof(ADCP_h));
         memset(&hdr, 0, sizeof(hdr));
         Sleep( 50);

	// ������ ������ ����� � ��� �� ���������
	if (ReadAnyData(&hdr, sizeof(HEADER_t)) == true && hdr.pack_type == PACK_ACK) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    sprintf(buf, "Stop, res = %d: ", res);
    frmMain->PrintLog(buf, (u8 *)&hdr, sizeof(HEADER_t));

    return res;
}

//---------------------------------------------------------------------------
// ������� �������� ������
void GetMyCounts(UART_COUNTS_t* cnt)
{
    if (cnt) {
	memcpy(cnt, &counts, sizeof(UART_COUNTS_t));
    }
}

//---------------------------------------------------------------------------

