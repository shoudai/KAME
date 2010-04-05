/***************************************************************************
		Copyright (C) 2002-2010 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef dmmH
#define dmmH
//---------------------------------------------------------------------------
#include <primarydriver.h>
#include <xnodeconnector.h>

class XScalarEntry;
class QMainWindow;
class Ui_FrmDMM;
typedef QForm<QMainWindow, Ui_FrmDMM> FrmDMM;

class XDMM : public XPrimaryDriver {
public:
	XDMM(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);

	//! usually nothing to do
	virtual ~XDMM() {}
	//! Shows all forms belonging to driver
	virtual void showForms();
	
	struct Payload : public XPrimaryDriver::Payload {
		double value() const {return m_var;}
		void _write(double var) {m_var = var;}
	private:
		double m_var;
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
  
	//! driver specific part below
	const shared_ptr<XComboNode> &function() const {return m_function;}
	const shared_ptr<XUIntNode> &waitInms() const {return m_waitInms;}
protected:
	//! one-shot reading
	virtual double oneShotRead() = 0; 
	//! is called when m_function is changed
	virtual void changeFunction() = 0;
  
	//! This should not cause an exception.
	virtual void afterStop() = 0;
private:
	//! is called when m_function is changed
	void onFunctionChanged(const shared_ptr<XValueNodeBase> &node);
  
	const shared_ptr<XScalarEntry> m_entry;
	const shared_ptr<XComboNode> m_function;
	const shared_ptr<XUIntNode> m_waitInms;
	shared_ptr<XListener> m_lsnOnFunctionChanged;
	xqcon_ptr m_conFunction, m_conWaitInms;
 
	shared_ptr<XThread<XDMM> > m_thread;
	const qshared_ptr<FrmDMM> m_form;
  
	void *execute(const atomic<bool> &);
  
};

#endif
