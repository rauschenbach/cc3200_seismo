#include <vcl.h>
#pragma hdrstop
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include "my_cmd.h"
#include <errno.h>
#include <stddef.h>		/* _threadid variable */
#include <process.h>		/* _beginthread, _endthread */
#include <time.h>		/* time, _ctime */
#include "main.h"
#include "utils.h"
#include "about.h"
#include "proto.h"
#include "FindStationsThread.h"
#include "WriteCommandThread.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "light"
#pragma link "LPComponent"
#pragma link "LPDrawLayers"
#pragma link "SLComponentCollection"
#pragma link "SLScope"
#pragma link "BMSpinEdit"
#pragma link "light"
#pragma resource "*.dfm"
static myLangID lang;

extern get_dsp_status(void *);
CRITICAL_SECTION cs; // Будет extern в другом Файле 
TfrmMain *frmMain;
//---------------------------------------------------------------------------
__fastcall TfrmMain::TfrmMain(TComponent * Owner):TForm(Owner), index(0),
enable_measure(false), timer_tick(0), test_run(false), bEndThread(true),
run_ok(false), MyThread(NULL), enable_log(true), big_timer(0), first_run(true), end_aqq(false)
{
    lang = lang_eng;
}

//---------------------------------------------------------------------------
// Послать команду, прочитать ответ
int __fastcall TfrmMain::Querry(String name, u8 * send, int tx, u8 * recv, int rx, int *baud)
{
    int i;
    unsigned long num;
    bool res = false;
    static int first = 0;
    char str[128];
    int rate[] = {115200};//{ 115200, 230400, 460800, 921600 };

    for (i = 0; i < sizeof(rate) / sizeof(rate[0]); i++) {
	PortHandle = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (PortHandle != INVALID_HANDLE_VALUE) {
	    sprintf(str, "baud=%d parity=N data=8 stop=2", rate[i]);
	    BuildCommDCB(str, &dcb);
	    SetCommState(PortHandle, &dcb);
	    m_Timeouts.ReadIntervalTimeout = 50;
	    m_Timeouts.ReadTotalTimeoutMultiplier = m_Timeouts.ReadTotalTimeoutConstant = 25;
	    m_Timeouts.WriteTotalTimeoutMultiplier = m_Timeouts.WriteTotalTimeoutConstant = 5;
	    SetCommTimeouts(PortHandle, &m_Timeouts);
	    WriteFile(PortHandle, send, tx, &num, NULL);

	    Sleep(10);
	    num = 0;
	    res = ReadFile(PortHandle, recv, rx, &num, NULL);
	    CloseHandle(PortHandle);

	    if (res && num) {
		*baud = rate[i];
                //PrintLog("recv: ", recv, rx);
		break;
	    }
	}
    }
    return (int) num;		// сколько прочитали
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnCloseClick(TObject * Sender)
{
    bEndThread = true;
    TerminateAllThreads();
    Close();
}

//---------------------------------------------------------------------------
// Запуск самотестирования
void __fastcall TfrmMain::btnTestClick(TObject * Sender)
{
    WriteCommandThread *thread;
    first_run = false;		// запустили

    LogOut("Begin SelfTest...wait!");

    enable_log = false;
//    thread = new WriteCommandThread(InitTest, NULL, false, false);
    thread->WaitFor();
    delete thread;
}

//---------------------------------------------------------------------------
// Вывеcти в Memo
void __fastcall TfrmMain::PrintLog(String text, u8 * buf, int len)
{
    int i;
    String str;
    char buf1[1024];

    for (i = 0; i < len; i++)
	str += String(IntToHex((u8) buf[i], 2) + " ");
    LogOut((text + str).c_str());
}

//---------------------------------------------------------------------------
// Вывод в лог
void __fastcall TfrmMain::LogOut(char *text, bool log_only)
{
    int i, len;
    char buf[1024];
    String str;
    char crlf[] = "\r\n";
    static String old;
    unsigned long bw;

    str = TimeString() + text;
    // Скопируем в буфер
    memset(buf, 0, sizeof(buf));
    memcpy(buf, str.c_str(), str.Length());

    // Уберем перевод строки
    for (i = 0; i < str.Length(); i++) {
	if (buf[i] == '\x0D' || buf[i] == '\x0A') {
	    buf[i] = 0;
	    break;
	}
    }

    if (!log_only && enable_log) {
	memoLog->Lines->Add(buf);
    }
/*
    if (log_fd != INVALID_HANDLE_VALUE && old != String(text)) {
	old = String(text);
	WriteFile(log_fd, buf, strlen(buf), &bw, NULL);
	WriteFile(log_fd, crlf, strlen(crlf), &bw, NULL);	// Добавлять перевод строки
    }
*/
}

//---------------------------------------------------------------------------
// Вывеcти время компьютера
void __fastcall TfrmMain::TimeUTC(void)
{
    int i;
    char str[128];
    time_t t;

    t = time(NULL);
    sec_to_str(t, str);;

    sbInfo->Panels->Items[2]->Text = String("Time PC:    ") + str;
}

//---------------------------------------------------------------------------
// Вывести время
// Если время различается более чем на 30 секунд - моргать статусом
void __fastcall TfrmMain::TimeGNS110(long t)
{
    char str[128];
    time_t n;
    static int i = 0;

    n = time(NULL);
    sec_to_str(t, str);
    sbInfo->Panels->Items[3]->Text =  String("Time GNS:    ") + str;
}

//---------------------------------------------------------------------------


// При открытии программы создаем LOG - файл
void __fastcall TfrmMain::FormActivate(TObject * Sender)
{
    char str[256];
    time_t t0;
    TIME_DATE td;

    // 2 секции
    InitializeCriticalSection(&time_cs);

    InitializeCriticalSection(&cs);


    this->Caption = "NDAS ComLink";
    Application->Title = "NDAS ComLink";

    t0 = time(NULL);

    param_struct.am3_modem_type = 0;
    param_struct.modem_num = 0;


    menuSave->Enabled = true;
    StartTime = t0;		// Время запуска программы сохраним

    // Открываем log файл
    sec_to_td(t0, &td);
/*
    CreateDirectory("LOGS", NULL);
    sprintf(str, "LOGS\\LOG_%02d%02d%02d_%02d%02d.log\0", td.day, td.month, td.year - 2000, td.hour, td.min);
    log_fd = CreateFile(str, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
*/
    sprintf(str, "Run program NDAS ComLink\r\n");
    LogOut(str);
    tmFind->Enabled = true;	// Таймер поиска станций
}

//---------------------------------------------------------------------------
// Поиск портов
void __fastcall TfrmMain::TerminateAllThreads(void)
{
    TerminateScope();		// Останавливаем устройство и чистим буфер

    // Выключаем поток
    if (MyThread != NULL) {
	MyThread->WaitFor();
	delete MyThread;
	MyThread = NULL;

        ShowStatus(NULL);
        cbPortName->Clear();
        reset_num_station();
    }
    // закроем порт
    PortClose();
    tmFind->Enabled = true;
}


//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormResize(TObject * Sender)
{
    int w, h, d, i;
    char str[128];
    int w2;

    w = pnUp->Width;
    h = this->Height;
    d = 5;

    // 1-я вкладка
    if (gbCounts->Visible) {
	gbParams->Width = w / 6 - d;
	gbTimes->Width = w / 6 - d;
	gbMyCounts->Width = w / 6 - d;
	gbCounts->Width = w / 6 - d;
	gbEnviron->Width = w / 6 * 2 + d;
    } else {
	gbParams->Width = w / 4 - d;
	gbTimes->Width = w / 4 - d;
	gbEnviron->Width = w / 4 * 2 + d;
    }

    // нижняя часть
    if (gbAdc->Visible) {
	gbStatus0->Width = w / 6 - d;
	gbStatus1->Width = w / 6 - d;
	gbStatus2->Width = w / 3 - d;
	gbAdc->Width = w / 6 - d;
	gbResetCause->Width = w / 6 - d;
    } else {
	gbStatus0->Width = w / 5 - d;
	gbStatus1->Width = w / 5 - d;
	gbStatus2->Width = w / 3;

	gbResetCause->Width = w / 5 - d;
    }


    pnChangeMode->Width = w - 10;
}

//---------------------------------------------------------------------------
// Запрос на выход из программы
void __fastcall TfrmMain::FormCloseQuery(TObject * Sender, bool & CanClose)
{
    int res;

    if (!bEndThread) {
	res = YESNO_BOX("Device will stay in interractive mode! Logout from the programm?");


	if (res == IDYES) {
	    bEndThread = true;
	    TerminateAllThreads();
	    CanClose = true;
	} else {
	    CanClose = false;
	}
    } else {
	CanClose = true;
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormDestroy(TObject * Sender)
{

    char str[128];
    sprintf(str, "NDAS ComLink close");
    LogOut(str);


    DeleteCriticalSection(&cs);
    
    DeleteCriticalSection(&time_cs);
}

//---------------------------------------------------------------------------
// Нажатие кнопки Старт
// Что выбрано?
// Запустить выбранную станцию
void __fastcall TfrmMain::sbRunModuleClick(TObject * Sender)
{
    String met;

    if (cbPortName->ItemIndex >= 0) {
	met = cbPortName->Items[cbPortName->ItemIndex].Text;
    }

    gns_num = frmMain->port_set[cbPortName->ItemIndex].gns_name;

    // Если нашел станции
    if (cbPortName->ItemIndex >= 0 && bEndThread) {
	tmFind->Enabled = false;
	cbPortName->Enabled = false;
	sbRunModule->Down = true;
	EnableControl();
	met = "Connect to recorder " + met;
    } else if (!bEndThread) {	// остановить станцию
	bEndThread = true;
	TerminateAllThreads();
	cbPortName->Enabled = true;
	sbRunModule->Down = false;
	met = "Disconnect from recorder " + met;
    }
    LogOut(met.c_str());
}

//---------------------------------------------------------------------------
// Разрешим кнопки
void __fastcall TfrmMain::EnableControl(void)
{
    int num, baud;

    bEndThread = true;
    TerminateAllThreads();

    btnStartThread->Enabled = true;
    btnTest->Enabled = true;

    num = port_set[cbPortName->ItemIndex].port_num;
    baud = port_set[cbPortName->ItemIndex].baud;

    if (PortInit(num, baud)) {
	LogOut("Port init OK");
    }


    bEndThread = false;
    MyThread = new ReadPortThread(false);
    MyThread->Priority = tpHighest;	// самый выше!

    run_ok = true;
}

//---------------------------------------------------------------------------
// Всегда выводить время UTC
void __fastcall TfrmMain::timIdleTimer(TObject * Sender)
{
    // Раз в секунду
    if (big_timer % 10 == 0) {
	TimeUTC();
        }
    GetMyCounts(&counts);
    ShowMyCounts(&counts);
    big_timer++;
}

//---------------------------------------------------------------------------
// Стоп измерения
void __fastcall TfrmMain::btnStopClick(TObject * Sender)
{
    TerminateScope();
}

//---------------------------------------------------------------------------
// Прекратить рисование
// Останавливаем устройство и чистим буфер
void __fastcall TfrmMain::TerminateScope(void)
{
    if (run_ok && enable_measure) {
	WriteCommandThread *thread;
	thread = new WriteCommandThread(StopDevice, NULL, false, false);
	thread->WaitFor();
	delete thread;
	Sleep(50);
////vvvv:       thread = new WriteCommandThread(ClearBuf, NULL, false, false);
//	thread->WaitFor();
//	delete thread;

	slsDisplay->Title->Text = "ADC";
	pnStatus->Visible = true;
    }
}

//---------------------------------------------------------------------------
// очистить вывод-иначе невозможно работать!
void __fastcall TfrmMain::sbInfoDblClick(TObject * Sender)
{
    memoLog->Clear();
}

//---------------------------------------------------------------------------
// Поиск станций
void __fastcall TfrmMain::tmFindTimer(TObject * Sender)
{
    if (get_num_station() > 0) {
	tmFind->Enabled = false;
	sbRunModule->Enabled = true;	// Разблокируем кнопку подклбючения
    } else {
	if (!no_enter()) {	// Запуск потока
	    sbRunModule->Enabled = false;
	    sbRunModule->Down = false;
	    FindStationsThread *thread = new FindStationsThread(false);
	    thread->FreeOnTerminate = true;
	}
    }
}

//---------------------------------------------------------------------------
// Вывести статус!
void __fastcall TfrmMain::ShowStatus(void *v)
{
    char str[128];
    u8 byte;
    u16 word;
    s16 volt;


    STATUS_h *status;

    if (v != NULL) {
	status = (STATUS_h *) v;
//////////////////////////////////////////////////////////////////////////////////////////
//                      углы и температура 0
//////////////////////////////////////////////////////////////////////////////////////////
	float p, r, h, T, P;
	p = status->sens_date.pitch / 10.0;
	r = status->sens_date.roll / 10.0;
	h = status->sens_date.head / 10.0;
	T = status->sens_date.temp / 10.0;
	P = status->sens_date.press;


	pnPitch->Caption = (status->sens_date.pitch == -1)?  String("-") : FloatToStrF(p, ffFixed, 3, 1);
	pnRoll->Caption = (status->sens_date.roll == -1)?  String("-") : FloatToStrF(r, ffFixed, 3, 1);
	pnHead->Caption = (status->sens_date.head == -1)?  String("-") : FloatToStrF(h, ffFixed, 3, 1);
	pnTemp0->Caption = (status->sens_date.temp == -1)?  String("-") : FloatToStrF(T, ffFixed, 3, 1);
	pnPressure->Caption = (status->sens_date.press == -1)?  String("-") : FloatToStrF(P, ffFixed, 6, 1);
        pnSupplyV->Caption = (status->sens_date.voltage == -1)?  String("-") : FloatToStrF(status->sens_date.voltage / 1000.0, ffFixed, 2, 2);

        // время
//  	TimeGNS110(status->current_DSP_time / 1000000000UL);	/* Время RTC прибора */

//        ShowPortCounts(&status->uart_counts);

	if (status->init_state.no_time) {
	    lbStatus0_0->Font->Color = clRed;
	} else {
	    lbStatus0_0->Font->Color = clGray;
	}

	if (status->init_state.no_const) {
	    lbStatus0_1->Font->Color = clRed;
	} else {
	    lbStatus0_1->Font->Color = clGray;
	}
        //  ошибка в команде
	if (status->runtime_state.cmd_error) {
	    lbStatus0_2->Font->Color = clRed;
	} else {
	    lbStatus0_2->Font->Color = clGray;
	}
        // Не найден файл регистрации
	if (status->init_state.no_reg_file) {
	    lbStatus0_3->Font->Color = clRed;
	} else {
	    lbStatus0_3->Font->Color = clGray;
	}

        // неисправен регистратор
	if (status->init_state.reg_fault) {
	    lbStatus0_4->Font->Color = clRed;
	} else {
	    lbStatus0_4->Font->Color = clGray;
	}

        // переполнение памяти
	if (status->runtime_state.mem_ovr) {
	    lbStatus0_5->Font->Color = clBlue;
	} else {
	    lbStatus0_5->Font->Color = clGray;
	}

	// GNS включен во время измерений
	if (status->runtime_state.acqis_running) {
	    lbStatus0_6->Font->Color = clGreen;
	} else {
	    lbStatus0_6->Font->Color = clGray;
	}

        /* Самотестирование  */
       	if (status->init_state.self_test_on) {
	    lbStatus0_7->Font->Color = clBlue;
	} else {
	    lbStatus0_7->Font->Color = clGray;
	}



        long long sec = status->current_GPS_data.time / 1000000000;
        sec_to_str(sec, str);
        lbGpsTime->Caption = str;

        grad_to_str(status->current_GPS_data.lat, str);
        lbGpsLat->Caption = str;

        grad_to_str(status->current_GPS_data.lon, str);
        lbGpsLon->Caption = str;

        lbGpsHi->Caption = IntToStr(status->current_GPS_data.hi);
        lbSV->Caption = IntToStr(status->current_GPS_data.numSV);
        pnDrift->Caption = IntToStr(status->current_GPS_data.tAcc);

	// Самотестирование

        // RTC
        if(status->test_state.rtc_ok) {
        	lbStatus1_0->Font->Color = clGreen;
        } else {
        	lbStatus1_0->Font->Color = clRed;
        }

        // Давление
        if(status->test_state.press_ok) {
        	lbStatus1_1->Font->Color = clGreen;
        } else {
        	lbStatus1_1->Font->Color = clRed;
        }

        // ускорение
        if(status->test_state.acc_ok) {
        	lbStatus1_2->Font->Color = clGreen;
        } else {
        	lbStatus1_2->Font->Color = clRed;
        }

        // магнетометр
        if(status->test_state.comp_ok) {
        	lbStatus1_3->Font->Color = clGreen;
        } else {
        	lbStatus1_3->Font->Color = clRed;
        }

        // Модуль влажности
        if(status->test_state.hum_ok) {
        	lbStatus1_4->Font->Color = clGreen;
        } else {
        	lbStatus1_4->Font->Color = clRed;
        }

        // EEPROM
        if(status->test_state.eeprom_ok) {
        	lbStatus1_5->Font->Color = clGreen;
        } else {
        	lbStatus1_5->Font->Color = clRed;
        }

        // SDCard
        if(status->test_state.sd_ok) {
        	lbStatus1_6->Font->Color = clGreen;
        } else {
        	lbStatus1_6->Font->Color = clRed;
        }

        // flash
        if(status->test_state.gps_ok) {
        	lbStatus1_7->Font->Color = clGreen;
        } else {
        	lbStatus1_7->Font->Color = clRed;
        }
///vvvv:
//    sbInfo->Panels->Items[0]->Text = String("Test: ") + IntToHex(status->current_GPS_data.time, 8);
    sbInfo->Panels->Items[0]->Text = String("Test:  ") + IntToStr((s64)status->current_GPS_data.time) + String("    ");
    
    } else {			// Обнулим все

	pnSupplyV->Color = clBtnFace;
	pnSupplyV->Font->Color = clWindowText;
	pnPitch->Color = clBtnFace;
	pnPitch->Font->Color = clWindowText;
	pnRoll->Color = clBtnFace;
	pnRoll->Font->Color = clWindowText;

	lbStatus0_0->Font->Color = clGray;
	lbStatus0_1->Font->Color = clGray;
	lbStatus0_2->Font->Color = clGray;
	lbStatus0_3->Font->Color = clGray;
	lbStatus0_4->Font->Color = clGray;
	lbStatus0_5->Font->Color = clGray;
	lbStatus0_6->Font->Color = clGray;
	lbStatus0_7->Font->Color = clGray;

	// Самотестирование
	lbStatus1_0->Font->Color = clGray;
	lbStatus1_1->Font->Color = clGray;
	lbStatus1_2->Font->Color = clGray;
	lbStatus1_3->Font->Color = clGray;
	lbStatus1_4->Font->Color = clGray;
	lbStatus1_5->Font->Color = clGray;
	lbStatus1_6->Font->Color = clGray;
	lbStatus1_7->Font->Color = clGray;

	// Прочие устройства
	lbStatus2_0->Font->Color = clGray;
//      lbStatus2_1->Font->Color = clGray;
	lbStatus2_2->Font->Color = clGray;
	lbStatus2_3->Font->Color = clGray;
	lbStatus2_4->Font->Color = clGray;
	lbStatus2_5->Font->Color = clGray;
	lbStatus2_6->Font->Color = clGray;
	lbStatus2_7->Font->Color = clGray;


	lbRele0->Font->Color = clGray;
//      lbRele1->Font->Color = clGray;
	lbRele2->Font->Color = clGray;
	lbRele3->Font->Color = clGray;
	lbRele4->Font->Color = clGray;
	lbRele5->Font->Color = clGray;
	lbRele6->Font->Color = clGray;
	lbRele7->Font->Color = clGray;


	// Причина сброса
	lbStatusReset_0->Font->Color = clGray;
	lbStatusReset_1->Font->Color = clGray;
	lbStatusReset_2->Font->Color = clGray;
	lbStatusReset_3->Font->Color = clGray;
	lbStatusReset_4->Font->Color = clGray;
	lbStatusReset_5->Font->Color = clGray;

	// среда
	pnSupplyV->Caption = "";
	pnSupplyV->Color = clBtnFace;
	pnSupplyV->Font->Color = clWindowText;


	pnTemp0->Caption = "";
	pnTemp0->Color = clBtnFace;
	pnTemp0->Font->Color = clWindowText;

	pnPressure->Caption = "";
	pnPressure->Color = clBtnFace;
	pnPressure->Font->Color = clWindowText;

        pnSupplyV->Caption = "";
	pnSupplyV->Color = clBtnFace;
	pnSupplyV->Font->Color = clWindowText;

	pnPitch->Caption = "";
	pnPitch->Color = clBtnFace;
	pnPitch->Font->Color = clWindowText;



	pnRoll->Caption = "";
	pnRoll->Color = clBtnFace;
	pnRoll->Font->Color = clWindowText;


	pnHead->Caption = "";
	pnHead->Color = clBtnFace;
	pnHead->Font->Color = clWindowText;
    }
}
//---------------------------------------------------------------------------
// Счетчики обмена свои
void __fastcall TfrmMain::ShowMyCounts(void* v)
{
    UART_COUNTS_t *counts;

    if (v != NULL) {
      	 counts = (UART_COUNTS_t *) v;
         pnRxPackMy->Caption = counts->rx_pack;
         pnTxPackMy->Caption = counts->tx_pack;
         pnRxCmdErrMy->Caption = counts->rx_cmd_err;
         pnRxStatErrMy->Caption = counts->rx_stat_err;
         pnRxCrcErrMy->Caption = counts->rx_crc_err;
     }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::ShowWorkTime(void *v)
{
}
////////////////////////////////////////////////////////////////////////////////
//           Команды, отвечающие на сообщения SendMessage,PostMessage
////////////////////////////////////////////////////////////////////////////////
// Вывести счетчики обмена свои и gns110
// Качество связи индикатором
void __fastcall TfrmMain::CMShowPortCounts(TMessage & Message)
{
	ShowPortCounts((void *) Message.WParam);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::CMShowParamData(TMessage & Message)
{

}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::CMShowWorkTime(TMessage & Message)
{

}

//---------------------------------------------------------------------------
// Принимаем сообщение и отрисовываем статус
void __fastcall TfrmMain::CMShowStatus(TMessage & Message)
{
    ShowStatus((void *) Message.WParam);
}

//---------------------------------------------------------------------------
// Счетчики обмена
void __fastcall TfrmMain::ShowPortCounts(void* v)
{
    UART_COUNTS_t *counts;

    if (v != NULL) {
 	 counts = (UART_COUNTS_t *) v;
         pnRxPack->Caption = counts->rx_pack;
         pnTxPack->Caption = counts->tx_pack;
         pnRxCmdErr->Caption = counts->rx_cmd_err;
         pnRxStatErr->Caption = counts->rx_stat_err;
         pnRxCrcErr->Caption = counts->rx_crc_err;
	}
}


//---------------------------------------------------------------------------
// Нет данных - сообщение от читающего потока
void __fastcall TfrmMain::CMNoData(TMessage & Message)
{
    // Ждем, что порт выключен
    bEndThread = true;
    TerminateAllThreads();
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::Splitter1Moved(TObject * Sender)
{
    frmMain->Resize();
}

//---------------------------------------------------------------------------
// Выводим, в зависимости от частоты!
void __fastcall TfrmMain::CMReadData(TMessage & Message)
{
    ADS131_DATA_h *pack;
    u8 adc;
    int sec, msec;
    u16 sps;
    int i;
    double freq_s = 125.0; // всегда 125

    pack = (ADS131_DATA_h *) Message.WParam;

    sps = ((1 << pack->adc_freq) ) * 125;


    slsDisplay->Title->Text = "ADC " + IntToStr((int) sps) + " Hz";
    ltAdc0->State = true;
    ltAdc1->State = true;
    ltAdc2->State = true;
    ltAdc3->State = true;

    sec = pack->t0 / 1000000000UL;
    msec = (pack->t0 / 1000000) % 1000;

      for (i = 0; i < NUM_ADS1282_PACK; i++) {
//        t[i] = longTime + i * 0.004 / j;      // 250

	t[i] = sec % 86400 + msec / 1000. + i / (double) freq_s;

	x[i] = (int) ((pack->sig[i].x) << 8) / 256;
	y[i] = (int) ((pack->sig[i].y) << 8) / 256;
	z[i] = (int) ((pack->sig[i].z) << 8) / 256;
	h[i] = (int) ((pack->sig[i].h) << 8) / 256;
    }

    slsDisplay->Channels->Items[0]->Data->AddXYData(t, x, NUM_ADS1282_PACK);
    slsDisplay->Channels->Items[1]->Data->AddXYData(t, y, NUM_ADS1282_PACK);
    slsDisplay->Channels->Items[2]->Data->AddXYData(t, z, NUM_ADS1282_PACK);
    slsDisplay->Channels->Items[3]->Data->AddXYData(t, h, NUM_ADS1282_PACK);
}

//---------------------------------------------------------------------------
// Число точек на экране осцилографа
void __fastcall TfrmMain::cbTimeLimitChange(TObject * Sender)
{
    int points;
    int net;

    // число точек на экране
    points = cbTimeLimit->ItemIndex == 0 ? 1000 : cbTimeLimit->ItemIndex == 1 ?
	2000 : cbTimeLimit->ItemIndex == 2 ? 5000 : cbTimeLimit->ItemIndex == 3 ? 10000 : 20000;
    slsDisplay->SizeLimit = points;
}

//---------------------------------------------------------------------------
// Теперь приявязать к времени диcкретизации!
// первести секунды в часы минуты секунды
void __fastcall TfrmMain::SLScope1XAxisCustomLabel(TObject * Sender, Real SampleValue, AnsiString & AxisLabel)
{
    char str[32];
    u32 h = 0, m = 0, s = 0;
    TIME_DATE td;
    int i;

    i = SampleValue;
    h = (i / 3600) % 24;
    m = (i / 60) % 60;
    s = i % 60;
    sprintf(str, "%02d:%02d:%02d", h, m, s);

    AxisLabel = str;
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::menuAboutClick(TObject * Sender)
{
    frmAbout->ShowModal();
}

//---------------------------------------------------------------------------

void __fastcall TfrmMain::btnGpsOnClick(TObject * Sender)
{
    WriteCommandThread *thread;
    thread = new WriteCommandThread(StopDevice, NULL, false, false);
    thread->WaitFor();
    delete thread;

}

//---------------------------------------------------------------------------
// Записать параметры и запустить старт
void __fastcall TfrmMain::btnStartThreadClick(TObject *Sender)
{
    WriteCommandThread *thread;
    char buf[128];

    thread = new WriteCommandThread(StartDevice, NULL, false, false);
    thread->WaitFor();
    delete thread;

    cbTimeLimitChange(this);
}
//---------------------------------------------------------------------------
// Записать параметры
void __fastcall TfrmMain::btnWriteParamsClick(TObject *Sender)
{
    memset(&adcp, 0, sizeof(adcp));

    adcp.sample_rate = cbAdcFreq->ItemIndex;	/* частота  */
    adcp.test_signal = cbMux->ItemIndex;  /* Тестовый сигнал */


    adcp.adc_board_1.board_active = 1;
    adcp.adc_board_1.ch_1_en = 1;
    adcp.adc_board_1.ch_1_gain = cbPGA->ItemIndex + 1;	/* PGA */
    adcp.adc_board_1.ch_1_range = 1;

    adcp.adc_board_1.ch_2_en = 1;
    adcp.adc_board_1.ch_2_gain = cbPGA->ItemIndex + 1;	/* PGA */
    adcp.adc_board_1.ch_2_range = 1;

    adcp.adc_board_1.ch_3_en = 1;
    adcp.adc_board_1.ch_3_gain = cbPGA->ItemIndex + 1;	/* PGA */
    adcp.adc_board_1.ch_3_range = 1;

    adcp.adc_board_1.ch_4_en = 1;
    adcp.adc_board_1.ch_4_gain = cbPGA->ItemIndex + 1;	/* PGA */
    adcp.adc_board_1.ch_4_range = 1;

    WriteCommandThread *thread;
    thread = new WriteCommandThread(ApplyParams, &adcp, false, false);
    thread->WaitFor();
    delete thread;
}
//---------------------------------------------------------------------------


