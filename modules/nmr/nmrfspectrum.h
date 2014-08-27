/***************************************************************************
		Copyright (C) 2002-2014 Kentaro Kitagawa
		                   kitag@kochi-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef nmrfspectrumH
#define nmrfspectrumH

#include "nmrspectrumbase.h"

class XSG;
class XPulser;
class XAutoLCTuner;
class QMainWindow;
class Ui_FrmNMRFSpectrum;
typedef QForm<QMainWindow, Ui_FrmNMRFSpectrum> FrmNMRFSpectrum;

class XNMRFSpectrum : public XNMRSpectrumBase<FrmNMRFSpectrum> {
public:
	XNMRFSpectrum(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	//! ususally nothing to do
	~XNMRFSpectrum() {}
protected:
	//! \return true to be cleared.
	virtual bool onCondChangedImpl(const Snapshot &shot, XValueNodeBase *) const;
	virtual double getFreqResHint(const Snapshot &shot_this) const;
	virtual double getMinFreq(const Snapshot &shot_this) const;
	virtual double getMaxFreq(const Snapshot &shot_this) const;
	virtual double getCurrentCenterFreq(const Snapshot &shot_this, const Snapshot &shot_others) const;
	virtual void getValues(const Snapshot &shot_this, std::vector<double> &values) const;

	virtual bool checkDependencyImpl(const Snapshot &shot_this,
		const Snapshot &shot_emitter, const Snapshot &shot_others,
		XDriver *emitter) const;

	virtual void rearrangeInstrum(const Snapshot &shot);
public:
	//! driver specific part below 
	const shared_ptr<XItemNode<XDriverList, XSG> > &sg1() const {return m_sg1;}
	const shared_ptr<XItemNode<XDriverList, XAutoLCTuner> > &autoTuner() const {return m_autoTuner;}
	const shared_ptr<XItemNode<XDriverList, XPulser> > &pulser() const {return m_pulser;}
	//! Offset for IF [MHz]
	const shared_ptr<XDoubleNode> &sg1FreqOffset() const {return m_sg1FreqOffset;}
	//! [MHz]
	const shared_ptr<XDoubleNode> &centerFreq() const {return m_centerFreq;}
	//! [kHz]
	const shared_ptr<XDoubleNode> &freqSpan() const {return m_freqSpan;}
	//! [kHz]
	const shared_ptr<XDoubleNode> &freqStep() const {return m_freqStep;}
	const shared_ptr<XBoolNode> &active() const {return m_active;}
	//! [MHz]
    const shared_ptr<XDoubleNode> &tuneStep() const {return m_tuneStep;}
    const shared_ptr<XComboNode> &tuneStrategy() const {return m_tuneStrategy;}
    enum TuneStrategy {TUNESTRATEGY_ASIS = 0, TUNESTRATEGY_AWAIT = 1, TUNESTRATEGY_AUTOTUNER = 2};
private:
	const shared_ptr<XItemNode<XDriverList, XSG> > m_sg1;
	const shared_ptr<XItemNode<XDriverList, XAutoLCTuner> > m_autoTuner;
	const shared_ptr<XItemNode<XDriverList, XPulser> > m_pulser;
	const shared_ptr<XDoubleNode> m_sg1FreqOffset;

	const shared_ptr<XDoubleNode> m_centerFreq;
	const shared_ptr<XDoubleNode> m_freqSpan;
	const shared_ptr<XDoubleNode> m_freqStep;
	const shared_ptr<XBoolNode> m_active;
    const shared_ptr<XDoubleNode> m_tuneStep;
    const shared_ptr<XComboNode> m_tuneStrategy;

	shared_ptr<XListener> m_lsnOnActiveChanged,
        m_lsnOnTuningStrategyChanged, m_lsnOnTuningChanged;
    
	xqcon_ptr m_conCenterFreq,
		m_conFreqSpan, m_conFreqStep;
	xqcon_ptr m_conActive, m_conSG1, m_conSG1FreqOffset,
        m_conTuneStep, m_conTuneStrategy, m_conAutoTuner, m_conPulser;

	void onActiveChanged(const Snapshot &shot, XValueNodeBase *);
	void onTuningChanged(const Snapshot &shot, XValueNodeBase *); //!< receives signals from AutoLCTuner.
    void performTuning(const Snapshot &shot_this, double newf);

    mutable double m_lastFreqAcquired; //!< to avoid inifite averaging after a sweep.
    double m_tunedFreq;
};


#endif
