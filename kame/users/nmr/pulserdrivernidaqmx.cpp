/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitagawa@scphys.kyoto-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
 ***************************************************************************/
#include "pulserdrivernidaqmx.h"

#ifdef HAVE_NI_DAQMX

#include "interface.h"
#include <klocale.h>

static const TaskHandle TASK_UNDEF = ((TaskHandle)-1);

XNIDAQmxPulser::XNIDAQmxPulser(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers) :
    XNIDAQmxDriver<XPulser>(name, runtime, scalarentries, interfaces, thermometers, drivers),
    m_pausingBit(0), m_pausingCount(0), m_pausingBlankBefore(0), m_pausingBlankAfter(0),
 	 m_running(false),
    m_resolutionDO(-1.0),
    m_resolutionAO(-1.0),
	 m_taskAO(TASK_UNDEF),
	 m_taskDO(TASK_UNDEF),
 	 m_taskDOCtr(TASK_UNDEF),
 	 m_taskGateCtr(TASK_UNDEF)
{
	for(unsigned int i = 0; i < NUM_DO_PORTS; i++)
		portSel(i)->add("Pausing(PFI4)");
    const int ports[] = {
    	PORTSEL_GATE, PORTSEL_PREGATE, PORTSEL_TRIG1, PORTSEL_TRIG2,
    	PORTSEL_GATE3, PORTSEL_COMB, PORTSEL_QSW, PORTSEL_ASW
    };
    for(unsigned int i = 0; i < sizeof(ports)/sizeof(int); i++) {
    	portSel(i)->value(ports[i]);
	}
	m_virtualTrigger.reset(new XNIDAQmxInterface::VirtualTrigger(name, NUM_DO_PORTS));
	XNIDAQmxInterface::VirtualTrigger::registerVirtualTrigger(m_virtualTrigger);
}

XNIDAQmxPulser::~XNIDAQmxPulser()
{
	if(m_taskAO != TASK_UNDEF)
	    DAQmxClearTask(m_taskAO);
	if(m_taskDO != TASK_UNDEF)
	    DAQmxClearTask(m_taskDO);
	if(m_taskDOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskDOCtr);
	if(m_taskGateCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskGateCtr);
}

void
XNIDAQmxPulser::openDO(bool use_ao_clock) throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	if(m_taskDO != TASK_UNDEF)
	    DAQmxClearTask(m_taskDO);
	if(m_taskDOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskDOCtr);
	if(m_taskGateCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskGateCtr); 

	if(intfDO()->maxDORate(1) == 0)
		throw XInterface::XInterfaceError(KAME::i18n("HW-timed transfer needed."), __FILE__, __LINE__);
	
	if(m_resolutionDO <= 0.0)
		m_resolutionDO = 1.0 / intfDO()->maxDORate(1);
	fprintf(stderr, "Using DO rate = %f[kHz]\n", 1.0/m_resolutionDO);

	float64 freq = 1e3 / resolution();

	CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskDO));
    CHECK_DAQMX_RET(DAQmxCreateDOChan(m_taskDO, 
    	formatString("%s/port0", intfDO()->devName()).c_str(),
    	 "", DAQmx_Val_ChanForAllLines));

	std::string do_clk_src;
	
	if(use_ao_clock) {
		do_clk_src = formatString("/%s/ao/SampleClock", intfAO()->devName());
	    fprintf(stderr, "Using ao/SampleClock for DO.\n");
	}
	else {
		do_clk_src = formatString("/%s/Ctr0InternalOutput", intfCtr()->devName());
		std::string ctrdev = formatString("%s/ctr0", intfCtr()->devName());
		//Continuous pulse train generation. Duty = 50%.
	    CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskDOCtr));
		CHECK_DAQMX_RET(DAQmxCreateCOPulseChanFreq(m_taskDOCtr, 
	    	ctrdev.c_str(), "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0,
	    	freq, 0.5));
	    //config. of timing is needed for some reasons.
		CHECK_DAQMX_RET(DAQmxCfgImplicitTiming(m_taskDOCtr, DAQmx_Val_ContSamps, 1000));
	    intfCtr()->synchronizeClock(m_taskDOCtr);

		m_virtualTrigger->setArmTerm(do_clk_src.c_str());
	}
   
	const unsigned int BUF_SIZE_HINT = lrint(65.536e-3 * freq * 4);
	//M series needs an external sample clock and trigger for DO channels.
	CHECK_DAQMX_RET(DAQmxCfgSampClkTiming(m_taskDO,
		do_clk_src.c_str(),
		freq, DAQmx_Val_Rising, DAQmx_Val_ContSamps, BUF_SIZE_HINT));
//    intfDO()->synchronizeClock(m_taskDO);
	
	//Buffer setup.
