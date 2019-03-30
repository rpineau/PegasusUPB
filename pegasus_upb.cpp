//
//  nexdome.cpp
//  Pegasus Ultimate power box X2 plugin
//
//  Created by Rodolphe Pineau on 3/11/2019.


#include "pegasus_upb.h"


CPegasusUPB::CPegasusUPB()
{
    m_globalStatus.nDeviceType = NONE;
    m_globalStatus.bReady = false;
    memset(m_globalStatus.szVersion,0,TEXT_BUFFER_SIZE);

    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bPosLimitEnabled = false;
    m_bAbborted = false;
    
    m_pSerx = NULL;
    m_pLogger = NULL;


#ifdef PEGA_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_PegasusUPBLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_PegasusUPBLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_PegasusUPBLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PEGA_DEBUG && PEGA_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::CPegasusUPB] build 2019_03_29_1830.\n", timestamp);
    fprintf(Logfile, "[%s] [CPegasusUPB::CPegasusUPB] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CPegasusUPB::~CPegasusUPB()
{
#ifdef	PEGA_DEBUG
	// Close LogFile
	if (Logfile) fclose(Logfile);
#endif
}

int CPegasusUPB::Connect(const char *pszPort)
{
    int nErr = PB_OK;
    int nDevice;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusUPB::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    // 9600 8N1
    nErr = m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if(nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusUPB::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif
	
    // get status so we can figure out what device we are connecting to.
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusUPB::Connect getting device type\n", timestamp);
	fflush(Logfile);
#endif
    nErr = getDeviceType(nDevice);
    if(nErr) {
        if(nDevice != UPB) {
            m_pSerx->close();
            m_bIsConnected = false;
            nErr = ERR_DEVICENOTSUPPORTED;
        }
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusUPB::Connect **** ERROR **** getting device type\n", timestamp);
		fflush(Logfile);
#endif
        return nErr;
    }
    
    nErr = getConsolidatedStatus();
    if(nErr) {
        m_pSerx->close();
        m_bIsConnected = false;
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusUPB::Connect **** ERROR **** getting device full status\n", timestamp);
        fflush(Logfile);
#endif
    }

    return nErr;
}

void CPegasusUPB::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
 
	m_bIsConnected = false;
}

#pragma mark move commands
int CPegasusUPB::haltFocuser()
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = upbCommand("SH\n", NULL, 0);
	m_bAbborted = true;
	
	return nErr;
}

int CPegasusUPB::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusUPB::gotoPosition moving to %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

    sprintf(szCmd,"SM:%d\n", nPos);
    nErr = upbCommand(szCmd, NULL, 0);
    m_nTargetPos = nPos;

    return nErr;
}

int CPegasusUPB::moveRelativeToPosision(int nSteps)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusUPB::moveRelativeToPosision moving by %d steps\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    sprintf(szCmd,"SG:%d\n", nSteps);
    nErr = upbCommand(szCmd, NULL, 0);

    return nErr;
}

#pragma mark command complete functions

int CPegasusUPB::isGoToComplete(bool &bComplete)
{
    int nErr = PB_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    getPosition(m_globalStatus.focuser.nCurPos);
	if(m_bAbborted) {
		bComplete = true;
		m_nTargetPos = m_globalStatus.focuser.nCurPos;
		m_bAbborted = false;
	}
    else if(m_globalStatus.focuser.nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

int CPegasusUPB::isMotorMoving(bool &bMoving)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
    nErr = upbCommand("SI\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(atoi(szResp)) {
        bMoving = true;
        m_globalStatus.focuser.bMoving = MOVING;
    }
    else {
        bMoving = false;
        m_globalStatus.focuser.bMoving = IDLE;
    }

    return nErr;
}

#pragma mark getters and setters
int CPegasusUPB::getStatus(int &nStatus)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    // OK_UPB or OK_PPB
    nErr = upbCommand("P#\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp,"_OK")) {
        if(strstr(szResp,"UPB")) {
            m_globalStatus.nDeviceType = UPB;
        }
        else if(strstr(szResp,"PPB")) {
            m_globalStatus.nDeviceType = PPB;
        }
        else {
            nStatus = UPB_BAD_CMD_RESPONSE;
            m_globalStatus.nDeviceType = NONE;
            return ERR_DEVICENOTSUPPORTED;
        }
        nStatus = PB_OK;
        nErr = PB_OK;
    }
    else {
        nErr = COMMAND_FAILED;
        nStatus = UPB_BAD_CMD_RESPONSE;
    }
    return nErr;
}

