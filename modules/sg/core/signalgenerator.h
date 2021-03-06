/***************************************************************************
		Copyright (C) 2002-2015 Kentaro Kitagawa
		                   kitagawa@phys.s.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef signalgeneratorH
#define signalgeneratorH

#include "primarydriver.h"
#include "xnodeconnector.h"

class Ui_FrmSG;
typedef QForm<QMainWindow, Ui_FrmSG> FrmSG;

class DECLSPEC_SHARED XSG : public XPrimaryDriver {
public:
	XSG(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	//! usually nothing to do
	virtual ~XSG() {}
	//! show all forms belonging to driver
	virtual void showForms();

	struct Payload : public XPrimaryDriver::Payload {
		double freq() const {return m_freq;}
	private:
		friend class XSG;
		double m_freq;
	};
protected:
	//! Starts up your threads, connects GUI, and activates signals.
	virtual void start();
	//! Shuts down your threads, unconnects GUI, and deactivates signals
	//! This function may be called even if driver has already stopped.
	virtual void stop();
  
	//! This function will be called when raw data are written.
	//! Implement this function to convert the raw data to the record (Payload).
	//! \sa analyze()
	virtual void analyzeRaw(RawDataReader &reader, Transaction &tr) throw (XRecordError&);
	//! This function is called after committing XPrimaryDriver::analyzeRaw() or XSecondaryDriver::analyze().
	//! This might be called even if the record is invalid (time() == false).
	virtual void visualize(const Snapshot &shot);
public:
	//! driver specific part below
	const shared_ptr<XBoolNode> &rfON() const {return m_rfON;} //!< Activate Output
	const shared_ptr<XDoubleNode> &freq() const {return m_freq;} //!< freq [MHz]
	const shared_ptr<XDoubleNode> &oLevel() const {return m_oLevel;} //!< Output Level [dBm]
	const shared_ptr<XBoolNode> &fmON() const {return m_fmON;} //!< Activate FM
	const shared_ptr<XBoolNode> &amON() const {return m_amON;} //!< Activate AM
protected:
	virtual void changeFreq(double mhz) = 0;
	virtual void onRFONChanged(const Snapshot &shot, XValueNodeBase *) = 0;
	virtual void onOLevelChanged(const Snapshot &shot, XValueNodeBase *) = 0;
	virtual void onFMONChanged(const Snapshot &shot, XValueNodeBase *) = 0;
	virtual void onAMONChanged(const Snapshot &shot, XValueNodeBase *) = 0;
    virtual void onFreqChanged(const Snapshot &shot, XValueNodeBase *);
private:

	const shared_ptr<XBoolNode> m_rfON;
	const shared_ptr<XDoubleNode> m_freq;
	const shared_ptr<XDoubleNode> m_oLevel;
	const shared_ptr<XBoolNode> m_fmON;
	const shared_ptr<XBoolNode> m_amON;
  
    std::deque<xqcon_ptr> m_conUIs;
    shared_ptr<Listener> m_lsnRFON, m_lsnFreq, m_lsnOLevel, m_lsnFMON, m_lsnAMON;
  
	const qshared_ptr<FrmSG> m_form;
};//---------------------------------------------------------------------------
#endif