/*	CHECK_DAQMX_RET(DAQmxSetDODataXferReqCond(m_taskDO, 
    	formatString("%s/port0", intfDO()->devName()).c_str(),
		DAQmx_Val_OnBrdMemHalfFullOrLess));
*/
	CHECK_DAQMX_RET(DAQmxCfgOutputBuffer(m_taskDO, BUF_SIZE_HINT));
	uInt32 bufsize;
	CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskDO, &bufsize));
	fprintf(stderr, "Using bufsize = %d, freq = %f\n", (int)bufsize, freq);
	m_bufSizeHintDO = bufsize / 4;
	CHECK_DAQMX_RET(DAQmxGetBufOutputOnbrdBufSize(m_taskDO, &bufsize));
	fprintf(stderr, "On-board bufsize = %d\n", (int)bufsize);
	m_transferSizeHintDO = std::min((unsigned int)bufsize / 2, m_bufSizeHintDO);
	CHECK_DAQMX_RET(DAQmxSetWriteRegenMode(m_taskDO, DAQmx_Val_DoNotAllowRegen));
	
	m_pausingBit = selectedPorts(PORTSEL_PAUSING);
	if(m_pausingBit) {
		m_pausingGateTerm = formatString("/%s/PFI4", intfCtr()->devName());
		std::string gatectrdev = formatString("%s/ctr1", intfCtr()->devName());
		m_pausingSrcTerm = formatString("/%s/Ctr1InternalOutput", intfCtr()->devName());
	
		for(unsigned int i = 0; i < NUM_DO_PORTS; i++) {
			if(m_pausingBit & (1u << i))
				portSel(i)->setUIEnabled(false);
		}

		m_pausingBlankBefore = 2;
		m_pausingBlankAfter = 1;
		m_pausingCount = (m_pausingBlankBefore + m_pausingBlankAfter) * 20;
		
	    CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskGateCtr));
		CHECK_DAQMX_RET(DAQmxCreateCOPulseChanTime(m_taskGateCtr, 
	    	gatectrdev.c_str(), "", DAQmx_Val_Seconds, DAQmx_Val_Low, 
	    	m_pausingBlankBefore * resolution() * 1e-3,
	    	m_pausingBlankAfter * resolution() * 1e-3, 
	    	m_pausingCount * resolution() * 1e-3));

		CHECK_DAQMX_RET(DAQmxCfgImplicitTiming(m_taskGateCtr,
			 DAQmx_Val_FiniteSamps, 1));
	    intfCtr()->synchronizeClock(m_taskGateCtr);

	    CHECK_DAQMX_RET(DAQmxCfgDigEdgeStartTrig(m_taskGateCtr,
			m_pausingGateTerm.c_str(),
	    	DAQmx_Val_Rising));

		CHECK_DAQMX_RET(DAQmxSetStartTrigRetriggerable(m_taskGateCtr, true));
		
		if(!use_ao_clock) {
			CHECK_DAQMX_RET(DAQmxSetPauseTrigType(m_taskDOCtr, DAQmx_Val_DigLvl));
			CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigSrc(m_taskDOCtr, m_pausingSrcTerm.c_str()));
			CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigWhen(m_taskDOCtr, DAQmx_Val_High));
		}
	}
	
	m_suspendDO = true; 	
	m_threadWriteDO.reset(new XThread<XNIDAQmxPulser>(shared_from_this(),
		 &XNIDAQmxPulser::executeWriteDO));
	m_threadWriteDO->resume();

	if(m_taskDOCtr != TASK_UNDEF)
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDOCtr, DAQmx_Val_Task_Reserve));
	if(m_taskGateCtr != TASK_UNDEF)
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskGateCtr, DAQmx_Val_Task_Reserve));
	CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Reserve));
}