int CPegasusUPB::getStepperStatus()
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    // OK_UPB or OK_PPB
    nErr = upbCommand("SA\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse results.
    nErr = parseResp(szResp, m_svParsedRespForSA);
    if(m_svParsedRespForSA.size()<4) {
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CPegasusUPB::getStepperStatus] parsing returned an incomplete answer\n", timestamp);
        fflush(Logfile);
#endif
        return UPB_BAD_CMD_RESPONSE;
    }
    m_globalStatus.focuser.nCurPos = std::stoi(m_svParsedRespForSA[upbFPos]);
    m_globalStatus.focuser.bMoving = std::stoi(m_svParsedRespForSA[upbFMotorState]) == 1?true:false;
    m_globalStatus.focuser.bReverse = std::stoi(m_svParsedRespForSA[upbFMotorInvert] )== 1?true:false;
    m_globalStatus.focuser.nBacklash = std::stoi(m_svParsedRespForSA[upbFBacklash]);

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getStepperStatus] nCurPos = %d\n", timestamp, m_globalStatus.focuser.nCurPos);
    fprintf(Logfile, "[%s] [CPegasusUPB::getStepperStatus] bMoving = %s\n", timestamp, m_globalStatus.focuser.bMoving?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getStepperStatus] bReverse = %s\n", timestamp, m_globalStatus.focuser.bReverse?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getStepperStatus] nBacklash = %d\n", timestamp, m_globalStatus.focuser.nBacklash);
    fflush(Logfile);
#endif
    

    return nErr;
}

int CPegasusUPB::getConsolidatedStatus()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = getStepperStatus();
    if(nErr)
        return nErr;

    nErr = upbCommand("PA\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] ERROR = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus]  about to parse response\n", timestamp);
	fflush(Logfile);
#endif

    // parse response
    nErr = parseResp(szResp, m_svParsedRespForPA);
    if(nErr) {
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] parse error = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus]  response parsing done\n", timestamp);
	fflush(Logfile);
#endif

    if(m_svParsedRespForPA.size()<18) {
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus]  parsing returned an incomplete answer\n", timestamp);
        fflush(Logfile);
#endif
        return UPB_BAD_CMD_RESPONSE;
    }

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] converting value and setting them in m_globalStatus\n", timestamp);
    fflush(Logfile);
#endif

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbDevice] = %s \n", timestamp, m_svParsedRespForPA[upbDevice].c_str());
    fflush(Logfile);
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbVoltage] = %s \n", timestamp, m_svParsedRespForPA[upbVoltage].c_str());
    fflush(Logfile);
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbCurrent] = %s \n", timestamp, m_svParsedRespForPA[upbCurrent].c_str());
    fflush(Logfile);
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbPower] = %s \n", timestamp, m_svParsedRespForPA[upbPower].c_str());
    fflush(Logfile);
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbTemp] = %s \n", timestamp, m_svParsedRespForPA[upbTemp].c_str());
    fflush(Logfile);
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbHumidity] = %s \n", timestamp, m_svParsedRespForPA[upbHumidity].c_str());
    fflush(Logfile);
    fprintf(Logfile, "[%s][CPegasusUPB::getConsolidatedStatus] m_svParsedRespForPA[upbDewPoint] = %s \n", timestamp, m_svParsedRespForPA[upbDewPoint].c_str());
    fflush(Logfile);
