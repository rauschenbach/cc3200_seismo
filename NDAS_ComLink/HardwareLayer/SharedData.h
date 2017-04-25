//------------------------------------------------------------------------------

#ifndef SharedDataH
#define SharedDataH

#include<vcl.h>


//------------------------------------------------------------------------------
//

#define OPERATION_OK 0
#define NO_ANSWER -1
#define EMPTY_ANSWER -2
#define NOT_FULL_ANSWER -3
#define OVERBUNDLED_ANSWER -4
#define INCORRECT_CHECKSUM -5
#define WRONG_LENGTH_BYTE -6
#define WRITE_FAILURE -7
#define THREAD_ALREADY_STARTED -8
#define ITERATION_STATE_PASSED -10
#define HARDWARE_ERROR -100
#define UNKNOWN_ERROR -150
#define OPERATION_FAILURE -200
#define OPERATION_ABORTED -201
#define INCORRECT_PARAMETERS -500

#define MAX_DATA_RETRAINS 10

#define CMD_USS_RESET 0x03
#define CMD_USS_SYNCHRONIZE 0x49
#define CMD_USS_SET_ZERO 0x04
#define CMD_USS_SET_ZERO_VALUE 0x4A
#define CMD_USS_SET_CONSTANTS 0x44
#define CMD_USS_CLEAR_STATUS 0x08
#define CMD_USS_START 0x45
#define CMD_USS_STOP 0x07
#define CMD_RESET_COUNTERS 0x0A

#define WM_EXCHANGE_ITERATION_PASSED (WM_USER + 1)
#define WM_USER_COMMAND_STATE (WM_USER + 3)

//------------------------------------------------------------------------------
//

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef long int lint;

//------------------------------------------------------------------------------
//

#pragma pack(push, 1)
struct CONNECTION_PARAMETERS {
	uchar  COMPort;
	uint   PortSpeed;
        uchar  MaxFullRetrains;
        ushort MaxTimeout;
};

//------------------------------------------------------------------------------
//

struct EXCHANGE_PARAMETERS {
    bool IsGetSeconds;
    bool IsGetStatus;
    bool IsGetConstants;
    bool IsGetAlarmData;
    bool IsGetData;
};

//------------------------------------------------------------------------------
//

struct USER_COMMAND_PARAMETER {
    uchar DataLength;
    char Data[128];
};

struct USER_COMMAND {
    uint  CommandID;
    uchar Command;
    USER_COMMAND_PARAMETER Parameter;
};

//------------------------------------------------------------------------------
//

struct USS_TIME {
    uchar Length;
    uint  Time;
};

//------------------------------------------------------------------------------
//

struct USS_STATUS {
    uchar Length;
    char  Main;
    char  Test;
};

//------------------------------------------------------------------------------
//

struct MEASUREMENT_TASK {
    uint MeasurementInterval;
    int  CriticalTime;
};

//------------------------------------------------------------------------------
//

struct USS_MEASUREMENT {
    uchar Length;
    char  MainStatus;
    char  TestStatus;
    uint  Time;
    int   Value;
};

//------------------------------------------------------------------------------
//

struct USS_ALARM_DATA {
    uchar Length;
    uint  TriggerBeforeTime;
    uint  TriggerAfterTime;
    int   TriggerBetweenCount;
};

//------------------------------------------------------------------------------
//

struct USS_CONSTANTS {
    double RotationCoefficient;
};

struct USS_CONSTANTS_EX {
    uchar  Length;
    double RotationCoefficient;
    MEASUREMENT_TASK Task;
};

//------------------------------------------------------------------------------
// Ñ÷åò÷èêè ÓÊ

struct DEVICE_COUNTERS {
    uint Counters[9];
};

struct CC_COUNTERS {
    DEVICE_COUNTERS PrimaryPortCounters;
};

//------------------------------------------------------------------------------
//

struct __NO_DATA {
    char N[128];
};

//------------------------------------------------------------------------------
//

#pragma pack(pop)

#endif