void
XNIDAQmxPulser::openAODO() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	stopPulseGen();
	
	if(m_taskAO != TASK_UNDEF)
	    DAQmxClearTask(m_taskAO);
	
	if(intfDO()->maxDORate(1) == 0)
		throw XInterface::XInterfaceError(KAME::i18n("HW-timed transfer needed."), __FILE__, __LINE__);
	if(intfAO()->maxAORate(2) == 0)
		throw XInterface::XInterfaceError(KAME::i18n("HW-timed transfer needed."), __FILE__, __LINE__);
	
	unsigned int oversamp = 1;
	if((m_resolutionDO <= 0.0) || (m_resolutionAO <= 0.0))
	{
		double do_rate = intfDO()->maxDORate(1);
		double ao_rate = intfAO()->maxAORate(2);
		if(ao_rate <= do_rate)
			do_rate = ao_rate;
		else {
			oversamp = lrint(ao_rate / do_rate);
			ao_rate = do_rate * oversamp;
		}
		m_resolutionDO = 1.0 / do_rate;
		m_resolutionAO = 1.0 / ao_rate;
	}
	fprintf(stderr, "Using AO rate = %f[kHz]\n", 1.0/m_resolutionAO);
	
    CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskAO));
	CHECK_DAQMX_RET(DAQmxCreateAOVoltageChan(m_taskAO,
    	formatString("%s/ao0:1", intfAO()->devName()).c_str(), "",
    	-1.0, 1.0, DAQmx_Val_Volts, NULL));
		
	float64 freq = 1e3 / resolutionQAM();
	const unsigned int BUF_SIZE_HINT = lrint(8 * 65.536e-3 * freq);
	
	CHECK_DAQMX_RET(DAQmxCfgSampClkTiming(m_taskAO, "",
		freq, DAQmx_Val_Rising, DAQmx_Val_ContSamps, BUF_SIZE_HINT));
    intfAO()->synchronizeClock(m_taskAO);
    
	openDO(oversamp == 1);
    if(oversamp != 1) {
		//Synchronize ARM.
		CHECK_DAQMX_RET(DAQmxCfgDigEdgeStartTrig(m_taskDOCtr,
			formatString("/%s/ao/StartTrigger", intfAO()->devName()).c_str(),
			DAQmx_Val_Rising));
		//for debugging.
		CHECK_DAQMX_RET(DAQmxExportSignal(m_taskAO, DAQmx_Val_StartTrigger,
			formatString("/%s/PFI6", intfAO()->devName()).c_str() ));
    }
	
	if(m_pausingBit) {
		CHECK_DAQMX_RET(DAQmxSetPauseTrigType(m_taskAO, DAQmx_Val_DigLvl));
		CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigSrc(m_taskAO, m_pausingSrcTerm.c_str()));
		CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigWhen(m_taskAO, DAQmx_Val_High));
		//Dummy trigger.
		CHECK_DAQMX_RET(DAQmxCfgDigEdgeStartTrig(m_taskAO,
			formatString("/%s/20MHzTimebase", intfAO()->devName()).c_str(),
			DAQmx_Val_Rising));
	}

	m_virtualTrigger->setArmTerm(
		formatString("/%s/ao/SampleClock", intfAO()->devName()).c_str());

	//Buffer setup.
	CHECK_DAQMX_RET(DAQmxCfgOutputBuffer(m_taskAO, BUF_SIZE_HINT));
	uInt32 bufsize;
	CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskAO, &bufsize));
	fprintf(stderr, "Using bufsize = %d\n", (int)bufsize);
	m_bufSizeHintAO = bufsize / 4;
	CHECK_DAQMX_RET(DAQmxGetBufOutputOnbrdBufSize(m_taskAO, &bufsize));
	fprintf(stderr, "On-board bufsize = %d\n", (int)bufsize);
	if(!m_pausingBit & (bufsize < 8192uL))
		throw XInterface::XInterfaceError(
			KAME::i18n("Use the pausing feature for a cheap DAQmx board.\n"
			+ KAME::i18n("Look at the port-selection table.")), __FILE__, __LINE__);
	if(!m_pausingBit)
		gWarnPrint(KAME::i18n("Use of the pausing feature is recommended.\n"
			+ KAME::i18n("Look at the port-selection table.")));
	
	m_transferSizeHintAO = std::min((unsigned int)bufsize / 2, m_bufSizeHintAO);
	CHECK_DAQMX_RET(DAQmxSetWriteRegenMode(m_taskAO, DAQmx_Val_DoNotAllowRegen));

	for(unsigned int ch = 0; ch < NUM_AO_CH; ch++) {
	//obtain range info.
		for(unsigned int i = 0; i < CAL_POLY_ORDER; i++)
			m_coeffAODev[ch][i] = 0.0;
		CHECK_DAQMX_RET(DAQmxGetAODevScalingCoeff(m_taskAO, 
			formatString("%s/ao%d", intfAO()->devName(), ch).c_str(),
			m_coeffAODev[ch], CAL_POLY_ORDER));
		CHECK_DAQMX_RET(DAQmxGetAODACRngHigh(m_taskAO,
			formatString("%s/ao%d", intfAO()->devName(), ch).c_str(),
			&m_upperLimAO[ch]));
		CHECK_DAQMX_RET(DAQmxGetAODACRngLow(m_taskAO,
			formatString("%s/ao%d", intfAO()->devName(), ch).c_str(),
			&m_lowerLimAO[ch]));
	}

	m_suspendAO = true;
	m_threadWriteAO.reset(new XThread<XNIDAQmxPulser>(shared_from_this(),
		 &XNIDAQmxPulser::executeWriteAO));
	m_threadWriteAO->resume();

	if(m_taskAO != TASK_UNDEF)
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Reserve));
}