#endif

    m_globalStatus.fVoltage = std::stof(m_svParsedRespForPA[upbVoltage]);
    m_globalStatus.fCurent = std::stof(m_svParsedRespForPA[upbCurrent]);
    m_globalStatus.nPower = std::stoi(m_svParsedRespForPA[upbPower]);
    m_globalStatus.fTemp = std::stof(m_svParsedRespForPA[upbTemp]);
    m_globalStatus.nHumidity = std::stoi(m_svParsedRespForPA[upbHumidity]);
    m_globalStatus.fDewPoint = std::stof(m_svParsedRespForPA[upbDewPoint]);

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fVoltage = %3.2f\n", timestamp, m_globalStatus.fVoltage);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurent = %3.2f\n", timestamp, m_globalStatus.fCurent);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nPower = %d\n", timestamp, m_globalStatus.nPower);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fTemp = %3.2f\n", timestamp, m_globalStatus.fTemp);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nHumidity = %d\n", timestamp, m_globalStatus.nHumidity);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fDewPoint = %3.2f\n", timestamp, m_globalStatus.fDewPoint);
    fflush(Logfile);
#endif

    
    m_globalStatus.bPort1On = m_svParsedRespForPA[upbPortStatus].at(0) == '1'? true : false;
    m_globalStatus.bPort2On = m_svParsedRespForPA[upbPortStatus].at(1) == '1'? true : false;
    m_globalStatus.bPort3On = m_svParsedRespForPA[upbPortStatus].at(2) == '1'? true : false;
    m_globalStatus.bPort4On = m_svParsedRespForPA[upbPortStatus].at(3) == '1'? true : false;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nPortStatus = %s\n", timestamp, m_svParsedRespForPA[upbPortStatus].c_str());
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bPort1On = %s\n", timestamp, m_globalStatus.bPort1On?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bPort2On = %s\n", timestamp, m_globalStatus.bPort2On?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bPort3On = %s\n", timestamp, m_globalStatus.bPort3On?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bPort4On = %s\n", timestamp, m_globalStatus.bPort4On?"Yes":"No");
    fflush(Logfile);
#endif

    m_globalStatus.bUsbPortOn = m_svParsedRespForPA[upbUsbStatus].at(0) == '1' ? false: true;
#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nUsbPortOn = %s\n", timestamp, m_svParsedRespForPA[upbUsbStatus].c_str());
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bUsbPortOn = %s\n", timestamp, m_globalStatus.bUsbPortOn?"Yes":"No");
    fflush(Logfile);
#endif

    m_globalStatus.nDew1PWM = std::stoi(m_svParsedRespForPA[upbDew1PWM]);
    m_globalStatus.nDew2PWM = std::stoi(m_svParsedRespForPA[upbDew2PWM]);
    
    m_globalStatus.fCurrentPort1 = std::stof(m_svParsedRespForPA[upbCurrentPort1])/400;
    m_globalStatus.fCurrentPort2 = std::stof(m_svParsedRespForPA[upbCurrentPort2])/400;
    m_globalStatus.fCurrentPort3 = std::stof(m_svParsedRespForPA[upbCurrentPort3])/400;
    m_globalStatus.fCurrentPort4 = std::stof(m_svParsedRespForPA[upbCurrentPort4])/400;

    m_globalStatus.fCurrentDew1 = std::stof(m_svParsedRespForPA[upbCurrentDew1])/400;
    m_globalStatus.fCurrentDew2 = std::stof(m_svParsedRespForPA[upbCurrentDew2])/400;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nDew1PWM = %d\n", timestamp, m_globalStatus.nDew1PWM);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nDew2PWM = %d\n", timestamp, m_globalStatus.nDew2PWM);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurrentPort1 = %3.2f\n", timestamp, m_globalStatus.fCurrentPort1);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurrentPort2 = %3.2f\n", timestamp, m_globalStatus.fCurrentPort2);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurrentPort3 = %3.2f\n", timestamp, m_globalStatus.fCurrentPort3);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurrentPort4 = %3.2f\n", timestamp, m_globalStatus.fCurrentPort4);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurrentDew1 = %3.2f\n", timestamp, m_globalStatus.fCurrentDew1);
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] fCurrentDew2 = %3.2f\n", timestamp, m_globalStatus.fCurrentDew2);
    fflush(Logfile);
