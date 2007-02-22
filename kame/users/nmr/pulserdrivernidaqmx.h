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
#ifndef PULSERDRIVERNIDAQMX_H_
#define PULSERDRIVERNIDAQMX_H_

#include "pulserdriver.h"

#include "nidaqmxdriver.h"

#ifdef HAVE_NI_DAQMX

#include <vector>

class XNIDAQmxPulser : public XNIDAQmxDriver<XPulser>
{
 XNODE_OBJECT
 protected:
  XNIDAQmxPulser(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XNIDAQmxPulser();

 protected:
	virtual void open() throw (XInterface::XInterfaceError &) = 0;
	virtual void close() throw (XInterface::XInterfaceError &);
	
    //! time resolution [ms]
    virtual double resolution() const {return m_resolutionDO;}
    double resolutionQAM() const {return m_resolutionAO;}
     //! existense of AO ports.
    virtual bool haveQAMPorts() const = 0;

 	virtual const shared_ptr<XNIDAQmxInterface> &intfDO() const {return interface();}
	virtual const shared_ptr<XNIDAQmxInterface> &intfAO() const {return interface();} 
	virtual const shared_ptr<XNIDAQmxInterface> &intfCtr() const {return interface();} 
       	 
    //! send patterns to pulser or turn-off
    virtual void changeOutput(bool output, unsigned int blankpattern);
    //! convert RelPatList to native patterns
    virtual void createNativePatterns();

    //! minimum period of pulses [ms]
    virtual double minPulseWidth() const {return resolution();}
    
	void openDO(bool use_ao_clock = false) throw (XInterface::XInterfaceError &);
	void openAODO() throw (XInterface::XInterfaceError &);
	
	void setPausingGateTerm(const char*term);
private:
	void startPulseGen() throw (XInterface::XInterfaceError &);
	void stopPulseGen();

 	typedef int16 tRawAO;
	typedef uInt16 tRawDO;
	  struct GenPattern {
	      GenPattern(uint32_t pat, uint64_t next, uint64_t t) :
	        pattern(pat), tonext(next), time(t) {}
	      uint32_t pattern;
	      uint64_t tonext; //!< in samps for buffer.
	      uint64_t time; //!< timesamp for physical time.
	  };

	scoped_ptr<std::vector<GenPattern> > m_genPatternListAO;
	scoped_ptr<std::vector<GenPattern> > m_genPatternListDO;
	scoped_ptr<std::vector<GenPattern> > m_genPatternListNextAO;
	scoped_ptr<std::vector<GenPattern> > m_genPatternListNextDO;
	typedef std::vector<GenPattern>::iterator GenPatternIterator;

	GenPatternIterator m_genLastPatItAO, m_genLastPatItDO;
	uint64_t m_genRestSampsAO, m_genRestSampsDO;
enum { NUM_AO_CH = 2};
	tRawAO m_genAOZeroLevel[NUM_AO_CH];
	unsigned int m_genAOIndex;
	shared_ptr<XNIDAQmxInterface::VirtualTrigger> m_virtualTrigger;
	unsigned int m_pausingBit;
	unsigned int m_pausingCount;
	unsigned int m_pausingBlankBefore;
	unsigned int m_pausingBlankAfter;
	std::string m_pausingSrcTerm;
	std::string m_pausingGateTerm;
	unsigned int m_bufSizeHintDO;
	unsigned int m_bufSizeHintAO;
	unsigned int m_transferSizeHintDO;
	unsigned int m_transferSizeHintAO;
	atomic<bool> m_running;
	atomic<bool> m_suspendDO, m_suspendAO;
protected:	
	double m_resolutionDO, m_resolutionAO;
	TaskHandle m_taskAO, m_taskDO,
		 m_taskDOCtr, m_taskGateCtr;
private:
enum {PORTSEL_PAUSING = 16};
	std::vector<tRawDO> m_genBufDO;
	std::vector<tRawAO> m_genBufAO;
	scoped_ptr<std::vector<tRawAO> > m_genPulseWaveAO[NUM_AO_CH][PAT_QAM_PULSE_IDX_MASK / PAT_QAM_PULSE_IDX];
	scoped_ptr<std::vector<tRawAO> > m_genPulseWaveNextAO[NUM_AO_CH][PAT_QAM_PULSE_IDX_MASK / PAT_QAM_PULSE_IDX];
enum { CAL_POLY_ORDER = 4};
	float64 m_coeffAO[NUM_AO_CH][CAL_POLY_ORDER];
	float64 m_coeffAODev[NUM_AO_CH][CAL_POLY_ORDER];
	float64 m_upperLimAO[NUM_AO_CH];
	float64 m_lowerLimAO[NUM_AO_CH];
	
	inline tRawAO aoVoltToRaw(int ch, float64 volt);
	void genBankDO();
	void genBankAO();
	shared_ptr<XThread<XNIDAQmxPulser> > m_threadWriteAO;
	shared_ptr<XThread<XNIDAQmxPulser> > m_threadWriteDO;
	void writeBufAO(const atomic<bool> &terminated, const atomic<bool> &suspended);
	void writeBufDO(const atomic<bool> &terminated, const atomic<bool> &suspended);
	void *executeWriteAO(const atomic<bool> &);
	void *executeWriteDO(const atomic<bool> &);
	
  int makeWaveForm(int num, double pw, tpulsefunc func, double dB, double freq = 0.0, double phase = 0.0);
  XRecursiveMutex m_totalLock;
  XRecursiveMutex m_mutexAO, m_mutexDO;
  
  inline bool tryOutputSuspend(const atomic<bool> &flag,
  	 XRecursiveMutex &mutex, const atomic<bool> &terminated);
};

#endif //HAVE_NI_DAQMX

#endif /*PULSERDRIVERNIDAQMX_H_*/
