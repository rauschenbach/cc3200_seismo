//---------------------------------------------------------------------------
// Для поиска станций на свободных COM портах
#include <vcl.h>
#pragma hdrstop
#include <stdio.h>
#include "main.h"
#include "about.h"
#include "my_defs.h"
#include "utils.h"
#include "proto.h"
#include "FindStationsThread.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall FindStationsThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
static int stations = 0;
static bool enter = false;

__fastcall FindStationsThread::FindStationsThread(bool CreateSuspended):
TThread(CreateSuspended)
{
}

//---------------------------------------------------------------------------
void __fastcall FindStationsThread::Execute()
{
    //---- Place thread code here ----
    int i, j = 0, k;
    u32 num;
    HEADER_t hdr;
    STATUS_h stat;
    u8 buf[128];
    bool res;
    int baud;
    static t1 = 0;
    char str[128];

    enter = true;


    frmMain->sbRunModule->Down = false;
    frmMain->cbPortName->Items->Clear();	// Очистить список портов

    if (t1 % 2 == 0) {
	    frmMain->sbInfo->Panels->Items[0]->Text = "Searching SPN...";
    } else {
	frmMain->sbInfo->Panels->Items[0]->Text = "";
    }
    t1++;

    // 255 портов
    for (i = 1; i < 255; i++) {
	frmMain->PortName = "\\\\.\\COM" + IntToStr(i);

	hdr.ndas = 0x444E;
	hdr.crc16 = 0;		/* CRC16 посылки ПОСЛЕ HEADER_t если есть */
	hdr.dev_id = DEV_ADDR; // константа;
	hdr.pack_type = PACK_REQUEST;
	hdr.cmd = CMD_QUERY_STAT;		/* Команда */
	hdr.ver = DEV_VERSION;
	hdr.size = 0;
	hdr.rsvd0 = 0;

	// 20 байта отсылаем, много ожидаем!!!
	// каждый порт опрашываем на 2-х скоростях
	num = frmMain->Querry(frmMain->PortName, (u8*)&hdr, sizeof(HEADER_t),
                 (u8 *) &stat, sizeof(STATUS_h),  &baud);

	if (num == sizeof(STATUS_h) && stat.header.pack_type == PACK_ACK) {	// len д.б == 5 + (1 len + 2 байта crc16)
	    frmMain->port_set[j].port_num = i;
	    frmMain->port_set[j].gns_name = (stat.header.dev_id);
	    frmMain->port_set[j].baud = baud;	// сохраняем скорость


  	    frmMain->port_set[j].board = 'F';
	    frmMain->port_set[j].v = 0;
	    frmMain->port_set[j].r = 2;
	    frmMain->port_set[j].ver = String(str);

 	    sprintf(buf,"Found SPN: GNS%04d on COM%d (%d bit per sec). Recorder ver: %s",
			frmMain->port_set[j].gns_name, i, baud,
			frmMain->port_set[j].ver.c_str());

	    frmMain->LogOut((char *) buf);
	    sprintf(buf, "GNS%04d", frmMain->port_set[j].gns_name);

	    frmMain->cbPortName->Items->Add((char *) buf);	// Добавляем станцию
	    frmMain->cbPortName->ItemIndex = j;	// Индекс внутри combobox
	    j++;
	    frmMain->sbInfo->Panels->Items[0]->Text = String("SPN found/") + String("Baud: ") + IntToStr(baud);
	}
    }
    if (j <= 0) {		// Если прочитали
//       frmMain->LogOut("Станций не найдено!");
    }
    stations = j;
    enter = false;
}

//---------------------------------------------------------------------------
int get_num_station(void)
{
    return stations;
}

//---------------------------------------------------------------------------
void reset_num_station(void)
{
    stations = 0;
}

//---------------------------------------------------------------------------
bool no_enter(void)
{
    return enter;
}

//---------------------------------------------------------------------------