#endif

    m_globalStatus.bOverCurrentPort1 = m_svParsedRespForPA[upbOvercurent].at(0) == '1'? true : false;
    m_globalStatus.bOverCurrentPort2 = m_svParsedRespForPA[upbOvercurent].at(1) == '1'? true : false;
    m_globalStatus.bOverCurrentPort3 = m_svParsedRespForPA[upbOvercurent].at(2) == '1'? true : false;
    m_globalStatus.bOverCurrentPort4 = m_svParsedRespForPA[upbOvercurent].at(3) == '1'? true : false;

    m_globalStatus.bOverCurrentDew1 = m_svParsedRespForPA[upbOvercurent].at(4) == '1'? true : false;
    m_globalStatus.bOverCurrentDew2 = m_svParsedRespForPA[upbOvercurent].at(5) == '1'? true : false;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] nOvercurrentStatus = %s\n", timestamp, m_svParsedRespForPA[upbOvercurent].c_str());
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bOverCurrentPort1 = %s\n", timestamp, m_globalStatus.bOverCurrentPort1?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bOverCurrentPort2 = %s\n", timestamp, m_globalStatus.bOverCurrentPort2?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bOverCurrentPort3 = %s\n", timestamp, m_globalStatus.bOverCurrentPort3?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bOverCurrentPort4 = %s\n", timestamp, m_globalStatus.bOverCurrentPort4?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bOverCurrentDew1 = %s\n", timestamp, m_globalStatus.bOverCurrentDew1?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bOverCurrentDew2 = %s\n", timestamp, m_globalStatus.bOverCurrentDew2?"Yes":"No");
    fflush(Logfile);
#endif

    
    m_globalStatus.bAutoDew = m_svParsedRespForPA[upbAutodew].at(0) == '1' ? true : false;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] bAutoDew = %s\n", timestamp, m_globalStatus.bAutoDew?"Yes":"No");
    fflush(Logfile);
#endif

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] Getting on boot power port status\n", timestamp);
    fflush(Logfile);
#endif

    nErr = getOnBootPowerState();

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getConsolidatedStatus] All data collected\n", timestamp);
    fflush(Logfile);
#endif
    return nErr;
}

int CPegasusUPB::getOnBootPowerState()
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> sParsedResp;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    // get power state for all 4 ports
    nErr = upbCommand("PS\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nErr = parseResp(szResp, sParsedResp);
    if(sParsedResp.size()>1 && sParsedResp[1].size()>=4) {
        m_globalStatus.bOnBootPort1On = sParsedResp[1].at(0) == '1'? true : false;
        m_globalStatus.bOnBootPort2On = sParsedResp[1].at(1) == '1'? true : false;
        m_globalStatus.bOnBootPort3On = sParsedResp[1].at(2) == '1'? true : false;
        m_globalStatus.bOnBootPort4On = sParsedResp[1].at(3) == '1'? true : false;
    }

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getOnBootPowerState] sParsedResp[1] : %s\n", timestamp, sParsedResp[1].c_str());
    fprintf(Logfile, "[%s] [CPegasusUPB::getOnBootPowerState] bOnBootPort1On = %s\n", timestamp, m_globalStatus.bOnBootPort1On?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getOnBootPowerState] bOnBootPort2On = %s\n", timestamp, m_globalStatus.bOnBootPort2On?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getOnBootPowerState] bOnBootPort3On = %s\n", timestamp, m_globalStatus.bOnBootPort3On?"Yes":"No");
    fprintf(Logfile, "[%s] [CPegasusUPB::getOnBootPowerState] bOnBootPort4On = %s\n", timestamp, m_globalStatus.bOnBootPort4On?"Yes":"No");
    fflush(Logfile);
#endif

    return nErr;
}

int CPegasusUPB::getMotoMaxSpeed(int &nSpeed)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = upbCommand("SS\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nSpeed = atoi(szResp);
#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CPegasusUPB::getMotoMaxSpeed] nSpeed = %d\n", timestamp, nSpeed);
    fflush(Logfile);
#endif

    return nErr;
}

