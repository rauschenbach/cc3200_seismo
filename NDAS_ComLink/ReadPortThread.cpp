//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include <stdio.h>
#include <math.h>
#include "proto.h"
#include "main.h"
#include "utils.h"
#include "ReadPortThread.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall ReadPortThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
// иначе при закрытии процесса - ошибка(данных уже нету!)
static UART_COUNTS_t counts;
static ADS131_DATA_h ads131_data, pack;


static STATUS_h status;

#define TIMER_HSEC_DIVIDER	        (500)
#define TIMER_SEC_DIVIDER		(1000)
#define TIMER_MIN_DIVIDER		(600000)

void get_dsp_status(void *p)
{
    memcpy(p, &status, sizeof(DEV_STATUS_STRUCT));
}


__fastcall ReadPortThread::ReadPortThread(bool CreateSuspended)
:TThread(CreateSuspended)
{
}

//---------------------------------------------------------------------------
// Наш таймер в main form считает по 100 мс
void __fastcall ReadPortThread::Execute()
{
    char buf[256];
    s64 timer0, timer1;
    int not_connect = 0;
    int res;

    do {
	if (frmMain->bEndThread) {
	    break;
	}
	bPick = false;

	timer0 = get_msec_ticks() / 500;


	/* Получить статус раз в 1/2 c */
	if (timer0 != timer1) {
	    timer1 = timer0;

	    /* Получить счетчики */
	    if (CountsGet(&counts) == 0) {
		::SendMessage(frmMain->Handle, CM_SHOW_PORT_COUNTS, (WPARAM) &counts, 0);
	    }

	    /* Получить счетчики и статусы */
	    if (StatusGet(&status) == 0) {
		::SendMessage(frmMain->Handle, CM_SHOW_STATUS, (WPARAM) & status, 0);
		not_connect = 0;
	    } else {		/*if (res < 0) */
		if (not_connect > 250) {	// нет 12 секунд статусов
		    frmMain->bEndThread = true;
		    ::PostMessage(frmMain->Handle, CM_NO_DATA, 0, 0);
		    break;
		}
	    }
	}
	// Если есть данные-читаем до получения статуса "нет данных" - подобраьт время!
	// если есть данные - как часто отправляем?
              /*
	if (DataGet(&ads131_data) == 0) {
	    memcpy(&pack, &ads131_data, sizeof(ADS131_DATA_h));
	    ::PostMessage(frmMain->Handle, CM_READ_DATA, (WPARAM) & pack, 0);
	}
                */
	bPick = true;
	Sleep(150);
    }
    while (!frmMain->bEndThread /* && !Terminated */ );
    not_connect = 0;
    frmMain->LogOut("Thread stopped");
}

//---------------------------------------------------------------------------)
