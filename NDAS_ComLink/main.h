//---------------------------------------------------------------------------

#ifndef mainH
#define mainH
//---------------------------------------------------------------------------

#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Buttons.hpp>
#include <ComCtrls.hpp>
#include <Grids.hpp>
#include <Chart.hpp>
#include <TeEngine.hpp>
#include <TeeProcs.hpp>
#include <Series.hpp>
#include <Mask.hpp>
#include <ToolWin.hpp>
#include <Menus.hpp>
#include <ImgList.hpp>
#include <time.h>
#include "my_cmd.h"
#include "LPComponent.h"
#include "LPDrawLayers.h"
#include "SLComponentCollection.h"
#include "SLScope.h"
#include "ComPort.h"
#include "ReadPortThread.h"
#include <Dialogs.hpp>
#include <BMSpinEdit.h>
#include <light.h>
#include "proto.h"
#include <FileCtrl.hpp>


#define DATA_SIZE               1

#define CM_READ_DATA            WM_USER+401
#define CM_NO_DATA              WM_USER+402 // Нет данных
#define CM_SHOW_TEST_RESULT     WM_USER+403 // результат тестирования
#define CM_SHOW_LINK            WM_USER+404
#define CM_SHOW_STATUS          WM_USER+405
#define CM_SHOW_WORK_TIME       WM_USER+406
#define CM_SHOW_PORT_COUNTS     WM_USER+407
#define CM_SHOW_PARAM_DATA      WM_USER+408

#define CLEAR_DATA              1


#define INFO_BOX(str) 	    Application->MessageBox(str, frmMain->Caption.c_str(), MB_OK | MB_ICONINFORMATION)
#define ERROR_BOX(str)      Application->MessageBox(str, frmMain->Caption.c_str(), MB_OK | MB_ICONERROR)
#define WARN_BOX(str)       Application->MessageBox(str, frmMain->Caption.c_str(), MB_OK | MB_ICONWARNING)
#define YESNO_BOX(str)	    Application->MessageBox(str, frmMain->Caption.c_str(), MB_YESNO | MB_ICONQUESTION)	
#define YESNOCANCEL_BOX(str)	    Application->MessageBox(str, frmMain->Caption.c_str(), MB_YESNOCANCEL | MB_ICONQUESTION)	


