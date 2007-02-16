#include "pulserdrivernidaqmx.h"

#ifdef HAVE_NI_DAQMX

#include "interface.h"
#include <klocale.h>

static const bool USE_FINITE_AO = false;
static const TaskHandle TASK_UNDEF = ((TaskHandle)-1);

static const bool USE_PAUSING = false;
static const unsigned int PAUSING_CNT = 400;
static const unsigned int PAUSING_CNT_BLANK = 10;

XNIDAQmxPulser::XNIDAQmxPulser(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers) :
    XNIDAQmxDriver<XPulser>(name, runtime, scalarentries, interfaces, thermometers, drivers),
    m_resolutionDO(-1.0),
    m_resolutionAO(-1.0),
	 m_taskAO(TASK_UNDEF),
	 m_taskDO(TASK_UNDEF),
 	 m_taskDOCtr(TASK_UNDEF),
 	 m_taskGateCtr(TASK_UNDEF),
 	 m_taskAOCtr(TASK_UNDEF)	 
{
    const int ports[] = {
    	PORTSEL_GATE, PORTSEL_PREGATE, PORTSEL_TRIG1, PORTSEL_TRIG2,
    	PORTSEL_GATE3, PORTSEL_COMB, PORTSEL_QSW, PORTSEL_ASW
    };
    for(unsigned int i = 0; i < sizeof(ports)/sizeof(int); i++) {
    	portSel(i)->value(ports[i]);
	}
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
	if(m_taskAOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskAOCtr);
}

void
XNIDAQmxPulser::openDO() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	if(m_taskDO != TASK_UNDEF)
	    DAQmxClearTask(m_taskDO);
	if(m_taskDOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskDOCtr);
	if(m_taskGateCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskGateCtr); 

	if(intfDO()->productInfo()->do_max_rate == 0)
		throw XInterface::XInterfaceError(KAME::i18n("HW-timed transfer needed."), __FILE__, __LINE__);
	
	if(m_resolutionDO <= 0.0)
		m_resolutionDO = 1.0 / intfDO()->productInfo()->do_max_rate;
	fprintf(stderr, "Using DO rate = %f[kHz]\n", 1.0/m_resolutionDO);

//	std::string ctrdev = formatString("%s/freqout", intfDO()->devName()).c_str();
//	std::string ctrout = formatString("/%s/FrequencyOutput", intfDO()->devName()).c_str();
	std::string ctrdev = formatString("%s/ctr0", intfCtr()->devName()).c_str();
	std::string ctrout = formatString("/%s/Ctr0InternalOutput", intfCtr()->devName()).c_str();
	std::string gatectrdev = formatString("%s/ctr1", intfCtr()->devName()).c_str();
	std::string gatectrout = formatString("/%s/Ctr1InternalOutput", intfCtr()->devName()).c_str();
	
	float64 freq = 1e3 / resolution();

	//Continuous pulse train generation. Duty = 50%.
    CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskDOCtr));
	CHECK_DAQMX_RET(DAQmxCreateCOPulseChanFreq(m_taskDOCtr, 
    	ctrdev.c_str(), "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0,
    	freq, 0.5));
    //config. of timing is needed for some reasons.
	CHECK_DAQMX_RET(DAQmxCfgImplicitTiming(m_taskDOCtr, DAQmx_Val_ContSamps, 1000));
   
	CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskDO));

    CHECK_DAQMX_RET(DAQmxCreateDOChan(m_taskDO, 
    	formatString("%s/port0", intfDO()->devName()).c_str(),
    	 "", DAQmx_Val_ChanForAllLines));

	const unsigned int BUF_SIZE_HINT = 65536;
	//M series needs an external sample clock and trigger for DO channels.
	CHECK_DAQMX_RET(DAQmxCfgSampClkTiming(m_taskDO,
		ctrout.c_str(),
		freq, DAQmx_Val_Rising, DAQmx_Val_ContSamps, BUF_SIZE_HINT));
	
	//Buffer setup.
