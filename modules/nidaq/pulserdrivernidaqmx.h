/***************************************************************************
		Copyright (C) 2002-2011 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
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

#include <vector>

class XNIDAQmxPulser : public XNIDAQmxDriver<XPulser> {
public:
	XNIDAQmxPulser(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	virtual ~XNIDAQmxPulser();

    //! time resolution [ms]
    virtual double resolution() const {return m_resolutionDO;}

    struct Payload : public XNIDAQmxDriver<XPulser>::Payload {
    private:
    	friend class XNIDAQmxPulser;
    };
protected:
	virtual void open() throw (XInterface::XInterfaceError &) = 0;
	virtual void close() throw (XInterface::XInterfaceError &);
	
    double resolutionQAM() const {return m_resolutionAO;}
	//! \return Existense of AO ports.
    virtual bool haveQAMPorts() const = 0;

 	virtual const shared_ptr<XNIDAQmxInterface> &intfDO() const {return interface();}
	virtual const shared_ptr<XNIDAQmxInterface> &intfAO() const {return interface();} 
	virtual const shared_ptr<XNIDAQmxInterface> &intfCtr() const {return interface();} 
       	 
    //! Sends patterns to pulser or turns off.
    virtual void changeOutput(const Snapshot &shot, bool output, unsigned int blankpattern);
    //! Converts RelPatList to native patterns.
    virtual void createNativePatterns(Transaction &tr);

    //! \return Minimum period of pulses [ms]
    virtual double minPulseWidth() const {return resolution();}
    
	void openDO(bool use_ao_clock = false) throw (XInterface::XInterfaceError &);
	void openAODO() throw (XInterface::XInterfaceError &);
private:
	void startPulseGen(const Snapshot &shot) throw (XInterface::XInterfaceError &);
	void stopPulseGen();
	void abortPulseGen();
	
	void clearTasks();
	void setupTasksDO(bool use_ao_clock);
	void setupTasksAODO();

 	typedef int16 tRawAO;
	typedef uInt16 tRawDO;
	struct GenPattern {
		GenPattern(uint32_t pat, uint64_t next) :
	        pattern(pat), tonext(next) {}
		uint32_t pattern;
		uint64_t tonext; //!< in samps for buffer.
	};

	static int32 onTaskDone_(TaskHandle task, int32 status, void*);
	void onTaskDone(TaskHandle task, int32 status);

	scoped_ptr<std::vector<GenPattern> > m_genPatternList;
	scoped_ptr<std::vector<GenPattern> > m_genPatternListNext;
	typedef std::vector<GenPattern>::iterator GenPatternIterator;

	GenPatternIterator m_genLastPatItAO, m_genLastPatItDO;
	uint64_t m_genRestSampsAO, m_genRestSampsDO;
	enum { NUM_AO_CH = 2};
	unsigned int m_genAOIndex;
	shared_ptr<XNIDAQmxInterface::SoftwareTrigger> m_softwareTrigger;
	unsigned int m_pausingBit;
	unsigned int m_aswBit;
	unsigned int m_pausingCount;
	XString m_pausingCh;
	XString m_pausingSrcTerm;
	XString m_pausingGateTerm;
	unsigned int m_bufSizeHintDO;
	unsigned int m_bufSizeHintAO;
	unsigned int m_transferSizeHintDO;
	unsigned int m_transferSizeHintAO;
	uint64_t m_genTotalCountDO;
	bool m_running;
protected:	
	double m_resolutionDO, m_resolutionAO;
	TaskHandle m_taskAO, m_taskDO,
		m_taskDOCtr, m_taskGateCtr;
private:
	enum {PORTSEL_PAUSING = 16};

	enum {NUM_BUF_BANKS = 16};
	struct BufDO {
		void reserve(ssize_t s) {data.resize(s); clear(); }
		//! Sets length to be sent.
		void resize(ssize_t s) {m_size = s; ASSERT(s <= data.size());}
		void clear() {resize(0); }
		std::vector<tRawDO> data;
		ssize_t size() const {return m_size;}
		ssize_t capacity() const {return data.size();}
	private:
		atomic<ssize_t> m_size;
	} m_bufBanksDO[NUM_BUF_BANKS]; //!< Buffers containing generated patterns for DO.
	typedef struct {tRawAO ch[NUM_AO_CH];} tRawAOSet;
	struct BufAO {
		void reserve(ssize_t s) {data.resize(s); clear(); }
		//! Sets length to be sent.
		void resize(ssize_t s) {m_size = s; ASSERT(s <= data.size());}
		void clear() {resize(0); }
		std::vector<tRawAOSet> data;
		ssize_t size() const {return m_size;}
		ssize_t capacity() const {return data.size();}
	private:
		ssize_t m_size;
	} m_bufBanksAO[NUM_BUF_BANKS]; //!< Buffers containing generated patterns for AO.
	tRawAOSet m_genAOZeroLevel;
	scoped_ptr<std::vector<tRawAOSet> > m_genPulseWaveAO[PAT_QAM_MASK / PAT_QAM_PHASE];
	scoped_ptr<std::vector<tRawAOSet> > m_genPulseWaveNextAO[PAT_QAM_MASK / PAT_QAM_PHASE];
	enum { CAL_POLY_ORDER = 4};
	double m_coeffAO[NUM_AO_CH][CAL_POLY_ORDER];
	double m_coeffAODev[NUM_AO_CH][CAL_POLY_ORDER];
	double m_upperLimAO[NUM_AO_CH];
	double m_lowerLimAO[NUM_AO_CH];
	inline tRawAOSet aoVoltToRaw(const std::complex<double> &volt);

	shared_ptr<XThread<XNIDAQmxPulser> > m_threadWriteAO;
	shared_ptr<XThread<XNIDAQmxPulser> > m_threadWriteDO;
	bool m_isThreadWriteAOReady;
	bool m_isThreadWriteDOReady;
	void writeBufAO(const BufAO &buf, const atomic<bool> &terminating);
	void writeBufDO(const BufDO &buf, const atomic<bool> &terminating);
	void *executeWriteAO(const atomic<bool> &);
	void *executeWriteDO(const atomic<bool> &);
	void genBankDO(BufDO &buf);
	void genBankAO(BufAO &buf);
	void *executeGenBankAO(const atomic<bool> &);
	void *executeGenBankDO(const atomic<bool> &);

	int makeWaveForm(int num, double pw, tpulsefunc func, double dB, double freq = 0.0, double phase = 0.0);
	XRecursiveMutex m_totalLock;
};

#endif /*PULSERDRIVERNIDAQMX_H_*/
