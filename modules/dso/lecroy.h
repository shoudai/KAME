/***************************************************************************
		Copyright (C) 2002-2008 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef lecroyH
#define lecroyH

#include "dso.h"
#include "chardevicedriver.h"
//---------------------------------------------------------------------------

//! Lecroy/Iwatsu DSO
class XLecroyDSO : public XCharDeviceDriver<XDSO>
{
	XNODE_OBJECT
protected:
	XLecroyDSO(const char *name, bool runtime,
		 const shared_ptr<XScalarEntryList> &scalarentries,
		 const shared_ptr<XInterfaceList> &interfaces,
		 const shared_ptr<XThermometerList> &thermometers,
		 const shared_ptr<XDriverList> &drivers);
	~XLecroyDSO() {}
	//! convert raw to record
	virtual void convertRaw() throw (XRecordError&);
protected:
	virtual void onTrace1Changed(const shared_ptr<XValueNodeBase> &);
	virtual void onTrace2Changed(const shared_ptr<XValueNodeBase> &);
	virtual void onAverageChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onSingleChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onTrigSourceChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onTrigPosChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onTrigLevelChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onTrigFallingChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onTimeWidthChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onVFullScale1Changed(const shared_ptr<XValueNodeBase> &);
	virtual void onVFullScale2Changed(const shared_ptr<XValueNodeBase> &);
	virtual void onVOffset1Changed(const shared_ptr<XValueNodeBase> &);
	virtual void onVOffset2Changed(const shared_ptr<XValueNodeBase> &);
	virtual void onRecordLengthChanged(const shared_ptr<XValueNodeBase> &);
	virtual void onForceTriggerTouched(const shared_ptr<XNode> &);

	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
  
	virtual double getTimeInterval();
	//! clear count or start sequence measurement
	virtual void startSequence();
	virtual int acqCount(bool *seq_busy);

	//! load waveform and settings from instrument
	virtual void getWave(std::deque<std::string> &channels);
private:
	double inspectDouble(const char *req, const std::string &trace);
	int m_totalCount;
};

#endif