/*	CHECK_DAQMX_RET(DAQmxSetDODataXferReqCond(m_taskDO, 
    	formatString("%s/port0", intfDO()->devName()).c_str(),
		DAQmx_Val_OnBrdMemHalfFullOrLess));
*/
	CHECK_DAQMX_RET(DAQmxCfgOutputBuffer(m_taskDO, BUF_SIZE_HINT));
	uInt32 bufsize;
	CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskDO, &bufsize));
	printf("Using bufsize = %d, freq = %f\n", (int)bufsize, freq);
	m_bufSizeHintDO = bufsize;
	CHECK_DAQMX_RET(DAQmxGetBufOutputOnbrdBufSize(m_taskDO, &bufsize));
	printf("On-board bufsize = %d\n", (int)bufsize);
	CHECK_DAQMX_RET(DAQmxSetWriteRegenMode(m_taskDO, DAQmx_Val_DoNotAllowRegen));
	
	if(USE_PAUSING) {
	const unsigned pausing_term = lrint(PAUSING_CNT * resolution() * 1e-3);
	const unsigned pausing_term_blank = lrint(PAUSING_CNT_BLANK * resolution() * 1e-3);
	    CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskGateCtr));
		CHECK_DAQMX_RET(DAQmxCreateCOPulseChanTime(m_taskGateCtr, 
	    	gatectrdev.c_str(), "", DAQmx_Val_Seconds, DAQmx_Val_Low, 0.0,
	    	pausing_term, pausing_term_blank));
		CHECK_DAQMX_RET(DAQmxCfgImplicitTiming(m_taskGateCtr,
			 DAQmx_Val_FiniteSamps, 1));

	    CHECK_DAQMX_RET(DAQmxCfgDigEdgeStartTrig(m_taskGateCtr,
			formatString("/%s/PFI0", intfCtr()->devName()).c_str(),
	    	DAQmx_Val_Rising));

		CHECK_DAQMX_RET(DAQmxSetStartTrigRetriggerable(m_taskGateCtr, true));
		
		CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigSrc(m_taskDOCtr, gatectrout.c_str()));
		CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigWhen(m_taskDOCtr, DAQmx_Val_High));
	}
}

