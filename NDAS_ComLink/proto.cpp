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
// Найти перевод строки и убрать его и Перетранслировать
static void LogOutStr(char *txt)
{
    frmMain->LogOut(txt);
}


static void PrintModemCmd(char *txt, bool req = false)
{
    int i;
    char buf[1024];
    String s;

    // Скопируем в буфер
    memset(buf, 0, sizeof(buf));
    memcpy(buf, txt, strlen(txt));

    // Уберем перевод строки
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

// Настраиваем порт
bool PortInit(int num, u32 baud)
{
    AnsiString temp;
    struct CONNECTION_PARAMETERS params;

    temp = "COM" + IntToStr(num);

    //Открываем порт и записываем настройки
    params.COMPort = num;
    params.PortSpeed = baud;
    params.MaxFullRetrains = 5;
    params.MaxTimeout = 50;


    // закроем порт если он есть
    if (CommPort != NULL) {
	delete CommPort;
	CommPort = NULL;
    }

    CommPort = new CComPort(params);
    Sleep(1);
    CommPort->Init();

    return true;
}

// Закрыть порт
bool PortClose(void)
{
    if (CommPort != NULL) {
	delete CommPort;
	CommPort = NULL;
    }

    return true;
}


// Запись в порт
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


// Чтение из порта
static int PortRead(char *buf, int len)
{
    int num;

    // Меньше pfujkjdrf но не короткий статус
    num = CommPort->Read(buf, len);
    if (num < len && num != sizeof(HEADER_t)) {
	CommPort->Reset();
	counts.rx_cmd_err++;
    }
    counts.rx_pack++;
    return num;
}


// Чтение из порта
static int PortReadToTimeout(char *buf, int t)
{
    int num;

    num = CommPort->ReadUntilTimeout(buf, t);
    if (num < 0) {		// Все принятые пакеты
	CommPort->Reset();
	counts.rx_cmd_err++;
	return -1;		// Нет связи
    }
    counts.rx_pack++;
    CommPort->Reset();
    return num;
}

////////////////////////////////////////////////////////////////////////////////
// Видны в других файлах
////////////////////////////////////////////////////////////////////////////////
/* посылка команды - len: длина всех даных!!! */
bool SendSomeCommand(u8 cmd, void *data, int dataSize)
{
    u8 buf[1024];
    HEADER_t hdr;
    int num;
    bool res = false;

    do {
	// неправильная длина посылки
	if (dataSize > 512 || dataSize <= 0) {
	    break;
	}

	hdr.ndas = 0x444E;
	hdr.crc16 = 0;		/* CRC16 посылки ПОСЛЕ HEADER_t если есть */
	hdr.dev_id = DEV_ADDR; // константа;
	hdr.pack_type = PACK_REQUEST;
	hdr.cmd = cmd;		/* Команда */
	hdr.ver = DEV_VERSION;
	hdr.size = dataSize - sizeof(HEADER_t);
	hdr.rsvd0 = 0;


	// В команде есть данные - скопируем их
	if (data != NULL && dataSize >= sizeof(HEADER_t)) {
	    memcpy((u8 *) data, &hdr, sizeof(HEADER_t));
	}
     //	add_crc16(buf);		// Контрольная сумма

	num = PortWrite((u8*)data, dataSize);
	if (num != dataSize) {
	    break;		// Порт не открыт
	}

	res = true;		// Все ОК
    } while (0);

    return res;
}

/**
 * Получить ответ из порта
 */
bool ReadAnyData(void *data, int len)
{
    int num;
    bool res = false;
    

    do {
	if (len == 0 || data == NULL)
	    break;

	// Ждем ответа
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
 * Получить ответ из порта
 */
bool ReadAnyDataToTimeout(void *data, int time)
{
    int num;
    bool res = false;

    do {
	if (time == 0 || data == NULL)
	    break;

	// Ждем ответа
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
///                          Сами команды                                                                               //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------
// Получить статус - new
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


	// Читаем SHORT_STATUS_LEN байт статуса
	if (ReadAnyData(st, sizeof(STATUS_h)) == true) {
	    res = 0;
	}

    } while (0);
    LeaveCriticalSection(&cs);

    return res;
}
//---------------------------------------------------------------------------
// Получить счетчики обменов
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


	// Читаем счетчики
	if (ReadAnyData(cnt, sizeof(UART_COUNTS_t)) == true) {
	    res = 0;
	}

    } while (0);
    LeaveCriticalSection(&cs);

    return res;
}



//---------------------------------------------------------------------------
// Записать параметры перед измерениями
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

	// Читаем статус прамо в эту же структуру
	if (ReadAnyData(par, sizeof(HEADER_t)) == true) {
	    res = 0;
	}

    } while (0);
    LeaveCriticalSection(&cs);
    sprintf(buf, "Записать параметры АПЦ, res = %d: ", res);
    frmMain->PrintLog(buf, (u8 *)par, sizeof(HEADER_t));

    return res;
}
//---------------------------------------------------------------------------
// Начать штатные измерения ускорения
// В критическую секцию - 2 раза, т.к. у нас есть ожидание
// Пусть другие процессы не ждут
int StartDevice(void)
{
    c8 buf[1024];
    int num, res = -1;
    START_PREVIEW_h start;
    HEADER_t hdr;


    EnterCriticalSection(&cs);
    do {
        // Получить время
        start.time_stamp = get_sec_ticks() * TIMER_NS_DIVIDER;
	if (SendSomeCommand(CMD_START_PREVIEW, &start, sizeof(start)) == false) {
	    break;
	}
         Sleep(50);

         memset(&hdr, 0, sizeof(hdr));
	// Читаем статус прамо в эту же структуру
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
// команда Выдай данные измерений или статус 5 байт
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


	// Читаем статус прамо в эту же структуру
	if (ReadAnyDataToTimeout(pack, sizeof(ADS131_DATA_h)) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}
//---------------------------------------------------------------------------
//  Команда: Стоп измерения - new
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

	// Читаем статус прамо в эту же структуру
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
// Выдасть счетчики обмена
void GetMyCounts(UART_COUNTS_t* cnt)
{
    if (cnt) {
	memcpy(cnt, &counts, sizeof(UART_COUNTS_t));
    }
}

//---------------------------------------------------------------------------

