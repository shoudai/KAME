/***************************************************************************
		Copyright (C) 2002-2009 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
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

#include <nmrspectrumbase.h>

class XSG;
class QMainWindow;
class Ui_FrmNMRFSpectrum;
typedef QForm<QMainWindow, Ui_FrmNMRFSpectrum> FrmNMRFSpectrum;

class XNMRFSpectrum : public XNMRSpectrumBase<FrmNMRFSpectrum>
{
	XNODE_OBJECT
protected:
	XNMRFSpectrum(const char *name, bool runtime,
				  const shared_ptr<XScalarEntryList> &scalarentries,
				  const shared_ptr<XInterfaceList> &interfaces,
				  const shared_ptr<XThermometerList> &thermometers,
				  const shared_ptr<XDriverList> &drivers);
public:
	//! ususally nothing to do
	~XNMRFSpectrum() {}
protected:
	//! \return true to be cleared.
	virtual bool onCondChangedImpl(const shared_ptr<XValueNodeBase> &) const;
	virtual double getFreqResHint() const;
	virtual double getMinFreq() const;
	virtual double getMaxFreq() const;
	virtual double getCurrentCenterFreq() const;
	virtual void afterFSSum();
	virtual void getValues(std::vector<double> &values) const;

	virtual bool checkDependencyImpl(const shared_ptr<XDriver> &emitter) const;
public:
	//! driver specific part below 
	const shared_ptr<XItemNode<XDriverList, XSG> > &sg1() const {return m_sg1;}
 
	//! Offset for IF [MHz]
	const shared_ptr<XDoubleNode> &sg1FreqOffset() const {return m_sg1FreqOffset;}
	//! [MHz]
	const shared_ptr<XDoubleNode> &centerFreq() const {return m_centerFreq;}
	//! [kHz]
	const shared_ptr<XDoubleNode> &freqSpan() const {return m_freqSpan;}
	//! [kHz]
	const shared_ptr<XDoubleNode> &freqStep() const {return m_freqStep;}
	const shared_ptr<XBoolNode> &active() const {return m_active;}

	//! Burst mode stuff.
	//! Burst count.
	const shared_ptr<XUIntNode> &burstCount() const {return m_burstCount;}
	
private:
	const shared_ptr<XItemNode<XDriverList, XSG> > m_sg1;
	const shared_ptr<XDoubleNode> m_sg1FreqOffset;

	const shared_ptr<XDoubleNode> m_centerFreq;
	const shared_ptr<XDoubleNode> m_freqSpan;
	const shared_ptr<XDoubleNode> m_freqStep;
	const shared_ptr<XBoolNode> m_active;
	const shared_ptr<XNode> m_clear;
  
	const shared_ptr<XUIntNode> m_burstCount;
	
	unsigned int m_burstFreqCycleCount;
	unsigned int m_burstPhaseCycleCount;

	shared_ptr<XListener> m_lsnOnActiveChanged;
    
	xqcon_ptr m_conCenterFreq,
		m_conFreqSpan, m_conFreqStep;
	xqcon_ptr m_conActive, m_conSG1, m_conSG1FreqOffset, m_conBurstCount, m_conBurstFreqStep;

	void onActiveChanged(const shared_ptr<XValueNodeBase> &);
};


#endif