int CPegasusUPB::setMotoMaxSpeed(int nSpeed)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
    sprintf(szCmd,"SS:%d\n", nSpeed);
    nErr = upbCommand(szCmd, NULL, 0);

    return nErr;
}

int CPegasusUPB::getBacklashComp(int &nSteps)
{
    int nErr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
    nErr = getStepperStatus();
    nSteps = m_globalStatus.focuser.nBacklash;

    return nErr;
}

int CPegasusUPB::setBacklashComp(int nSteps)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusUPB::setBacklashComp setting backlash comp\n", timestamp);
    fflush(Logfile);
#endif

    sprintf(szCmd,"SB:%d\n", nSteps);
    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(!nErr)
        m_globalStatus.focuser.nBacklash = nSteps;

    return nErr;
}


int CPegasusUPB::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = upbCommand("PV\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    strncpy(pszVersion, szResp, nStrMaxLen);
    return nErr;
}

int CPegasusUPB::getTemperature(double &dTemperature)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = upbCommand("ST\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    if(strstr(szResp,"nan")) {
        dTemperature = -100.0f;
    }
    else
        dTemperature = atof(szResp);

    return nErr;
}

int CPegasusUPB::getPosition(int &nPosition)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = upbCommand("SP\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    nPosition = atoi(szResp);

    return nErr;
}

int CPegasusUPB::getLedStatus(int &nStatus)
{
    int nErr = PB_OK;
    int nLedStatus = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> sParsedResp;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = upbCommand("PL\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nErr = parseResp(szResp, sParsedResp);
    nLedStatus = atoi(sParsedResp[1].c_str());
    switch(nLedStatus) {
        case 0:
            nStatus = OFF;
            break;
        case 1:
            nStatus = ON;
            break;
    }

    return nErr;
}

int CPegasusUPB::setLedStatus(int nStatus)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    switch (nStatus) {
        case ON:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "PL:%d\n", SWITCH_ON);
            break;
        case OFF:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "PL:%d\n", SWITCH_OFF);
            break;

        default:
            break;
    }
    nErr = upbCommand(szCmd, NULL, 0);

    return nErr;
}

int CPegasusUPB::syncMotorPosition(int nPos)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "SC:%d\n", nPos);
    nErr = upbCommand(szCmd, NULL, 0);
    return nErr;
}

int CPegasusUPB::getDeviceType(int &nDevice)
{
    int nErr = PB_OK;
    int nStatus;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = getStatus(nStatus);
    nDevice = m_globalStatus.nDeviceType;
    
    return nErr;
}

int CPegasusUPB::getPosLimit()
{
    return m_nPosLimit;
}

void CPegasusUPB::setPosLimit(int nLimit)
{
    m_nPosLimit = nLimit;
}

bool CPegasusUPB::isPosLimitEnabled()
{
    return m_bPosLimitEnabled;
}

void CPegasusUPB::enablePosLimit(bool bEnable)
{
    m_bPosLimitEnabled = bEnable;
}


int CPegasusUPB::setReverseEnable(bool bEnabled)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(bEnabled)
        sprintf(szCmd,"SR:%d\n", REVERSE);
    else
        sprintf(szCmd,"SR:%d\n", NORMAL);

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusUPB::setReverseEnable setting reverse : %s\n", timestamp, szCmd);
    fflush(Logfile);
#endif

    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

#ifdef PEGA_DEBUG
    if(nErr) {
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusUPB::setReverseEnable **** ERROR **** setting reverse (\"%s\") : %d\n", timestamp, szCmd, nErr);
        fflush(Logfile);
    }
#endif

    return nErr;
}

int CPegasusUPB::getReverseEnable(bool &bEnabled)
{
    int nErr = PB_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = getStepperStatus();
    bEnabled = m_globalStatus.focuser.bReverse;

    return nErr;
}

float CPegasusUPB::getVoltage()
{
    return m_globalStatus.fVoltage;
}

float CPegasusUPB::getCurrent()
{
    return m_globalStatus.fCurent;
}

int CPegasusUPB::getPower()
{
    return m_globalStatus.nPower;
}