void
XNIDAQmxPulser::close() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	stopPulseGen();
	
	if(m_threadWriteAO) {
		m_threadWriteAO->terminate();
	}
	if(m_threadWriteDO) {
		m_threadWriteDO->terminate();
	}

	XScopedLock<XRecursiveMutex> lockAO(m_mutexAO);
	XScopedLock<XRecursiveMutex> lockDO(m_mutexDO);
	
	if(m_taskAO != TASK_UNDEF)
	    DAQmxClearTask(m_taskAO);
	if(m_taskDO != TASK_UNDEF)
	    DAQmxClearTask(m_taskDO);
	if(m_taskDOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskDOCtr);
	if(m_taskGateCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskGateCtr);
	m_taskAO = TASK_UNDEF;
	m_taskDO = TASK_UNDEF;
	m_taskDOCtr = TASK_UNDEF;
	m_taskGateCtr = TASK_UNDEF;

	m_resolutionDO = -1.0;    
	m_resolutionAO = -1.0;    

	intfDO()->stop();
	intfAO()->stop();
	intfCtr()->stop();
}
void
XNIDAQmxPulser::startPulseGen() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);
	{
		m_suspendDO = true;
		m_suspendAO = true;
		XScopedLock<XRecursiveMutex> lockAO(m_mutexAO);
		XScopedLock<XRecursiveMutex> lockDO(m_mutexDO);
		
		//Non-stop restart has not been implimented.
		stopPulseGen();
		
		//unlock memory.
		munlock(&m_genBufDO[0], m_genBufDO.size() * sizeof(tRawDO));
		munlock(&m_genBufAO[0], m_genBufAO.size() * sizeof(tRawAO));
		if(m_genPatternListAO.get())
			munlock(&m_genPatternListAO->at(0), m_genPatternListAO->size() * sizeof(GenPattern));
		if(m_genPatternListDO.get())
			munlock(&m_genPatternListDO->at(0), m_genPatternListDO->size() * sizeof(GenPattern));
	 	for(unsigned int i = 0; i < NUM_AO_CH; i++) {
	 		for(unsigned int j = 0; j < PAT_QAM_MASK / PAT_QAM_PHASE; j++) {
	 			if(m_genPulseWaveAO[i][j].get() && m_genPulseWaveAO[i][j]->size())
	 				munlock(&m_genPulseWaveAO[i][j]->at(0), m_genPulseWaveAO[i][j]->size() * sizeof(tRawAO));
	 		}
	 	}
		//swap generated pattern lists to new ones.
	 	m_genPatternListAO.reset();
	 	m_genPatternListNextAO.swap(m_genPatternListAO);
		if(m_genPatternListAO.get())
			mlock(&m_genPatternListAO->at(0), m_genPatternListAO->size() * sizeof(GenPattern));
	 	m_genPatternListDO.reset();
	 	m_genPatternListNextDO.swap(m_genPatternListDO);
		mlock(&m_genPatternListDO->at(0), m_genPatternListDO->size() * sizeof(GenPattern));
	 	for(unsigned int i = 0; i < NUM_AO_CH; i++) {
	 		for(unsigned int j = 0; j < PAT_QAM_MASK / PAT_QAM_PHASE; j++) {
	 			m_genPulseWaveAO[i][j].reset();
	 			m_genPulseWaveNextAO[i][j].swap(m_genPulseWaveAO[i][j]);
	 			if(m_genPulseWaveAO[i][j].get() && m_genPulseWaveAO[i][j]->size())
	 				mlock(&m_genPulseWaveAO[i][j]->at(0), m_genPulseWaveAO[i][j]->size() * sizeof(tRawAO));
	 		}
	 	}

	 	//prepare pattern generation.
		m_genLastPatItDO = m_genPatternListDO->begin();
		m_genRestSampsDO = m_genPatternListDO->back().tonext;
		m_genBufDO.resize(m_bufSizeHintDO);
		if(m_taskAO != TASK_UNDEF) {
			m_genLastPatItAO = m_genPatternListAO->begin();
			m_genRestSampsAO = m_genPatternListAO->back().tonext;
			m_genAOIndex = 0;
			m_genBufAO.resize(m_bufSizeHintAO * NUM_AO_CH);
		}
		
		//memory locks.
		mlock(&m_genBufDO[0], m_genBufDO.size() * sizeof(tRawDO));
		if(m_taskAO != TASK_UNDEF) {
			mlock(&m_genBufAO[0], m_genBufAO.size() * sizeof(tRawAO));
		}
	const void *FIRST_OF_MLOCK_MEMBER = &m_genPatternListAO;
	const void *LAST_OF_MLOCK_MEMBER = &m_lowerLimAO[NUM_AO_CH];
		mlock(FIRST_OF_MLOCK_MEMBER, (size_t)LAST_OF_MLOCK_MEMBER - (size_t)FIRST_OF_MLOCK_MEMBER);
	
		if(m_running) {
			//set write positions to the appropriate pos.
			uInt64 curr_pos, total_cnt;
			CHECK_DAQMX_RET(DAQmxGetWriteTotalSampPerChanGenerated(m_taskDO, &total_cnt));
			CHECK_DAQMX_RET(DAQmxGetWriteCurrWritePos(m_taskDO, &curr_pos));
			uInt32 bufsize;
			CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskDO, &bufsize));
			int32 offset = (total_cnt % bufsize) - (curr_pos % bufsize);
		    CHECK_DAQMX_RET(DAQmxSetWriteOffset(m_taskDO, offset));
			if(m_taskAO != TASK_UNDEF) {
				uInt64 curr_pos, total_cnt;
				CHECK_DAQMX_RET(DAQmxGetWriteTotalSampPerChanGenerated(m_taskDO, &total_cnt));
				CHECK_DAQMX_RET(DAQmxGetWriteCurrWritePos(m_taskDO, &curr_pos));
				uInt32 bufsize;
				CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskDO, &bufsize));
				int32 offset = (total_cnt % bufsize) - (curr_pos % bufsize);
			    CHECK_DAQMX_RET(DAQmxSetWriteOffset(m_taskAO, offset));
			}
		}
		else {
			//synchronize the software trigger.
			m_virtualTrigger->start(1e3 / resolution());

			//committing is needed before queries.
			if(m_taskDOCtr != TASK_UNDEF)
				CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDOCtr, DAQmx_Val_Task_Commit));
			if(m_taskGateCtr != TASK_UNDEF)
				CHECK_DAQMX_RET(DAQmxTaskControl(m_taskGateCtr, DAQmx_Val_Task_Commit));
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Commit));
			if(m_taskAO != TASK_UNDEF)
				CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Commit));
			//clear on-board/off-board FIFOs.
			uInt32 on_bufsize, off_bufsize;
			CHECK_DAQMX_RET(DAQmxGetBufOutputOnbrdBufSize(m_taskDO, &on_bufsize));
			CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskDO, &off_bufsize));
