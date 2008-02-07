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
//---------------------------------------------------------------------------

#include "userdmm.h"
#include "charinterface.h"
//---------------------------------------------------------------------------

REGISTER_TYPE(XDriverList, KE2000, "Keithley 2000/2001 DMM");
REGISTER_TYPE(XDriverList, KE2182, "Keithley 2182 nanovolt meter");
REGISTER_TYPE(XDriverList, HP34420A, "Agilent 34420A nanovolt meter");
REGISTER_TYPE(XDriverList, HP3458A, "Agilent 3458A DMM");
REGISTER_TYPE(XDriverList, HP3478A, "Agilent 3478A DMM");
REGISTER_TYPE(XDriverList, SanwaPC500, "SANWA PC500/510/520M DMM");
REGISTER_TYPE(XDriverList, SanwaPC5000, "SANWA PC5000 DMM");

void
XDMMSCPI::changeFunction()
{
    std::string func = function()->to_str();
    if(!func.empty())
        interface()->sendf(":CONF:%s", func.c_str());
}
double
XDMMSCPI::fetch()
{
    interface()->query(":FETC?");
    return interface()->toDouble();
}
double
XDMMSCPI::oneShotRead()
{
    interface()->query(":READ?");
    return interface()->toDouble();
}
/*
double
XDMMSCPI::measure(const std::string &func)
{
    interface()->queryf(":MEAS:%s?", func.c_str());
    return interface()->toDouble();
}
*/

XHP3458A::XHP3458A(const char *name, bool runtime,
		 const shared_ptr<XScalarEntryList> &scalarentries,
		 const shared_ptr<XInterfaceList> &interfaces,
		 const shared_ptr<XThermometerList> &thermometers,
		 const shared_ptr<XDriverList> &drivers) :
	XCharDeviceDriver<XDMM>(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
	interface()->setGPIBMAVbit(0x80);
	interface()->setGPIBUseSerialPollOnWrite(false);
	const char *funcs[] = {
		"DCV", "ACV", "ACDCV", "OHM", "OHMF", "DCI", "ACI", "ACDCI", "FREQ", "PER", "DSAC", "DSDC", "SSAC", "SSDC", ""
	};
	for(const char **func = funcs; strlen(*func); func++) {
		function()->add(*func);
	}
}
void
XHP3458A::changeFunction()
{
    std::string func = function()->to_str();
    if(!func.empty())
        interface()->sendf("FUNC %s;ARANGE ON", func.c_str());
}
double
XHP3458A::fetch()
{
    interface()->receive();
    return interface()->toDouble();
}
double
XHP3458A::oneShotRead()
{
    interface()->query("END ALWAYS;OFORMAT ASCII;QFORMAT NUM;NRDGS 1;TRIG AUTO;TARM SGL");
    return interface()->toDouble();
}


XHP3478A::XHP3478A(const char *name, bool runtime,
		 const shared_ptr<XScalarEntryList> &scalarentries,
		 const shared_ptr<XInterfaceList> &interfaces,
		 const shared_ptr<XThermometerList> &thermometers,
		 const shared_ptr<XDriverList> &drivers) :
	XCharDeviceDriver<XDMM>(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
	interface()->setGPIBUseSerialPollOnWrite(false);
	interface()->setGPIBMAVbit(0x01);
//	setEOS("\r\n");
	const char *funcs[] = {
		"DCV", "ACV", "OHM", "OHMF", "DCI", "ACI", ""
	};
	for(const char **func = funcs; strlen(*func); func++) {
		function()->add(*func);
	}
}
void
XHP3478A::changeFunction()
{
    int func = *function();
    if(func < 0)
		return;
//    		throw XInterface::XInterfaceError(KAME::i18n("Select function!"), __FILE__, __LINE__);
    interface()->sendf("F%dRAZ1", func + 1);
}
double
XHP3478A::fetch()
{
    interface()->receive();
    return interface()->toDouble();
}
double
XHP3478A::oneShotRead()
{
    interface()->query("T3");
    return interface()->toDouble();
}

XSanwaPC500::XSanwaPC500(const char *name, bool runtime,
		 const shared_ptr<XScalarEntryList> &scalarentries,
		 const shared_ptr<XInterfaceList> &interfaces,
		 const shared_ptr<XThermometerList> &thermometers,
		 const shared_ptr<XDriverList> &drivers) :
	XCharDeviceDriver<XDMM>(name, runtime, scalarentries, interfaces, thermometers, drivers)
{
	const char *funcs[] = {
		"AcV", "DcV", "Ac+DcV", "Cx", "Dx", "TC", "TF", "Ohm",
		"Conti", "AcA", "DcA", "Ac+DcA", "Hz", "Duty%", "%mA", "dB", ""
	};
	for(const char **func = funcs; strlen(*func); func++) {
		function()->add(*func);
	}
	function()->setUIEnabled(false);
}
void
XSanwaPC500::changeFunction() {
}
double
XSanwaPC500::fetch() {
	requestData();
	msecsleep(200);
	interface()->receive(8);
	if((interface()->buffer()[0] != 0x10) ||
		(interface()->buffer()[1] != 0x02))
		throw XInterface::XInterfaceError(KAME::i18n("Format Error!"), __FILE__, __LINE__);
	if((interface()->buffer()[6] != 0x00) ||
		(interface()->buffer()[7] != 0x00))
		throw XInterface::XInterfaceError(KAME::i18n("Format Error!"), __FILE__, __LINE__);
	const int funcs[] = {0x05, 0x06, 0x07, 0x08, 0x04, 0x14, 0x00, 0x20, 0x40, 0x80,
		0x180, 0x201, 0x202, 0x203, 0x400, 0x800, 0x802, 0x2000
	};
	int f = (int)interface()->buffer()[4] + (int)interface()->buffer()[5] * 256u;
	for(int i = 0; i < sizeof(funcs)/sizeof(int); i++) {
		if(funcs[i] == f) {
			function()->value(i);
		}
	}

	int dlen = interface()->buffer()[3] - 1;
	interface()->receive(dlen);
	std::vector<char> buf[dlen];
	std::copy(interface()->buffer().begin(), interface()->buffer().end, buf);
	buf[dlen - 3] = '\0';
	if(std::string(&buf[0]) == "+OL") {
		return 1e99;
	}
	if(std::string(&buf[0]) == "-OL") {
		return -1e99;
	}
	double x;
	if(sscanf(&buf[0], "%lf", &x) != 1) {
		throw XInterface::XInterfaceError(KAME::i18n("Format Error!"), __FILE__, __LINE__);
	}
	return x;
}
double
XSanwaPC500::oneShotRead() {
	fetch();
}
void
XSanwaPC500::requestData() {
	char bytes[8] = {0x10, 0x02, 0x42, 0x00, 0x00, 0x00, 0x10, 0x03};
	interface()->write(bytes, sizeof(bytes));
}
void
XSanwaPC5000::requestData() {
	char bytes[8] = {0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x03};
	interface()->write(bytes, sizeof(bytes));
}
