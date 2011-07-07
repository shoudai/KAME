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
//---------------------------------------------------------------------------
#ifndef usermagnetpsH
#define usermagnetpsH

#include "magnetps.h"
#include "oxforddriver.h"
//---------------------------------------------------------------------------
//OXFORD PS120 Magnet Power Supply
class XPS120 : public XOxfordDriver<XMagnetPS> {
public:
	XPS120(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	virtual ~XPS120() {}

protected:
	virtual void toNonPersistent();
	virtual void toPersistent();
	virtual void toZero();
	virtual void toSetPoint();
	virtual double getTargetField();
	virtual double getSweepRate();
	virtual double getOutputField();
	virtual double getMagnetField();
	virtual double getPersistentField();
	virtual double getOutputVolt();
	virtual double getOutputCurrent();

	//! Persistent Current Switch Heater
	//! please return *TRUE* if no PCS fitted
	virtual bool isPCSHeaterOn();
	//! please return false if no PCS fitted
	virtual bool isPCSFitted();
  
	virtual double fieldResolution() {return 0.001;}
  
	virtual void setPoint(double field);
	virtual void setRate(double hpm);
private:
	virtual double currentResolution() {return 0.01;}
	virtual double voltageResolution() {return 0.01;}
    
	void setPCSHeater(bool val) throw (XInterface::XInterfaceError&);
	void setActivity(int val) throw (XInterface::XInterfaceError&);
};

//OXFORD IPS120 Magnet Power Supply
class XIPS120 : public XPS120 {
public:
	XIPS120(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
		XPS120(name, runtime, ref(tr_meas), meas) {}
	virtual ~XIPS120() {}
	virtual double fieldResolution() {return 0.0001;}
protected:
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);

	virtual double currentResolution() {return 0.001;}
	virtual double voltageResolution() {return 0.001;}
	virtual double getTargetField();
	virtual double getSweepRate();
	virtual double getOutputField();
	virtual double getPersistentField();
	virtual double getOutputVolt();
	virtual double getOutputCurrent();
	virtual void setPoint(double field);
	virtual void setRate(double hpm);
};

#endif