//			CHECK_DAQMX_RET(DAQmxSetBufOutputOnbrdBufSize(m_taskDO, 0));
//			CHECK_DAQMX_RET(DAQmxSetBufOutputBufSize(m_taskDO, 0));
			CHECK_DAQMX_RET(DAQmxResetBufOutputOnbrdBufSize(m_taskDO));
			CHECK_DAQMX_RET(DAQmxResetBufOutputBufSize(m_taskDO));
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Commit));
			CHECK_DAQMX_RET(DAQmxSetBufOutputOnbrdBufSize(m_taskDO, on_bufsize));
			CHECK_DAQMX_RET(DAQmxSetBufOutputBufSize(m_taskDO, off_bufsize));
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Commit));
			if(m_taskAO != TASK_UNDEF) {
				CHECK_DAQMX_RET(DAQmxGetBufOutputOnbrdBufSize(m_taskAO, &on_bufsize));
				CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskAO, &off_bufsize));
//				CHECK_DAQMX_RET(DAQmxSetBufOutputOnbrdBufSize(m_taskAO, 0));
//				CHECK_DAQMX_RET(DAQmxSetBufOutputBufSize(m_taskAO, 0));
				CHECK_DAQMX_RET(DAQmxResetBufOutputOnbrdBufSize(m_taskAO));
				CHECK_DAQMX_RET(DAQmxResetBufOutputBufSize(m_taskAO));
				CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Commit));
				CHECK_DAQMX_RET(DAQmxSetBufOutputOnbrdBufSize(m_taskAO, on_bufsize));
				CHECK_DAQMX_RET(DAQmxSetBufOutputBufSize(m_taskAO, off_bufsize));
				CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Commit));
			}

		}
		
		//prefilling of the buffers.
		if(m_taskAO != TASK_UNDEF)
			genBankAO();
		genBankDO();
		atomic<bool> terminated = false;
		atomic<bool> suspended = false;

		if(m_taskAO != TASK_UNDEF) {
			CHECK_DAQMX_RET(DAQmxSetWriteRelativeTo(m_taskAO, DAQmx_Val_FirstSample));
			CHECK_DAQMX_RET(DAQmxSetWriteOffset(m_taskAO, 0));
			writeBufAO(terminated, suspended);
		}
		CHECK_DAQMX_RET(DAQmxSetWriteRelativeTo(m_taskDO, DAQmx_Val_FirstSample));
		CHECK_DAQMX_RET(DAQmxSetWriteOffset(m_taskDO, 0));
		writeBufDO(terminated, suspended);

		m_suspendAO = false;
		m_suspendDO = false;
	}
	if(!m_running) {
		//slave must start before the master.
		if(m_taskGateCtr != TASK_UNDEF)
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskGateCtr));
	    CHECK_DAQMX_RET(DAQmxStartTask(m_taskDO));
		if(m_taskDOCtr != TASK_UNDEF)
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskDOCtr));
		if(m_taskAO != TASK_UNDEF) {
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskAO));
		}
		
		m_running = true;	
	}
}
void
XNIDAQmxPulser::stopPulseGen()
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	m_suspendAO = true;
	m_suspendDO = true;
	XScopedLock<XRecursiveMutex> lockAO(m_mutexAO);
	XScopedLock<XRecursiveMutex> lockDO(m_mutexDO);

	m_virtualTrigger->stop();

	if(m_running) {
		if(m_taskAO != TASK_UNDEF)
		    DAQmxStopTask(m_taskAO);
		if(m_taskDOCtr != TASK_UNDEF)
		    DAQmxStopTask(m_taskDOCtr);
	    DAQmxStopTask(m_taskDO);
		if(m_taskGateCtr != TASK_UNDEF)
		    DAQmxStopTask(m_taskGateCtr);

		//reset counters/buffers.
/*		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Reserve));
		if(m_taskAO != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Reserve));
		if(m_taskGateCtr != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskGateCtr, DAQmx_Val_Task_Reserve));
		if(m_taskDOCtr != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDOCtr, DAQmx_Val_Task_Reserve));

		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Verify));
		if(m_taskAO != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Verify));
		if(m_taskGateCtr != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskGateCtr, DAQmx_Val_Task_Verify));
		if(m_taskDOCtr != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDOCtr, DAQmx_Val_Task_Verify));
*/
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Reserve));
		if(m_taskAO != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Reserve));
		if(m_taskGateCtr != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskGateCtr, DAQmx_Val_Task_Reserve));
		if(m_taskDOCtr != TASK_UNDEF)
			CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDOCtr, DAQmx_Val_Task_Reserve));

		m_running = false;
	}
}
inline XNIDAQmxPulser::tRawAO
XNIDAQmxPulser::aoVoltToRaw(int ch, float64 volt)
{
	float64 x = 1.0;
	float64 y = 0.0;
	float64 *pco = m_coeffAO[ch];
	for(unsigned int i = 0; i < CAL_POLY_ORDER; i++) {
		y += (*pco++) * x;
		x *= volt;
	}
	return lrint(y);
}
inline bool
XNIDAQmxPulser::tryOutputSuspend(const atomic<bool> &flag, 
	XRecursiveMutex &mutex, const atomic<bool> &terminated) {
	if(flag) {
		mutex.unlock();
		while(flag && !terminated) msecsleep(10);
		mutex.lock();
		return true;
	}
	return false;
}
void *
XNIDAQmxPulser::executeWriteAO(const atomic<bool> &terminated)
{
	while(!terminated) {
		writeBufAO(terminated, m_suspendAO);
	}
	return NULL;
}
void *
XNIDAQmxPulser::executeWriteDO(const atomic<bool> &terminated)
{
	while(!terminated) {
		writeBufDO(terminated, m_suspendDO);
	}
	return NULL;
}