float CPegasusUPB::getTemp()
{
    return m_globalStatus.fTemp;
}

int CPegasusUPB::getHumidity()
{
    return m_globalStatus.nHumidity;
}

float CPegasusUPB::getDewPoint()
{
    return m_globalStatus.fDewPoint;

}

bool CPegasusUPB::getPortOn(const int &nPortNumber)
{
    switch(nPortNumber) {
        case 1:
            return m_globalStatus.bPort1On;
            break;

        case 2:
            return m_globalStatus.bPort2On;
            break;

        case 3:
            return m_globalStatus.bPort3On;
            break;

        case 4:
            return m_globalStatus.bPort4On;
            break;

        default:
            return false;
            break;
    }
}

int CPegasusUPB::setPortOn(const int &nPortNumber, const bool &bEnabled)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    switch(nPortNumber) {
        case 1:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "P1:%d", bEnabled?1:0);
            m_globalStatus.bPort1On = bEnabled;
            break;
        case 2:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "P2:%d", bEnabled?1:0);
            m_globalStatus.bPort2On = bEnabled;
            break;
        case 3:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "P3:%d", bEnabled?1:0);
            m_globalStatus.bPort3On = bEnabled;
            break;
        case 4:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "P4:%d", bEnabled?1:0);
            m_globalStatus.bPort4On = bEnabled;
            break;
        default:
            return ERR_CMDFAILED;
            break;
    }

    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

    return nErr;
}


float CPegasusUPB::getPortCurrent(const int &nPortNumber)
{
    switch(nPortNumber) {
        case 1:
            return m_globalStatus.fCurrentPort1;
            break;

        case 2:
            return m_globalStatus.fCurrentPort2;
            break;

        case 3:
            return m_globalStatus.fCurrentPort3;
            break;

        case 4:
            return m_globalStatus.fCurrentPort4;
            break;

        default:
            return 0.0f;
            break;
    }
}


bool CPegasusUPB::getOnBootPortOn(const int &nPortNumber)
{
    switch(nPortNumber) {
        case 1:
            return m_globalStatus.bOnBootPort1On;
            break;

        case 2:
            return m_globalStatus.bOnBootPort2On;
            break;

        case 3:
            return m_globalStatus.bOnBootPort3On;
            break;

        case 4:
            return m_globalStatus.bOnBootPort4On;
            break;

        default:
            return false;
            break;
    }
}

int CPegasusUPB::setOnBootPortOn(const int &nPortNumber, const bool &bEnable)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
    std::string sPorts;
    
    switch(nPortNumber) {
        case 1:
            m_globalStatus.bOnBootPort1On = bEnable;
            break;
            
        case 2:
            m_globalStatus.bOnBootPort2On = bEnable;
            break;
            
        case 3:
            m_globalStatus.bOnBootPort3On = bEnable;
            break;
            
        case 4:
            m_globalStatus.bOnBootPort4On = bEnable;
            break;
            
        default:
            return false;
            break;
    }

    sPorts.empty();
    sPorts += m_globalStatus.bOnBootPort1On? "1" : "0";
    sPorts += m_globalStatus.bOnBootPort2On? "1" : "0";
    sPorts += m_globalStatus.bOnBootPort3On? "1" : "0";
    sPorts += m_globalStatus.bOnBootPort4On? "1" : "0";

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "PE:%s\n", sPorts.c_str());
    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    
    return nErr;
}


bool CPegasusUPB::isOverCurrentPort(const int &nPortNumber)
{
    switch(nPortNumber) {
        case 1:
            return m_globalStatus.bOverCurrentPort1;
            break;

        case 2:
            return m_globalStatus.bOverCurrentPort2;
            break;

        case 3:
            return m_globalStatus.bOverCurrentPort3;
            break;

        case 4:
            return m_globalStatus.bOverCurrentPort4;
            break;

        default:
            return false;
            break;
    }
}

bool CPegasusUPB::isOverCurrentDewHeater(const int &nPortNumber)
{
    switch(nPortNumber) {
        case 1:
            return m_globalStatus.bOverCurrentDew1;
            break;

        case 2:
            return m_globalStatus.bOverCurrentDew2;
            break;

        default:
            return false;
            break;
    }
}