void
XNIDAQmxPulser::openAODO() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	stopPulseGen();
	
	if(m_taskAO != TASK_UNDEF)
	    DAQmxClearTask(m_taskAO);
	if(m_taskAOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskAOCtr);
	
	if(intfDO()->productInfo()->do_max_rate == 0)
		throw XInterface::XInterfaceError(KAME::i18n("HW-timed transfer needed."), __FILE__, __LINE__);
	if(intfAO()->productInfo()->ao_max_rate == 0)
		throw XInterface::XInterfaceError(KAME::i18n("HW-timed transfer needed."), __FILE__, __LINE__);
	
	if((m_resolutionDO <= 0.0) || (m_resolutionAO <= 0.0))
	{
		unsigned long do_rate = intfDO()->productInfo()->do_max_rate;
		unsigned long ao_rate = intfAO()->productInfo()->ao_max_rate;
		if(ao_rate <= do_rate)
			do_rate = ao_rate;
		else {
			unsigned int oversamp = ao_rate / do_rate;
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
		
	openDO();
	
	float64 freq = 1e3 / resolutionQAM();
	const unsigned int BUF_SIZE_HINT = 65536*4;
	
	if(USE_FINITE_AO) {
		ASSERT(!USE_PAUSING);
		
		std::string ctrdev = formatString("%s/ctr1", intfCtr()->devName()).c_str();
		std::string ctrout = formatString("/%s/Ctr1InternalOutput", intfCtr()->devName()).c_str();

	    CHECK_DAQMX_RET(DAQmxCreateTask("", &m_taskAOCtr));
		CHECK_DAQMX_RET(DAQmxCreateCOPulseChanFreq(m_taskAOCtr, 
	    	ctrdev.c_str(), "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0,
	    	freq, 0.5));
		CHECK_DAQMX_RET(DAQmxCfgImplicitTiming(m_taskAOCtr, DAQmx_Val_FiniteSamps, 1));

	    CHECK_DAQMX_RET(DAQmxCfgDigEdgeStartTrig(m_taskAOCtr,
			formatString("/%s/PFI0", intfCtr()->devName()).c_str(),
	    	DAQmx_Val_Rising));

		CHECK_DAQMX_RET(DAQmxSetStartTrigRetriggerable(m_taskAOCtr, true));

		CHECK_DAQMX_RET(DAQmxCfgSampClkTiming(m_taskAO,
			ctrout.c_str(),
			freq, DAQmx_Val_Rising, DAQmx_Val_ContSamps, BUF_SIZE_HINT));
	}
	else {
		CHECK_DAQMX_RET(DAQmxCfgSampClkTiming(m_taskAO, "",
			freq, DAQmx_Val_Rising, DAQmx_Val_ContSamps, BUF_SIZE_HINT));

		CHECK_DAQMX_RET(DAQmxCfgDigEdgeStartTrig(m_taskDOCtr,
			formatString("/%s/ao/StartTrigger", intfAO()->devName()).c_str(),
			DAQmx_Val_Rising));

		if(USE_PAUSING) {
			std::string gatectrout = formatString("/%s/Ctr1InternalOutput", intfAO()->devName()).c_str();
			CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigSrc(m_taskAO, gatectrout.c_str()));
			CHECK_DAQMX_RET(DAQmxSetDigLvlPauseTrigWhen(m_taskAO, DAQmx_Val_High));
		}
	}

	//Buffer setup.
	CHECK_DAQMX_RET(DAQmxCfgOutputBuffer(m_taskAO, BUF_SIZE_HINT));
	uInt32 bufsize;
	CHECK_DAQMX_RET(DAQmxGetBufOutputBufSize(m_taskAO, &bufsize));
	printf("Using bufsize = %d\n", (int)bufsize);
	m_bufSizeHintAO = bufsize;
	CHECK_DAQMX_RET(DAQmxGetBufOutputOnbrdBufSize(m_taskAO, &bufsize));
	printf("On-board bufsize = %d\n", (int)bufsize);
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
}

void
XNIDAQmxPulser::close() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	stopPulseGen();
	if(m_taskAO != TASK_UNDEF)
	    DAQmxClearTask(m_taskAO);
	if(m_taskAOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskAOCtr);
	if(m_taskDO != TASK_UNDEF)
	    DAQmxClearTask(m_taskDO);
	if(m_taskDOCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskDOCtr);
	if(m_taskGateCtr != TASK_UNDEF)
	    DAQmxClearTask(m_taskGateCtr);
	m_taskAO = TASK_UNDEF;
	m_taskDO = TASK_UNDEF;
	m_taskAOCtr = TASK_UNDEF;
	m_taskDOCtr = TASK_UNDEF;
	m_taskGateCtr = TASK_UNDEF;

	m_resolutionDO = -1.0;    
	m_resolutionAO = -1.0;    

	intfDO()->stop();
	intfAO()->stop();
	intfCtr()->stop();
}

unsigned int
XNIDAQmxPulser::finiteAOSamps(unsigned int finiteaosamps)
{
	unsigned int finiteaocnt = 0;
	unsigned int finiteaorest = 0;
	for(GenPatternIterator it = m_genPatternList.begin(); it != m_genPatternList.end(); it++) {
		unsigned int pat = it->pattern;
		unsigned int gen_cnt = it->tonext;
		tRawDO patDO = PAT_DO_MASK & pat;
		if(patDO & m_ctrTrigBit) {
			finiteaocnt += finiteaorest;
			if(!finiteaocnt) {
				finiteaocnt = 0;
			}
			finiteaocnt += gen_cnt;
			finiteaorest = 0;
		}
		else {
			if(finiteaocnt) {
				finiteaorest += gen_cnt;
				if(finiteaorest > 10) {
					finiteaosamps = std::max(finiteaosamps, finiteaocnt);
					if(finiteaocnt + finiteaorest > finiteaosamps) {
						finiteaocnt = 0;
						finiteaorest = 0;
					}
				}
			}
		}
	}
	return finiteaosamps;
}
void
XNIDAQmxPulser::startPulseGen() throw (XInterface::XInterfaceError &)
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);

	m_ctrTrigBit = selectedPorts(PORTSEL_PREGATE);
	m_pausingBit = selectedPorts(PORTSEL_PAUSING);
	
	 stopPulseGen();
		   
