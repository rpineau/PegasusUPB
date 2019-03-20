//
//
//  Created by Rodolphe Pineau on 3/11/2019.


#ifndef __PEGASUS_C__
#define __PEGASUS_C__

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <math.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"

#define PEGA_DEBUG 2

#ifdef PEGA_DEBUG
#if defined(SB_WIN_BUILD)
#define PEGA_LOGFILENAME "C:\\PegasusLog.txt"
#elif defined(SB_LINUX_BUILD)
#define PEGA_LOGFILENAME "/tmp/PegasusLog.txt"
#elif defined(SB_MAC_BUILD)
#define PEGA_LOGFILENAME "/tmp/PegasusLog.txt"
#endif
#endif

#define SERIAL_BUFFER_SIZE 1024
#define MAX_TIMEOUT 1000
#define TEXT_BUFFER_SIZE    1024

enum DMFC_Errors    {PB_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, UPB_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum DeviceType     {NONE = 0, UPB, PPB};
enum MotorType      {DC = 0, STEPPER};
enum GetLedStatus   {OFF = 0, ON};
enum SetLEdStatus   {SWITCH_OFF = 1, SWITCH_ON};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};


typedef struct {
    int     nMotorType;
    double  dTemperature;
    int     nCurPos;
    bool    bMoving;
    bool    bReverse;
    int     nBacklash;
} upbFocuser;

typedef struct {
    int     nDeviceType;
    bool    bReady;
    char    szVersion[TEXT_BUFFER_SIZE];
    int     nLedStatus;
    float   fVoltage;
    float   fCurent;
    int     nPower;
    float   fTemp;
    int     nHumidity;
    float   fDewPoint;
    bool    bPort1On;
    bool    bPort2On;
    bool    bPort3On;
    bool    bPort4On;
    bool    bOnBootPort1On;
    bool    bOnBootPort2On;
    bool    bOnBootPort3On;
    bool    bOnBootPort4On;
    bool    bUsbPortOff;
    int     nDew1PWM;
    int     nDew2PWM;
    float   fCurrentPort1;
    float   fCurrentPort2;
    float   fCurrentPort3;
    float   fCurrentPort4;
    float   fCurrentDew1;
    float   fCurrentDew2;
    bool    bOverCurrentPort1;
    bool    bOverCurrentPort2;
    bool    bOverCurrentPort3;
    bool    bOverCurrentPort4;
    bool    bOverCurrentDew1;
    bool    bOverCurrentDew2;
    bool    bAutoDew;
    upbFocuser focuser;
} upbStatus;

// field indexes in response for PA command
#define upbVoltage      0
#define upbCurrent      1
#define upbPower        2
#define upbTemp         3
#define upbHumidity     4
#define upbDewPoint     5
#define upbPortStatus   6
#define upbUsbStatus    7
#define upbDew1PWM      8
#define upbDew2PWM      9
#define upbCurrentPort1 10
#define upbCurrentPort2 11
#define upbCurrentPort3 12
#define upbCurrentPort4 13
#define upbCurrentDew1  14
#define upbCurrentDew2  15
#define upbOvercurent   16
#define upbAutodew      17


// field indexes in response for SA command
#define upbFPos         0
#define upbFMotorState  1
#define upbFMotorInvert 2
#define upbFBacklash    3

class CPegasusUPB
{
public:
    CPegasusUPB();
    ~CPegasusUPB();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void        setLogger(LoggerInterface *pLogger) { m_pLogger = pLogger; };

    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    int         isMotorMoving(bool &bMoving);

    // getter and setter
    int         getStatus(int &nStatus);
    int         getStepperStatus(void);
    int         getConsolidatedStatus(void);
    int         getOnBootPowerState(void);

    int         getMotoMaxSpeed(int &nSpeed);
    int         setMotoMaxSpeed(int nSpeed);

    int         getBacklashComp(int &nSteps);
    int         setBacklashComp(int nSteps);

    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    int         getTemperature(double &dTemperature);

    int         getPosition(int &nPosition);

    int         getLedStatus(int &nStatus);
    int         setLedStatus(int nStatus);

    int         syncMotorPosition(int nPos);

    int         getDeviceType(int &nDevice);

    int         getPosLimit(void);
    void        setPosLimit(int nLimit);

    bool        isPosLimitEnabled(void);
    void        enablePosLimit(bool bEnable);

    int         setReverseEnable(bool bEnabled);
    int         getReverseEnable(bool &bEnabled);

    float       getVoltage();
    float       getCurrent();
    int         getPower();
    float       getTemp();
    int         getHumidity();
    float       getDewPoint();

    bool        getPortOn(const int &nPortNumber);
    float       getPortCurrent(const int &nPortNumber);
    bool        getOnBootPortOn(const int &nPortNumber);
    bool        isOverCurrentPort(const int &nPortNumber);

protected:

    int             upbCommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *pszRespBuffer, int nBufferLen);
    int             parseResp(char *pszResp, std::vector<std::string>  &sParsedRes);



    SerXInterface   *m_pSerx;
    LoggerInterface *m_pLogger;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[TEXT_BUFFER_SIZE];

    std::vector<std::string>    m_svParsedRespForSA;
    std::vector<std::string>    m_svParsedRespForPA;

    upbStatus       m_globalStatus;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bPosLimitEnabled;
	bool			m_bAbborted;
	
#ifdef PEGA_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};

#endif //__PEGASUS_C__
