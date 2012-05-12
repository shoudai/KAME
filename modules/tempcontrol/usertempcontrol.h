/***************************************************************************
		Copyright (C) 2002-2012 Kentaro Kitagawa
		                   kitag@kochi-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
//---------------------------------------------------------------------------

#ifndef usertempcontrolH
#define usertempcontrolH

#include "tempcontrol.h"
#include "oxforddriver.h"
#include "chardevicedriver.h"
//---------------------------------------------------------------------------
//! ITC503 Oxford
class XITC503 : public XOxfordDriver<XTempControl> {
public:
	XITC503(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XITC503() {}

protected:
	//! reads sensor value from the instrument
	virtual double getRaw(shared_ptr<XChannel> &channel);
	//! reads a value in Kelvin from the instrument
	virtual double getTemp(shared_ptr<XChannel> &channel);
	//! obtains current heater power
	//! \sa m_heaterPowerUnit()
	virtual double getHeater();
	//! ex. "W", "dB", or so
	virtual const char *m_heaterPowerUnit() {return "%";}
  
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
  
	virtual void onPChanged(double p);
	virtual void onIChanged(double i);
	virtual void onDChanged(double d);
	virtual void onTargetTempChanged(double temp);
	virtual void onManualPowerChanged(double pow);
	virtual void onHeaterModeChanged(int mode);
	virtual void onPowerRangeChanged(int range);
	virtual void onCurrentChannelChanged(const shared_ptr<XChannel> &ch);
	virtual void onExcitationChanged(const shared_ptr<XChannel> &ch, int exc);
private:
};

//! Picowatt/Oxford AVS47-IB
//! AVS47 and TS530A
class XAVS47IB:public XCharDeviceDriver<XTempControl> {
public:
	XAVS47IB(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XAVS47IB() {}

protected:
	//! reads sensor value from the instrument
	virtual double getRaw(shared_ptr<XChannel> &channel);
	//! reads a value in Kelvin from the instrument
	virtual double getTemp(shared_ptr<XChannel> &channel);
	//! obtains current heater power
	//! \sa m_heaterPowerUnit()
	virtual double getHeater();
	//! ex. "W", "dB", or so
	virtual const char *m_heaterPowerUnit() {return "W";}
  
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
	//! Be called for closing interfaces.
	virtual void afterStop();
  
	virtual void onPChanged(double p);
	virtual void onIChanged(double i);
	virtual void onDChanged(double d);
	virtual void onTargetTempChanged(double temp);
	virtual void onManualPowerChanged(double pow);
	virtual void onHeaterModeChanged(int mode);
	virtual void onPowerRangeChanged(int range);
	virtual void onCurrentChannelChanged(const shared_ptr<XChannel> &ch);
	virtual void onExcitationChanged(const shared_ptr<XChannel> &ch, int exc);
private:
	double read(const char *str);

	void setTemp(double temp);
	void setHeaterMode(int /*mode*/) {}
	int setPoint();
	//AVS-47 COMMANDS
	int setRange(unsigned int range);
	double getRes();
	int getRange();
	//TS-530 COMMANDS
	int setBias(unsigned int bias);
	void setPowerRange(int range);

	int m_autorange_wait;
};