int CPegasusUPB::setUsbOn(const bool &bEnable)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "PU:%s\n", bEnable?"1":"0");
    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    m_globalStatus.bUsbPortOn = bEnable;
    return nErr;
}

bool CPegasusUPB::getUsbOn(void)
{
    return m_globalStatus.bUsbPortOn;
}

int CPegasusUPB::setDewHeaterPWM(const int &nDewHeater, const int &nPWM)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    switch(nDewHeater) {
        case 1:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "P5:%d\n", nPWM);
            m_globalStatus.nDew1PWM = nPWM;
            break;
        case 2:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "P6:%d\n", nPWM);
            m_globalStatus.nDew2PWM = nPWM;
            break;
        default:
            return ERR_CMDFAILED;
            break;
    }

    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    return nErr;
}

int CPegasusUPB::getDewHeaterPWM(const int &nDewHeater)
{
    switch(nDewHeater) {
        case 1:
            return m_globalStatus.nDew1PWM;
            break;
        case 2:
            return m_globalStatus.nDew2PWM;
            break;
        default:
            return -1;
            break;
    }
}

float CPegasusUPB::getDewHeaterCurrent(const int &nDewHeater)
{
    switch(nDewHeater) {
        case 1:
            return m_globalStatus.fCurrentDew1;
            break;
        case 2:
            return m_globalStatus.fCurrentDew2;
            break;
        default:
            return -1;
            break;
    }
}

int CPegasusUPB::setAutoDewOn(const bool &bOn)
{
    int nErr = PB_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "PD:%s\n", bOn?"1":"0");
    nErr = upbCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    m_globalStatus.bAutoDew = bOn;
#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusUPB::setAutoDewOn  m_globalStatus.bAutoDew : %s\n", timestamp, m_globalStatus.bAutoDew?"Yes":"No");
    fflush(Logfile);
#endif
    return nErr;
}


bool CPegasusUPB::isAutoDewOn()
{
#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusUPB::isAutoDewOn  m_globalStatus.bAutoDew : %s\n", timestamp, m_globalStatus.bAutoDew?"Yes":"No");
    fflush(Logfile);
#endif
    return m_globalStatus.bAutoDew;
}



#pragma mark command and response functions

int CPegasusUPB::upbCommand(const char *pszszCmd, char *pszResult, unsigned long nResultMaxLen)
{
    int nErr = PB_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusUPB::upbCommand Sending %s\n", timestamp, pszszCmd);
	fflush(Logfile);
#endif
    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    // printf("Command %s sent. wrote %lu bytes\n", szCmd, ulBytesWrite);
    if(nErr){
        return nErr;
    }

    if(pszResult) {
        // read response
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr){
            return nErr;
        }
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusUPB::upbCommand response \"%s\"\n", timestamp, szResp);
		fflush(Logfile);
#endif
        // printf("Got response : %s\n",resp);
        strncpy(pszResult, szResp, nResultMaxLen);
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusUPB::upbCommand response copied to pszResult : \"%s\"\n", timestamp, pszResult);
		fflush(Logfile);
#endif
    }
    return nErr;
}

int CPegasusUPB::readResponse(char *pszRespBuffer, unsigned long nBufferLen)
{
    int nErr = PB_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#ifdef PEGA_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] CPegasusUPB::readResponse timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \n

    return nErr;
}


int CPegasusUPB::parseResp(char *pszResp, std::vector<std::string>  &svParsedResp)
{
    std::string sSegment;
    std::vector<std::string> svSeglist;
    std::stringstream ssTmp(pszResp);

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusUPB::parseResp parsing \"%s\"\n", timestamp, pszResp);
	fflush(Logfile);
#endif
	svParsedResp.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, ':'))
    {
        svSeglist.push_back(sSegment);
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusUPB::parseResp sSegment : %s\n", timestamp, sSegment.c_str());
        fflush(Logfile);
#endif
    }

    svParsedResp = svSeglist;


    return PB_OK;
}