void
XNIDAQmxPulser::writeBufAO(const atomic<bool> &terminated, const atomic<bool> &suspended)
{
	XScopedLock<XRecursiveMutex> lock(m_mutexAO);

	if(tryOutputSuspend(suspended, m_mutexAO, terminated))
		return;

 	const double dma_ao_period = resolutionQAM();
	const unsigned int size = m_genBufAO.size() / NUM_AO_CH;
	bool firsttime = true;
	try {
		const unsigned int num_samps = m_transferSizeHintAO;
		for(unsigned int cnt = 0; cnt < size;) {
			int32 samps;
			samps = std::min(size - cnt, num_samps);
			while(!terminated) {
				if(tryOutputSuspend(suspended, m_mutexAO, terminated))
					return;
//				if(bank == m_genBankWriting)
//					throw XInterface::XInterfaceError(KAME::i18n("AO buffer overrun."), __FILE__, __LINE__);
			uInt32 space;
				int ret = DAQmxGetWriteSpaceAvail(m_taskAO, &space);
				if(!ret && (space >= (uInt32)samps))
					break;
				usleep(lrint(1e3 * samps * dma_ao_period));
			}
			if(terminated)
				break;
			CHECK_DAQMX_RET(DAQmxWriteBinaryI16(m_taskAO, samps, false, DAQmx_Val_WaitInfinitely, 
				DAQmx_Val_GroupByScanNumber,
				&m_genBufAO[cnt * NUM_AO_CH],
				&samps, NULL));
			if(firsttime) {
			   CHECK_DAQMX_RET(DAQmxSetWriteRelativeTo(m_taskAO, DAQmx_Val_CurrWritePos));
			   firsttime = false;
			}
			cnt += samps;
		}
	}
	catch (XInterface::XInterfaceError &e) {
		e.print(getLabel());
		stopPulseGen();
		return;
	}
	if(terminated)
		return;
 	genBankAO();
	return;
}
void
XNIDAQmxPulser::writeBufDO(const atomic<bool> &terminated, const atomic<bool> &suspended)
{
	XScopedLock<XRecursiveMutex> lock(m_mutexDO);

	if(tryOutputSuspend(suspended, m_mutexDO, terminated))
		return;

 	const double dma_do_period = resolution();
	const unsigned int size = m_genBufDO.size();
	bool firsttime = true;
	try {
		const unsigned int num_samps = m_transferSizeHintDO;
		for(unsigned int cnt = 0; cnt < size;) {
			int32 samps;
			samps = std::min(size - cnt, num_samps);
			while(!terminated) {
				if(tryOutputSuspend(suspended, m_mutexDO, terminated))
					return;
//				if(bank == m_genBankWriting)
//					throw XInterface::XInterfaceError(KAME::i18n("DO buffer overrun."), __FILE__, __LINE__);
			uInt32 space;
				int ret = DAQmxGetWriteSpaceAvail(m_taskDO, &space);
				if(!ret && (space >= (uInt32)samps))
					break;
				usleep(lrint(1e3 * samps * dma_do_period));
			}
			if(terminated)
				break;
			CHECK_DAQMX_RET(DAQmxWriteDigitalU16(m_taskDO, samps, false, DAQmx_Val_WaitInfinitely, 
				DAQmx_Val_GroupByScanNumber,
				&m_genBufDO[cnt],
				&samps, NULL));
			if(firsttime) {
			   CHECK_DAQMX_RET(DAQmxSetWriteRelativeTo(m_taskDO, DAQmx_Val_CurrWritePos));
			   firsttime = false;
			}
			cnt += samps;
		}
	}
	catch (XInterface::XInterfaceError &e) {
		e.print(getLabel());
		stopPulseGen();
 		return; 	
	}
	if(terminated)
		return;
 	genBankDO();
	return;
}
void
XNIDAQmxPulser::genBankDO()
{
	GenPatternIterator it = m_genLastPatItDO;
	tRawDO patDO = it->pattern;
	uint64_t tonext = m_genRestSampsDO;
	
	shared_ptr<XNIDAQmxInterface::VirtualTrigger> &vt = m_virtualTrigger;
	
	tRawDO *pDO = &m_genBufDO[0];
	const unsigned int size = m_bufSizeHintDO;
	for(unsigned int samps_rest = size; samps_rest;) {
		//number of samples to be written into buffer.
		unsigned int gen_cnt = std::min((uint64_t)samps_rest, tonext);
		
		//write digital pattern.
		for(unsigned int cnt = 0; cnt < gen_cnt; cnt++) {
			*pDO++ = patDO;
		}
		
		tonext -= gen_cnt;
		samps_rest -= gen_cnt;
		ASSERT(samps_rest < size);
		if(tonext == 0) {
			it++;
			if(it == m_genPatternListDO->end()) {
				it = m_genPatternListDO->begin();
//				printf("p.\n");
			}
			vt->changeValue(patDO, (tRawDO)it->pattern, it->time);
			patDO = it->pattern;
			tonext = it->tonext;
		}
	}
	ASSERT(pDO == &m_genBufDO[m_genBufDO.size()]);
	m_genRestSampsDO = tonext;
	m_genLastPatItDO = it;
}
void
XNIDAQmxPulser::genBankAO()
{
	GenPatternIterator it = m_genLastPatItAO;
	unsigned int patAO = it->pattern;
	uint64_t tonext = m_genRestSampsAO;
	unsigned int aoidx = m_genAOIndex;

	tRawAO *pAO = &m_genBufAO[0];
	const tRawAO raw_ao0_zero = m_genAOZeroLevel[0];
	const tRawAO raw_ao1_zero = m_genAOZeroLevel[1];
	const unsigned int size = m_bufSizeHintAO;
	for(unsigned int samps_rest = size; samps_rest;) {
		//number of samples to be written into buffer.
		unsigned int gen_cnt = std::min((uint64_t)samps_rest, tonext);
		unsigned int pidx = patAO / (PAT_QAM_PULSE_IDX/PAT_QAM_PHASE);
		if(pidx == 0) {
			//write blank in analog lines.
			aoidx = 0;
			for(unsigned int cnt = 0; cnt < gen_cnt; cnt++) {
				*pAO++ = raw_ao0_zero;
				*pAO++ = raw_ao1_zero;
			}
		}
		else {
			unsigned int pnum = patAO - (PAT_QAM_PULSE_IDX/PAT_QAM_PHASE);
			ASSERT(pnum < PAT_QAM_PULSE_IDX_MASK/PAT_QAM_PULSE_IDX);
			tRawAO *pGenAO0 = &m_genPulseWaveAO[0][pnum]->at(aoidx);
			tRawAO *pGenAO1 = &m_genPulseWaveAO[1][pnum]->at(aoidx);
			ASSERT(m_genPulseWaveAO[0][pnum]->size());
			ASSERT(m_genPulseWaveAO[1][pnum]->size());
			if(m_genPulseWaveAO[0][pnum]->size() < aoidx + gen_cnt) {
				fprintf(stderr, "Oops. This should not happen.\n");
				int lps = m_genPulseWaveAO[0][pnum]->size() - aoidx;
				lps = std::max(0, lps);
				for(int cnt = 0; cnt < lps; cnt++) {
					*pAO++ = *pGenAO0++;
					*pAO++ = *pGenAO1++;
				}
				for(unsigned int cnt = 0; cnt < gen_cnt - lps; cnt++) {
					*pAO++ = raw_ao0_zero;
					*pAO++ = raw_ao1_zero;
				}
			}
			else {
				for(unsigned int cnt = 0; cnt < gen_cnt; cnt++) {
					*pAO++ = *pGenAO0++;
					*pAO++ = *pGenAO1++;
				}
				aoidx += gen_cnt;
			}
		}
		tonext -= gen_cnt;
		samps_rest -= gen_cnt;
		ASSERT(samps_rest < size);
		if(tonext == 0) {
			it++;
			if(it == m_genPatternListAO->end()) {
				it = m_genPatternListAO->begin();
//				printf("p.\n");
			}
			patAO = it->pattern;
			tonext = it->tonext;
		}
	}
	ASSERT(pAO == &m_genBufAO[m_genBufAO.size()]);
	m_genRestSampsAO = tonext;
	m_genLastPatItAO = it;
	m_genAOIndex = aoidx;
}