//	std::deque<GenPattern> m_genPatternList;
	m_genLastPatItAO = m_genPatternList.begin();
	m_genRestSampsAO = m_genPatternList.back().tonext;
	m_genLastPatItDO = m_genPatternList.begin();
	m_genRestSampsDO = m_genPatternList.back().tonext;
	
	m_genBufDO.resize(m_bufSizeHintDO);
	m_genBufDO.reserve(m_bufSizeHintDO); //redundant
	if(m_taskAO != TASK_UNDEF) {
		m_genAOIndex = 0;
		m_genBufAO.resize(m_bufSizeHintAO * NUM_AO_CH);
		m_genBufAO.reserve(m_bufSizeHintAO * NUM_AO_CH); //redundant
		if(USE_FINITE_AO) {
			unsigned int oldv = 0;
			unsigned int newv = 2;
			while(oldv != newv) {
				oldv = newv;
				newv = finiteAOSamps(oldv);
			}
			m_genFiniteAOSamps = newv;
			printf("Using finite ao = %u.\n", newv);
			if(m_taskAOCtr != TASK_UNDEF)
				CHECK_DAQMX_RET(DAQmxSetSampQuantSampPerChan(m_taskAOCtr, newv));
		}

	}
	
const void *FIRST_OF_MLOCK_MEMBER = &m_genPatternList;
const void *LAST_OF_MLOCK_MEMBER = &m_lowerLimAO[NUM_AO_CH];
	//Suppress swapping.
	mlock(FIRST_OF_MLOCK_MEMBER, (size_t)LAST_OF_MLOCK_MEMBER - (size_t)FIRST_OF_MLOCK_MEMBER);
	mlock(&m_genBufDO[0], m_genBufDO.size() * sizeof(tRawDO));
	mlock(&m_genBufAO[0], m_genBufAO.size() * sizeof(tRawAO));
	for(unsigned int ch = 0; ch < NUM_AO_CH; ch++) {
		for(unsigned int i = 0; i < 32; i++)
			mlock(&m_genPulseWaveAO[ch][i], m_genPulseWaveAO[ch][i].size() * sizeof(tRawAO));
	}
	
	//prefilling of buffer.
	if(m_taskAO != TASK_UNDEF)
		genBankAO();
	genBankDO();
	
	CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDOCtr, DAQmx_Val_Task_Commit));
	CHECK_DAQMX_RET(DAQmxTaskControl(m_taskDO, DAQmx_Val_Task_Commit));
    CHECK_DAQMX_RET(DAQmxSetWriteRelativeTo(m_taskDO, DAQmx_Val_FirstSample));
    CHECK_DAQMX_RET(DAQmxSetWriteOffset(m_taskDO, 0));
	if(m_taskGateCtr != TASK_UNDEF)
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskGateCtr, DAQmx_Val_Task_Commit));
	if(m_taskAOCtr != TASK_UNDEF)
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAOCtr, DAQmx_Val_Task_Commit));
	if(m_taskAO != TASK_UNDEF) {
		CHECK_DAQMX_RET(DAQmxTaskControl(m_taskAO, DAQmx_Val_Task_Commit));
	    CHECK_DAQMX_RET(DAQmxSetWriteRelativeTo(m_taskAO, DAQmx_Val_FirstSample));
	    CHECK_DAQMX_RET(DAQmxSetWriteOffset(m_taskAO, 0));
	}
	atomic<bool> terminated = false;
	if(m_taskAO != TASK_UNDEF) {
		writeBufAO(terminated);
	}
	writeBufDO(terminated);
 	
	m_threadWriteDO.reset(new XThread<XNIDAQmxPulser>(shared_from_this(),
		 &XNIDAQmxPulser::executeWriteDO));
	m_threadWriteDO->resume();
	if(m_taskAO != TASK_UNDEF) {
		m_threadWriteAO.reset(new XThread<XNIDAQmxPulser>(shared_from_this(),
			 &XNIDAQmxPulser::executeWriteAO));
		m_threadWriteAO->resume();
	}

	//slave must start before the master.
	if(USE_FINITE_AO && (m_taskAOCtr != TASK_UNDEF)) {
		if(m_taskAO != TASK_UNDEF) {
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskAO));
		}
		if(m_taskAOCtr != TASK_UNDEF) {
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskAOCtr));
		}
	    CHECK_DAQMX_RET(DAQmxStartTask(m_taskDOCtr));
	    CHECK_DAQMX_RET(DAQmxStartTask(m_taskDO));
	}
	else {
		if(m_taskGateCtr != TASK_UNDEF) {
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskGateCtr));
		}
	    CHECK_DAQMX_RET(DAQmxStartTask(m_taskDOCtr));
	    CHECK_DAQMX_RET(DAQmxStartTask(m_taskDO));
		if(m_taskAOCtr != TASK_UNDEF) {
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskAOCtr));
		}
		if(m_taskAO != TASK_UNDEF) {
		    CHECK_DAQMX_RET(DAQmxStartTask(m_taskAO));
		}
	}

}
void
XNIDAQmxPulser::stopPulseGen()
{
	XScopedLock<XRecursiveMutex> tlock(m_totalLock);
	
	if(m_threadWriteAO) {
		m_threadWriteAO->terminate();
	}
	if(m_threadWriteDO) {
		m_threadWriteDO->terminate();
	}
	{
		XScopedLock<XMutex> lockAO(m_mutexAO);
		XScopedLock<XMutex> lockDO(m_mutexDO);
		if(m_taskAOCtr != TASK_UNDEF)
		    DAQmxStopTask(m_taskAOCtr);
		if(m_taskDOCtr != TASK_UNDEF)
		    DAQmxStopTask(m_taskDOCtr);
		if(m_taskDO != TASK_UNDEF)
		    DAQmxStopTask(m_taskDO);
		if(m_taskAO != TASK_UNDEF)
		    DAQmxStopTask(m_taskAO);
		if(m_taskGateCtr != TASK_UNDEF)
		    DAQmxStopTask(m_taskGateCtr);
	}
}
inline XNIDAQmxPulser::tRawAO
XNIDAQmxPulser::aoVoltToRaw(int ch, float64 volt)
{
//	volt = std::max(volt, m_lowerLimAO[ch]);
//	volt = std::min(volt, m_upperLimAO[ch]);
	float64 x = 1.0;
	float64 y = 0.0;
	float64 *pco = m_coeffAO[ch];
	for(unsigned int i = 0; i < CAL_POLY_ORDER; i++) {
		y += (*pco++) * x;
		x *= volt;
	}
	return lrint(y);
}
void *
XNIDAQmxPulser::executeWriteAO(const atomic<bool> &terminated)
{
	XScopedLock<XMutex> lockAO(m_mutexAO);
	while(!terminated) {
		writeBufAO(terminated);
	}
	return NULL;
}
void *
XNIDAQmxPulser::executeWriteDO(const atomic<bool> &terminated)
{
	XScopedLock<XMutex> lockDO(m_mutexDO);
	while(!terminated) {
		writeBufDO(terminated);
	}
	return NULL;
}