//---------------------------------------------------------------------------
// Режимы наладки
enum mode {
        off_mode,
        cmd_mode,
        oper_mode,
        ctrl_mode
};
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class TfrmMain : public TForm
{
__published:	// IDE-managed Components
        TStatusBar *sbInfo;
        TToolBar *tbButtons;
        TToolButton *btnReset;
        TPanel *pnUp;
        TGroupBox *gbTimes;
        TGroupBox *gbParams;
        TPanel *pnConfig1;
        TToolButton *btnPowerOff;
        TSaveDialog *sdParamFile;
        TMainMenu *menuMain;
        TMenuItem *MenuFile;
        TMenuItem *menuSave;
        TMenuItem *N3;
        TMenuItem *menu10Min;
        TMenuItem *menu60Min;
        TMenuItem *menu1Day;
        TMenuItem *menu1Week;
        TMenuItem *N7;
        TMenuItem *menuExit;
        TMenuItem *menuCtrl;
        TMenuItem *menuOperator;
        TMenuItem *menuHelp;
        TMenuItem *menuAbout;
        TToolButton *ToolButton5;
        TMemo *memoLog;
        TToolButton *btnStartThread;
        TMenuItem *menuAdjust;
        TPanel *pnStatus;
        TGroupBox *gbStatus1;
        TLabel *lbStatus1_0;
        TLabel *lbStatus1_1;
        TLabel *lbStatus1_2;
        TLabel *lbStatus1_3;
        TLabel *lbStatus1_4;
        TLabel *lbStatus1_5;
        TLabel *lbStatus1_6;
        TLabel *lbStatus1_7;
        TGroupBox *gbStatus0;
        TLabel *lbStatus0_3;
        TLabel *lbStatus0_4;
        TLabel *lbStatus0_5;
        TLabel *lbStatus0_6;
        TLabel *lbStatus0_7;
        TPanel *pnStatusAndErrors;
        TGroupBox *gbResetCause;
        TLabel *lbStatusReset_0;
        TLabel *lbStatusReset_1;
        TLabel *lbStatusReset_2;
        TLabel *lbStatusReset_3;
        TLabel *lbStatusReset_4;
        TLabel *lbStatusReset_5;
        TLabel *Label17;
        TLabel *Label18;
        TGroupBox *gbCounts;
        TPanel *pnGnsTotalRx;
        TPanel *pnGnsTotalTx;
        TPanel *pnTxPack;
        TPanel *pnGnsFrameError;
        TPanel *pnRxStatErr;
        TPanel *pnGnsCmdError;
        TPanel *pnRxCmdErr;
        TPanel *pnGnsCrcError;
        TPanel *pnRxCrcErr;
        TPanel *pnRxPack;
        TGroupBox *gbMyCounts;
        TPanel *pnPcTotalRx;
        TPanel *pnPcTotalTx;
        TPanel *pnTxPackMy;
        TPanel *pnPcCntPack;
        TPanel *pnRxStatErrMy;
        TPanel *pnPcCmdError;
        TPanel *pnRxCmdErrMy;
        TPanel *pnPcXchgErr;
        TPanel *pnRxCrcErrMy;
        TPanel *pnRxPackMy;
        TToolButton *btnTest;
        TSpeedButton *sbRunModule;
        TOpenDialog *odParamFile;
        TMenuItem *menuOpen;
        TToolButton *btnGpsOn;
        TGroupBox *gbStatus2;
        TLabel *lbStatus2_0;
        TLabel *lbStatus2_3;
        TLabel *lbStatus2_4;
        TLabel *lbStatus2_5;
        TLabel *lbStatus2_6;
        TLabel *lbStatus2_7;
        TTimer *timIdle;
        TTimer *tmFind;
        TLabel *Label14;
        TLabel *lbGpsTime;
        TImageList *imgList;
        TGroupBox *gbEnviron;
        TGroupBox *gbEnvironement;
        TPanel *Panel31;
        TPanel *pnPressure;
        TPanel *Panel42;
        TPanel *pnTemp0;
        TGroupBox *gbOrientation;
        TPanel *pnPitchName;
        TPanel *pnPitch;
        TPanel *pnRollName;
        TPanel *pnRoll;
        TPanel *pnHeadName;
        TPanel *pnHead;
        TGroupBox *gbAdc;
        TPanel *pnAdcNum;
        TPanel *Panel3;
        TPanel *Panel6;
        TPanel *Panel9;
        TPanel *Panel11;
        TPanel *Panel13;
        TLight *ltAdc0;
        TPanel *Panel14;
        TLight *ltAdc1;
        TPanel *Panel15;
        TLight *ltAdc2;
        TPanel *Panel16;
        TLight *ltAdc3;
        TPanel *pnAdcStatus;
        TListBox *cbPortName;
        TMenuItem *menuOpenLogs;
        TOpenDialog *odOpenLogs;
        TOpenDialog *odOpenLogPath;
        TLabel *lbStatus0_0;
        TLabel *lbStatus0_1;
        TLabel *lbStatus2_2;
        TBevel *Bevel1;
        TLabel *lbStatus2_6m;
        TLabel *lbStatus2_7m;
        TLabel *lbRele4;
        TLabel *lbRele5;
        TLabel *lbRele2;
        TLabel *lbRele3;
        TLabel *lbRele6;
        TLabel *lbRele7;
        TLabel *lbStatus0_2;
        TLabel *lbRele0;
        TMenuItem *menu1Month;
        TMenuItem *menuLang;
        TMenuItem *menuRus;
        TMenuItem *N9;
        TMenuItem *menuEng;
        TPanel *pnChangeMode;
        TGroupBox *gbScope;
        TSLScope *slsDisplay;
        TGroupBox *gbContr;
        TLabel *Label4;
        TLabel *lbAdcFreqName;
        TLabel *lbNumPoint;
        TLabel *lbMux;
        TComboBox *cbPGA;
        TComboBox *cbAdcFreq;
        TComboBox *cbTimeLimit;
        TComboBox *cbMux;
        TPanel *Panel22;
        TPanel *pnSupplyV;
        TLabel *Label123123;
        TLabel *lbGpsLat;
        TLabel *Label20;
        TLabel *lbGpsLon;
        TBitBtn *btnWriteParams;
        TLabel *Label1;
        TLabel *Label2;
        TLabel *lbGpsHi;
        TLabel *lbSV;
        TPanel *pnDrift;
        TPanel *Panel2;

        void __fastcall FormResize(TObject *Sender);
        void __fastcall btnCloseClick(TObject *Sender);
        void __fastcall FormActivate(TObject *Sender);
        void __fastcall SLScope1XAxisCustomLabel(TObject *Sender,
          Real SampleValue, AnsiString &AxisLabel);
        void __fastcall FormCloseQuery(TObject *Sender, bool &CanClose);
        void __fastcall FormDestroy(TObject *Sender);
        void __fastcall btnTestClick(TObject *Sender);
        void __fastcall sbRunModuleClick(TObject *Sender);
        void __fastcall btnStopClick(TObject *Sender);
        void __fastcall timIdleTimer(TObject *Sender);
        void __fastcall sbInfoDblClick(TObject *Sender);
        void __fastcall tmFindTimer(TObject *Sender);


        void __fastcall Splitter1Moved(TObject *Sender);
        void __fastcall cbTimeLimitChange(TObject *Sender);
        void __fastcall menuAboutClick(TObject *Sender);
        void __fastcall btnGpsOnClick(TObject *Sender);
        void __fastcall btnStartThreadClick(TObject *Sender);
        void __fastcall btnWriteParamsClick(TObject *Sender);

public:		// User declarations
        __fastcall TfrmMain(TComponent* Owner);

        void __fastcall  PrintLog(String text, u8 * buf, int len);
        void __fastcall  GetModemParameters(void);
        void __fastcall  SetModemButtons(bool);

        int  __fastcall Querry(String name, u8* send, int tx, u8* recv, int rx, int*);
	void __fastcall LogOut(char*, bool log_only = false);


        void __fastcall TimeGNS110(long);
        int __fastcall  TimeGet(void);
        void __fastcall TimeUTC(void);
        int  __fastcall TimeSyncOld(void);
        int  __fastcall DataGet(void);


        void __fastcall ShowParamData(void*);

        void __fastcall ShowModemTime(SYSTEMTIME * st);
        void __fastcall ShowAlarmTime(SYSTEMTIME * st);
        void __fastcall ShowCalTime(u16 h0, u16 m0, u16 h1, u16 m1);
        void __fastcall TerminateAllThreads(void);

        void __fastcall ShowTimeForRelease(int len);
        void __fastcall ShowBurnLen(int len);
        void __fastcall TerminateScope(void);

private:
        ADS131_Regs  regs;
        double x[NUM_ADS131_PACK];
        double y[NUM_ADS131_PACK];
        double z[NUM_ADS131_PACK];
        double h[NUM_ADS131_PACK];
        double t[NUM_ADS131_PACK];

        s64 big_timer;

public:		// User declarations
        int __fastcall CanPowerOffModem(void);
        DEV_STATUS_STRUCT   dev_addr;


        void __fastcall HideButtons(void);
        void __fastcall UnhideButtons(void);
        void __fastcall ShowStatus(void*);
        void __fastcall ShowPortCounts(void*);
        void __fastcall ShowMyCounts(void* v);
        void __fastcall ShowWorkTime(void* v);
        bool __fastcall VolumeIsFat(String);

        void __fastcall WriteDataToFile(void*);

///////


        UART_COUNTS_t  counts;
        void __fastcall  OffMenu(TObject * Sender);
        void __fastcall  EnableControl(void);
public:
        CRITICAL_SECTION time_cs;
        bool bEndThread;
        STATION_PORT_STRUCT port_set[255]; // всего 255 портов
        int   gns_num;

        String PortName;
        HANDLE PortHandle;
        COMMTIMEOUTS m_Timeouts;
        int    index;
        bool   enable_measure;
        DCB    dcb;
        u16    freq;

        u8      pga;

        int                 timer_tick;
        ReadPortThread*     MyThread;
        int numPoint;
        u32                 StartTime;
        float               flt_freq, quartz_freq;
        HANDLE              log_fd;
        long                start_time, curr_time;
        bool                run_ok, gps_on, test_run, enable_log, first_run, end_aqq;
        unsigned short      hhh,mm,ss;


        MODEM_STRUCT         modem;
        ADCP_h               adcp;
        TIMER_CLOCK_h        timer_clock;
        param_file_struct    param_struct;

        void __fastcall     CMReadData(TMessage &Message);
        void __fastcall     CMNoData(TMessage &Message);
        void __fastcall     CMShowStatus(TMessage & Message);
        void __fastcall     CMShowWorkTime(TMessage & Message);
        void __fastcall     CMShowPortCounts(TMessage & Message);
        void __fastcall     CMShowParamData(TMessage &Message);
BEGIN_MESSAGE_MAP
  MESSAGE_HANDLER(CM_READ_DATA,  TMessage, CMReadData)
  MESSAGE_HANDLER(CM_NO_DATA,  TMessage, CMNoData)
  MESSAGE_HANDLER(CM_SHOW_STATUS, TMessage, CMShowStatus)
  MESSAGE_HANDLER(CM_SHOW_WORK_TIME, TMessage, CMShowWorkTime)
  MESSAGE_HANDLER(CM_SHOW_PORT_COUNTS, TMessage, CMShowPortCounts)
  MESSAGE_HANDLER(CM_SHOW_PARAM_DATA, TMessage, CMShowParamData)
END_MESSAGE_MAP(TControl)
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmMain *frmMain;
//---------------------------------------------------------------------------
#endif