void
XNIDAQmxPulser::createNativePatterns()
{
  const unsigned int oversamp_ao = lrint(resolution() / resolutionQAM());
	
  const tRawDO pausingbit = m_pausingBit;
  const uint64_t pausing_cnt = m_pausingCount;
  const uint64_t pausing_cnt_blank_before = m_pausingBlankBefore + m_pausingBlankAfter;
  const uint64_t pausing_cnt_blank_after = 1;
      
	m_genPatternListNextDO.reset(new std::vector<GenPattern>);
	m_genPatternListNextAO.reset(new std::vector<GenPattern>);
	uint32_t pat = m_relPatList.back().pattern;
	uint64_t time = 0;
	for(RelPatListIterator it = m_relPatList.begin(); it != m_relPatList.end(); it++)
	{
	 	uint64_t tonext = it->toappear;
		//pattern of digital lines.
		tRawDO patDO = PAT_DO_MASK & pat;
		const unsigned int patAO = pat / PAT_QAM_PHASE;
		if(pausingbit && ((pat & PAT_QAM_PULSE_IDX_MASK) == 0)) {
			patDO &= ~pausingbit;
			while(tonext > pausing_cnt + pausing_cnt_blank_before + pausing_cnt_blank_after) {
				//generate a pausing trigger.
				{
				 	GenPattern genpat(patDO | pausingbit, pausing_cnt_blank_before, time);
			  		m_genPatternListNextDO->push_back(genpat);
			  		genpat.pattern = 0;
			  		genpat.tonext *= oversamp_ao;
			  		genpat.time *= oversamp_ao;
			  		m_genPatternListNextAO->push_back(genpat);
				}
				tonext -= pausing_cnt + pausing_cnt_blank_before;
				time += pausing_cnt + pausing_cnt_blank_before;
				{
				 	GenPattern genpat(patDO, pausing_cnt_blank_after, time);
			  		m_genPatternListNextDO->push_back(genpat);
			  		genpat.pattern = 0;
			  		genpat.tonext *= oversamp_ao;
			  		genpat.time *= oversamp_ao;
			  		m_genPatternListNextAO->push_back(genpat);
				}
				tonext -= pausing_cnt_blank_after;
				time += pausing_cnt_blank_after;
			}
		}
		{
		 	GenPattern genpat(patDO, tonext, time);
	  		m_genPatternListNextDO->push_back(genpat);
	  		genpat.pattern = patAO;
	  		genpat.tonext *= oversamp_ao;
	  		genpat.time *= oversamp_ao;
	  		m_genPatternListNextAO->push_back(genpat);
		}
	 	pat = it->pattern;
	 	time = it->time;
	}

	if(m_taskAO != TASK_UNDEF) {
		const double offset[] = {*qamOffset1(), *qamOffset2()};
		const double level[] = {*qamLevel1(), *qamLevel2()};
		  			  	
		for(unsigned int ch = 0; ch < NUM_AO_CH; ch++) {
		//arrange range info.
			double x = 1.0;
			for(unsigned int i = 0; i < CAL_POLY_ORDER; i++) {
				m_coeffAO[ch][i] = m_coeffAODev[ch][i] * x
					+ ((i == 0) ? offset[ch] : 0);
				x *= level[ch];
			}
		}
	    m_genAOZeroLevel[0] = aoVoltToRaw(0, 0.0);
	    m_genAOZeroLevel[1] = aoVoltToRaw(1, 0.0);
		for(unsigned int i = 0; i < PAT_QAM_PULSE_IDX_MASK/PAT_QAM_PULSE_IDX; i++) {
			std::complex<double> c(pow(10.0, *masterLevel()/20.0), 0);
			for(unsigned int qpsk = 0; qpsk < 4; qpsk++) {
				const unsigned int pnum = i * (PAT_QAM_PULSE_IDX/PAT_QAM_PHASE) + qpsk;
				m_genPulseWaveNextAO[0][pnum].reset(new std::vector<tRawAO>);
				m_genPulseWaveNextAO[1][pnum].reset(new std::vector<tRawAO>);
				for(std::vector<std::complex<double> >::const_iterator it = 
					qamWaveForm(i).begin(); it != qamWaveForm(i).end(); it++) {
					std::complex<double> z(*it * c);
					m_genPulseWaveNextAO[0][pnum]->push_back(aoVoltToRaw(0, z.real()));
					m_genPulseWaveNextAO[1][pnum]->push_back(aoVoltToRaw(1, z.imag()));
				}
				c *= std::complex<double>(0,1);
			}
		}
  	}
}


void
XNIDAQmxPulser::changeOutput(bool output, unsigned int /*blankpattern*/)
{
  if(output)
    {
      if(!m_genPatternListNextDO || m_genPatternListNextDO->empty() )
              throw XInterface::XInterfaceError(KAME::i18n("Pulser Invalid pattern"), __FILE__, __LINE__);
	  startPulseGen();
    }
  else
    {
      stopPulseGen();
    }
  return;
}

#endif //HAVE_NI_DAQMX