//! Cryo-con base class
class XCryocon : public XCharDeviceDriver<XTempControl> {
public:
	XCryocon(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	virtual ~XCryocon() {}

protected:
	//! reads sensor value from the instrument
	virtual double getRaw(shared_ptr<XChannel> &channel);
	//! reads a value in Kelvin from the instrument
	virtual double getTemp(shared_ptr<XChannel> &channel);
	//! obtains current heater power
	//! \sa m_heaterPowerUnit()
	virtual double getHeater();
	//! ex. "W", "dB", or so
	virtual const char *m_heaterPowerUnit() {return "%";}
  
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
  
	virtual void onPChanged(double p);
	virtual void onIChanged(double i);
	virtual void onDChanged(double d);
	virtual void onTargetTempChanged(double temp);
	virtual void onManualPowerChanged(double pow);
	virtual void onHeaterModeChanged(int mode);
	virtual void onPowerRangeChanged(int range);
	virtual void onCurrentChannelChanged(const shared_ptr<XChannel> &ch);
	virtual void onExcitationChanged(const shared_ptr<XChannel> &ch, int exc);
private:
	void setTemp(double temp);
	//        void SetChannel(XChannel *channel);
	void setHeaterMode();
	void getChannel();
	int control();
	int stopControl();
	double getInput(shared_ptr<XChannel> &channel);
	int setHeaterSetPoint(double value);
};

//! Cryo-con Model 32 Cryogenic Inst.
class XCryoconM32:public XCryocon {
public:
	XCryoconM32(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XCryoconM32() {}

protected:
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
};

//! Cryo-con Model 62 Cryogenic Inst.
class XCryoconM62 : public XCryocon {
public:
	XCryoconM62(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XCryoconM62() {}

protected:
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
};

//! Neocera LTC-21.
class XNeoceraLTC21 : public XCharDeviceDriver<XTempControl> {
public:
	XNeoceraLTC21(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XNeoceraLTC21() {}

protected:
	//! reads sensor value from the instrument
	virtual double getRaw(shared_ptr<XChannel> &channel);
	//! reads a value in Kelvin from the instrument
	virtual double getTemp(shared_ptr<XChannel> &channel);
	//! obtains current heater power
	//! \sa m_heaterPowerUnit()
	virtual double getHeater();
	//! ex. "W", "dB", or so
	virtual const char *m_heaterPowerUnit() {return "%";}
  
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
    
	virtual void onPChanged(double p);
	virtual void onIChanged(double i);
	virtual void onDChanged(double d);
	virtual void onTargetTempChanged(double temp);
	virtual void onManualPowerChanged(double pow);
	virtual void onHeaterModeChanged(int mode);
	virtual void onPowerRangeChanged(int range);
	virtual void onCurrentChannelChanged(const shared_ptr<XChannel> &ch);
	virtual void onExcitationChanged(const shared_ptr<XChannel> &ch, int exc);
private:
	//! set the system into the control mode.
	void control();
	//! leave the control mode.
	void monitor();
	//! set PID, manual power.
	void setHeater();
};

//! LakeShore 340
class XLakeShore340 : public XCharDeviceDriver<XTempControl> {
public:
	XLakeShore340(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XLakeShore340() {}

protected:
	//! reads sensor value from the instrument
	virtual double getRaw(shared_ptr<XChannel> &channel);
	//! reads a value in Kelvin from the instrument
	virtual double getTemp(shared_ptr<XChannel> &channel);
	//! obtains current heater power
	//! \sa m_heaterPowerUnit()
	virtual double getHeater();
	//! ex. "W", "dB", or so
	virtual const char *m_heaterPowerUnit() {return "%";}
  
	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);
    
	virtual void onPChanged(double p);
	virtual void onIChanged(double i);
	virtual void onDChanged(double d);
	virtual void onTargetTempChanged(double temp);
	virtual void onManualPowerChanged(double pow);
	virtual void onHeaterModeChanged(int mode);
	virtual void onPowerRangeChanged(int range);
	virtual void onCurrentChannelChanged(const shared_ptr<XChannel> &ch);
	virtual void onExcitationChanged(const shared_ptr<XChannel> &ch, int exc);
private:
};

//! Keithley Integra 2700 w/ 7700 switching module.
class XKE2700w7700 : public XTempControl {
public:
	XKE2700w7700(const char *name, bool runtime,
		Transaction &tr_meas, const shared_ptr<XMeasure> &meas);
	~XKE2700w7700() {}

protected:
	//! reads sensor value from the instrument
	virtual double getRaw(shared_ptr<XChannel> &channel);
	//! reads a value in Kelvin from the instrument
	virtual double getTemp(shared_ptr<XChannel> &channel);
	//! obtains current heater power
	//! \sa m_heaterPowerUnit()
	virtual double getHeater();
	//! ex. "W", "dB", or so
	virtual const char *m_heaterPowerUnit() {return "%";}

	//! Be called just after opening interface. Call start() inside this routine appropriately.
	virtual void open() throw (XInterface::XInterfaceError &);

	virtual void onPChanged(double p);
	virtual void onIChanged(double i);
	virtual void onDChanged(double d);
	virtual void onTargetTempChanged(double temp);
	virtual void onManualPowerChanged(double pow);
	virtual void onHeaterModeChanged(int mode);
	virtual void onPowerRangeChanged(int range);
	virtual void onCurrentChannelChanged(const shared_ptr<XChannel> &ch);
	virtual void onExcitationChanged(const shared_ptr<XChannel> &ch, int exc);
private:
};
#endif
