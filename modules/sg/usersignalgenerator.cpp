/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include <klocale.h>

#include "analyzer.h"
#include "charinterface.h"
#include "usersignalgenerator.h"

REGISTER_TYPE(XDriverList, SG7130, "KENWOOD SG7130 signal generator");
REGISTER_TYPE(XDriverList, SG7200, "KENWOOD SG7200 signal generator");
REGISTER_TYPE(XDriverList, HP8643, "HP/Agilent 8643/8644 signal generator");
REGISTER_TYPE(XDriverList, HP8648, "HP/Agilent 8648 signal generator");
REGISTER_TYPE(XDriverList, HP8664, "HP/Agilent 8664/8665 signal generator");

XSG7200::XSG7200(const char *name, bool runtime,
				 const shared_ptr<XScalarEntryList> &scalarentries,
				 const shared_ptr<XInterfaceList> &interfaces,
				 const shared_ptr<XThermometerList> &thermometers,
				 const shared_ptr<XDriverList> &drivers)
    : XCharDeviceDriver<XSG>(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
	interface()->setGPIBUseSerialPollOnWrite(false);
	interface()->setGPIBUseSerialPollOnRead(false);
}
XSG7130::XSG7130(const char *name, bool runtime,
				 const shared_ptr<XScalarEntryList> &scalarentries,
				 const shared_ptr<XInterfaceList> &interfaces,
				 const shared_ptr<XThermometerList> &thermometers,
				 const shared_ptr<XDriverList> &drivers)
    : XSG7200(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
}
void
XSG7200::changeFreq(double mhz) {
	interface()->sendf("FR%fMHZ", mhz);
	msecsleep(50); //wait stabilization of PLL
}
void
XSG7200::onOLevelChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("LE%fDBM", (double)*oLevel());
}
void
XSG7200::onFMONChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->send(*fmON() ? "FMON" : "FMOFF");
}
void
XSG7200::onAMONChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->send(*amON() ? "AMON" : "AMOFF");
}

XHP8643::XHP8643(const char *name, bool runtime,
				 const shared_ptr<XScalarEntryList> &scalarentries,
				 const shared_ptr<XInterfaceList> &interfaces,
				 const shared_ptr<XThermometerList> &thermometers,
				 const shared_ptr<XDriverList> &drivers)
    : XCharDeviceDriver<XSG>(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
}
void
XHP8643::changeFreq(double mhz) {
	interface()->sendf("FREQ:CW %f MHZ", mhz);
	msecsleep(50); //wait stabilization of PLL
}
void
XHP8643::onOLevelChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("AMPL:LEV %f DBM", (double)*oLevel());
}
void
XHP8643::onFMONChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("FMSTAT %s", *fmON() ? "ON" : "OFF");
}
void
XHP8643::onAMONChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("AMSTAT %s", *amON() ? "ON" : "OFF");
}

XHP8648::XHP8648(const char *name, bool runtime,
				 const shared_ptr<XScalarEntryList> &scalarentries,
				 const shared_ptr<XInterfaceList> &interfaces,
				 const shared_ptr<XThermometerList> &thermometers,
				 const shared_ptr<XDriverList> &drivers)
    : XHP8643(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
}
void
XHP8648::onOLevelChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("POW:AMPL %f DBM", (double)*oLevel());
}

XHP8664::XHP8664(const char *name, bool runtime,
				 const shared_ptr<XScalarEntryList> &scalarentries,
				 const shared_ptr<XInterfaceList> &interfaces,
				 const shared_ptr<XThermometerList> &thermometers,
				 const shared_ptr<XDriverList> &drivers)
    : XCharDeviceDriver<XSG>(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
}
void
XHP8664::changeFreq(double mhz) {
	interface()->sendf("FREQ:CW %f MHZ", mhz);
	msecsleep(50); //wait stabilization of PLL
}
void
XHP8664::onOLevelChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("AMPL %f DBM", (double)*oLevel());
}
void
XHP8664::onFMONChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("FM:STAT %s", *fmON() ? "ON" : "OFF");
}
void
XHP8664::onAMONChanged(const shared_ptr<XValueNodeBase> &) {
	interface()->sendf("AM:STAT %s", *amON() ? "ON" : "OFF");
}