void
XNIDAQmxPulser::writeBufAO(const atomic<bool> &terminated)
{
 	const double dma_ao_period = resolutionQAM();
	const unsigned int size = m_genBufAO.size() / NUM_AO_CH;
	bool firsttime = true;
	try {
		const unsigned int num_samps = 256;
		for(unsigned int cnt = 0; cnt < size;) {
			int32 samps;
			samps = std::min(size - cnt, num_samps);
			while(!terminated) {
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
XNIDAQmxPulser::writeBufDO(const atomic<bool> &terminated)
{
 	const double dma_do_period = resolution();
	const unsigned int size = m_genBufDO.size();
	bool firsttime = true;
	try {
		const unsigned int num_samps = 256;
		for(unsigned int cnt = 0; cnt < size;) {
			int32 samps;
			samps = std::min(size - cnt, num_samps);
			while(!terminated) {
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
	const unsigned int pausing_cnt = PAUSING_CNT;
	const unsigned int pausing_cnt_blank = PAUSING_CNT_BLANK;
	
	GenPatternIterator it = m_genLastPatItDO;
	uint32_t pat = it->pattern;
	uint64_t tonext = m_genRestSampsDO;
	
	tRawDO pausingbit = m_pausingBit;

	bool paused = false;

	tRawDO *pDO = &m_genBufDO[0];
	const unsigned int size = m_bufSizeHintDO;
	for(unsigned int samps_rest = size; samps_rest;) {
		//pattern of digital lines.
		tRawDO patDO = PAT_DO_MASK & pat;
		//index for analog pulses.
		const unsigned int pidx = (pat & PAT_QAM_PULSE_IDX_MASK) / PAT_QAM_PULSE_IDX;
		//number of samples to be written into buffer.
		unsigned int gen_cnt = std::min((uint64_t)samps_rest, tonext);
		
		if(pausingbit) {
			patDO &= ~pausingbit;
			if(paused) {
				ASSERT(gen_cnt >= pausing_cnt_blank + 1);
				//generate a blank time after pausing.
				gen_cnt = pausing_cnt_blank + 1;
				paused = false;
			}
			else {
				if((pidx == 0) &&
					 (tonext > pausing_cnt + pausing_cnt_blank + 1) &&
					 (samps_rest > pausing_cnt_blank + 1)) {
					//generate a pausing trigger.
					gen_cnt = 1;
					tonext -= pausing_cnt;
					patDO |= pausingbit;
					samps_rest = std::max((int)samps_rest - (int)pausing_cnt, (int)pausing_cnt_blank + 2);
					paused = true;
				}
			}
		}
		//write digital pattern.
		for(unsigned int cnt = 0; cnt < gen_cnt; cnt++) {
			*pDO++ = patDO;
		}
		tonext -= gen_cnt;
		samps_rest -= gen_cnt;
		ASSERT(samps_rest < size);
		if(tonext == 0) {
			it++;
			if(it == m_genPatternList.end()) {
				it = m_genPatternList.begin();
//				printf("p.\n");
			}
			pat = it->pattern;
			tonext = it->tonext;
		}
	}
	//resize buffers if necessary.
	if(pausingbit)
		m_genBufDO.resize((int)(pDO - &m_genBufDO[0]));
	else
		ASSERT(pDO == &m_genBufDO[m_genBufDO.size()]);
	m_genRestSampsDO = tonext;
	m_genLastPatItDO = it;
}
void
XNIDAQmxPulser::genBankAO()
{
	const unsigned int oversamp_ao = lrint(resolution() / resolutionQAM());
	const unsigned int pausing_cnt = PAUSING_CNT;
	const unsigned int pausing_cnt_blank = PAUSING_CNT_BLANK;
	
	GenPatternIterator it = m_genLastPatItAO;
	uint32_t pat = it->pattern;
	uint64_t tonext = m_genRestSampsAO;
	unsigned int aoidx = m_genAOIndex;
	
	const tRawDO pausingbit = m_pausingBit;
	const tRawDO ctrtrigbit = m_ctrTrigBit;

	unsigned int finiteaorest = m_genFiniteAORestSamps;
	const unsigned int finiteaosamps = m_genFiniteAOSamps;
	bool paused = false;

	tRawAO *pAO = &m_genBufAO[0];
	const tRawAO raw_ao0_zero = aoVoltToRaw(0, 0.0);
	const tRawAO raw_ao1_zero = aoVoltToRaw(1, 0.0);
	const unsigned int size = m_bufSizeHintAO;
	for(unsigned int samps_rest = size; samps_rest;) {
		//index for analog pulses.
		const unsigned int pidx = (pat & PAT_QAM_PULSE_IDX_MASK) / PAT_QAM_PULSE_IDX;
		//number of samples to be written into buffer.
		unsigned int gen_cnt = std::min((uint64_t)samps_rest, tonext);
		
		if(pausingbit) {
			if(paused) {
				ASSERT(gen_cnt >= pausing_cnt_blank + 1);
				//generate a blank time after pausing.
				gen_cnt = pausing_cnt_blank + 1;
				paused = false;
			}
			else {
				if((pidx == 0) &&
					 (tonext > pausing_cnt + pausing_cnt_blank + 1) &&
					 (samps_rest > pausing_cnt_blank + 1)) {
					//generate a pausing trigger.
					gen_cnt = 1;
					tonext -= pausing_cnt;
					samps_rest = std::max((int)samps_rest - (int)pausing_cnt, (int)pausing_cnt_blank + 2);
					paused = true;
				}
			}
		}
		if(USE_FINITE_AO)
		{
			if((pat & ctrtrigbit) && (finiteaorest == 0)) {
				finiteaorest = finiteaosamps;
			}
		}
		if(pidx == 0) {
			//write blank in analog lines.
			aoidx = 0;
			unsigned int zerocnt = gen_cnt;
			if(USE_FINITE_AO) {
				zerocnt = std::min(finiteaorest, gen_cnt);
				finiteaorest -= zerocnt;
			}
			zerocnt *= oversamp_ao;
			for(unsigned int cnt = 0; cnt < zerocnt; cnt++) {
				*pAO++ = raw_ao0_zero;
				*pAO++ = raw_ao1_zero;
			}
		}
		else {
			if(USE_FINITE_AO) {
				finiteaorest -= gen_cnt;
			}
			const unsigned int qpskidx = (pat & PAT_QAM_PHASE_MASK) / PAT_QAM_PHASE;
			ASSERT(qpskidx < 4);
			const unsigned int pnum = (pidx - 1) * (PAT_QAM_PULSE_IDX/PAT_QAM_PHASE) + qpskidx;
			ASSERT(pnum < PAT_QAM_PULSE_IDX_MASK/PAT_QAM_PULSE_IDX);
			tRawAO *pGenAO0 = &m_genPulseWaveAO[0][pnum][aoidx];
			tRawAO *pGenAO1 = &m_genPulseWaveAO[1][pnum][aoidx];
			ASSERT(m_genPulseWaveAO[0][pnum].size());
			ASSERT(m_genPulseWaveAO[1][pnum].size());
			if(m_genPulseWaveAO[0][pnum].size() <= aoidx + gen_cnt * oversamp_ao) {
				fprintf(stderr, "Oops. This should not happen.\n");
				int lps = m_genPulseWaveAO[0][pnum].size() - aoidx;
				lps = std::max(0, lps);
				for(int cnt = 0; cnt < lps; cnt++) {
					*pAO++ = *pGenAO0++;
					*pAO++ = *pGenAO1++;
				}
				for(unsigned int cnt = 0; cnt < gen_cnt * oversamp_ao - lps; cnt++) {
					*pAO++ = raw_ao0_zero;
					*pAO++ = raw_ao1_zero;
				}
			}
			else {
				for(unsigned int cnt = 0; cnt < gen_cnt * oversamp_ao; cnt++) {
					*pAO++ = *pGenAO0++;
					*pAO++ = *pGenAO1++;
				}
				aoidx += gen_cnt * oversamp_ao;
			}
		}
		tonext -= gen_cnt;
		samps_rest -= gen_cnt;
		ASSERT(samps_rest < size);
		if(tonext == 0) {
			it++;
			if(it == m_genPatternList.end()) {
				it = m_genPatternList.begin();
//				printf("p.\n");
			}
			pat = it->pattern;
			tonext = it->tonext;
		}
	}
	//resize buffers if necessary.
	if(USE_FINITE_AO || pausingbit)
		m_genBufAO.resize((int)(pAO - &m_genBufAO[0]));
	else
		ASSERT(pAO == &m_genBufAO[m_genBufAO.size()]);
	m_genRestSampsAO = tonext;
	m_genLastPatItAO = it;
	m_genAOIndex = aoidx;
	m_genFiniteAORestSamps = finiteaorest;
}

void
XNIDAQmxPulser::createNativePatterns()
{
  const double _master = *masterLevel();
  const double _tau = m_tauRecorded;
  const double _pw1 = m_pw1Recorded;
  const double _pw2 = m_pw2Recorded;
  const double _comb_pw = m_combPWRecorded;
  const double _dif_freq = m_difFreqRecorded;

  const bool _induce_emission = *induceEmission();
  const double _induce_emission_pw = _comb_pw;
  const double _induce_emission_phase = *induceEmissionPhase() / 180.0 * PI;
      
  m_genPatternList.clear();
  uint32_t lastpat = m_relPatList.back().pattern;
  for(RelPatListIterator it = m_relPatList.begin(); it != m_relPatList.end(); it++)
  {
	 	GenPattern pat(lastpat, it->toappear);
	 	lastpat = it->pattern;
  		m_genPatternList.push_back(pat);
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
  	
	  makeWaveForm(PAT_QAM_PULSE_IDX_P1/PAT_QAM_PULSE_IDX - 1, _pw1/1000.0, pulseFunc(p1Func()->to_str() ),
	  	 _master + *p1Level()
	    , _dif_freq * 1000.0, -2 * PI * _dif_freq * 2 * _tau);
	  makeWaveForm(PAT_QAM_PULSE_IDX_P2/PAT_QAM_PULSE_IDX - 1, _pw2/1000.0, pulseFunc(p2Func()->to_str() ),
	  	_master + *p2Level()
	    , _dif_freq * 1000.0, -2 * PI * _dif_freq * 2 * _tau);
	  makeWaveForm(PAT_QAM_PULSE_IDX_PCOMB/PAT_QAM_PULSE_IDX - 1, _comb_pw/1000.0, pulseFunc(combFunc()->to_str() ),
	         _master + *combLevel(), *combOffRes() + _dif_freq *1000.0);
	  if(_induce_emission) {
	      makeWaveForm(PAT_QAM_PULSE_IDX_INDUCE_EMISSION/PAT_QAM_PULSE_IDX - 1, _induce_emission_pw/1000.0, pulseFunc(combFunc()->to_str() ),
	         _master + *combLevel(), *combOffRes() + _dif_freq *1000.0, _induce_emission_phase);
	  }

  }
}

int
XNIDAQmxPulser::makeWaveForm(int num, double pw, tpulsefunc func, double dB, double freq, double phase)
{
	const double dma_ao_period = resolutionQAM();
	const double delay1 = *qamDelay1() * 1e-3 / dma_ao_period;
	const double delay2 = *qamDelay2() * 1e-3 / dma_ao_period;
	for(unsigned int qpsk = 0; qpsk < 4; qpsk++) {
		const unsigned int pnum = num * (PAT_QAM_PULSE_IDX/PAT_QAM_PHASE) + qpsk;
		m_genPulseWaveAO[0][pnum].clear();
		m_genPulseWaveAO[1][pnum].clear();
		ASSERT(pnum < PAT_QAM_PULSE_IDX_MASK/PAT_QAM_PULSE_IDX);
	  	unsigned short word = (unsigned short)lrint(pw / dma_ao_period);
		double dx = dma_ao_period / pw;
		double dp = 2*PI*freq*dma_ao_period;
		double z = pow(10.0, dB/20.0);
		for(int i = 0; i < word + 2; i++) {
			double i1 = i - word / 2.0 - delay1;
			double i2 = i - word / 2.0 - delay2;
			double x = z * func(i1 * dx) * cos(i1 * dp + PI/4 + phase);
			double y = z * func(i2 * dx) * sin(i2 * dp + PI/4 + phase);
			m_genPulseWaveAO[0][pnum].push_back(aoVoltToRaw(0, x));
			m_genPulseWaveAO[1][pnum].push_back(aoVoltToRaw(1, y));
		}
		phase += PI/2;
	}
	return 0;
}

void
XNIDAQmxPulser::changeOutput(bool output, unsigned int /*blankpattern*/)
{
  if(output)
    {
      if(m_genPatternList.empty() )
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
