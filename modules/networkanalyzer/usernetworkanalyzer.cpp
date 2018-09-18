/***************************************************************************
        Copyright (C) 2002-2018 Kentaro Kitagawa
		                   kitagawa@phys.s.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "usernetworkanalyzer.h"
#include "charinterface.h"
#include "analyzer.h"

REGISTER_TYPE(XDriverList, HP8711, "HP/Agilent 8711/8712/8713/8714 Network Analyzer");
REGISTER_TYPE(XDriverList, AgilentE5061, "Agilent E5061/E5062 Network Analyzer");
REGISTER_TYPE(XDriverList, VNWA3ENetworkAnalyzer, "DG8SAQ VNWA3E/Custom Network Analyzer");
REGISTER_TYPE(XDriverList, VNWA3ENetworkAnalyzerTCPIP, "DG8SAQ VNWA3E Network Analyzer TCP/IP");

//---------------------------------------------------------------------------
XAgilentNetworkAnalyzer::XAgilentNetworkAnalyzer(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
	XCharDeviceDriver<XNetworkAnalyzer>(name, runtime, ref(tr_meas), meas) {
    iterate_commit([=](Transaction &tr){
        tr[ *points()].add({"3", "5", "11", "21", "51", "101", "201", "401", "801", "1601"});
    });

	calOpen()->disable();
	calShort()->disable();
	calTerm()->disable();
	calThru()->disable();

    interface()->setGPIBWaitBeforeRead(10);
    interface()->setGPIBWaitBeforeWrite(10);
}

void
XAgilentNetworkAnalyzer::open() throw (XKameError &) {
	interface()->query("SENS:FREQ:START?");
	trans( *startFreq()) = interface()->toDouble() / 1e6;
	interface()->query("SENS:FREQ:STOP?");
	trans( *stopFreq()) = interface()->toDouble() / 1e6;
	interface()->query("SENS:AVER:STAT?");
	if(interface()->toUInt() == 0) {
		trans( *average()) = 1;
	}
	else {
		interface()->query("SENS:AVER:COUNT?");
		trans( *average()) = interface()->toUInt();
	}
	interface()->query("SENS:SWE:POIN?");
	trans( *points()).str(formatString("%u", interface()->toUInt()));
	interface()->send("SENS:SWE:TIME:AUTO OFF");
	interface()->query("SENS:SWE:TIME?");
	double swet = interface()->toDouble();
	interface()->sendf(":SENS:SWE:TIME %f S", std::min(1.0, std::max(0.3, swet)));
	interface()->send("ABOR;INIT:CONT OFF");
	
	start();
}
void 
XAgilentNetworkAnalyzer::onStartFreqChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("SENS:FREQ:START %f MHZ", (double)shot[ *startFreq()]);
}
void 
XAgilentNetworkAnalyzer::onStopFreqChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("SENS:FREQ:STOP %f MHZ", (double)shot[ *stopFreq()]);
}
void
XAgilentNetworkAnalyzer::onAverageChanged(const Snapshot &shot, XValueNodeBase *) {
	unsigned int avg = shot[ *average()];
	if(avg >= 2)
		interface()->sendf("SENS:AVER:CLEAR;STAT ON;COUNT %u", avg);
	else
		interface()->send("SENS:AVER:STAT OFF");
}
void
XAgilentNetworkAnalyzer::onPointsChanged(const Snapshot &shot, XValueNodeBase *) {	
	interface()->sendf("SENS:SWE:POIN %s", shot[ *points()].to_str().c_str());
}
void
XAgilentNetworkAnalyzer::getMarkerPos(unsigned int num, double &x, double &y) {
	XScopedLock<XInterface> lock( *interface());
	if(num >= 8)
		throw XDriver::XSkippedRecordError(__FILE__, __LINE__);
	interface()->queryf("CALC:MARK%u:STAT?", num + 1u);
	if(interface()->toInt() != 1)
		throw XDriver::XSkippedRecordError(__FILE__, __LINE__);		
	interface()->queryf("CALC:MARK%u:X?", num + 1u);
	x = interface()->toDouble() / 1e6;
	interface()->queryf("CALC:MARK%u:Y?", num + 1u);
	y = interface()->toDouble();
}
void
XAgilentNetworkAnalyzer::oneSweep() {
	interface()->query("INIT:IMM;*OPC?");
}
void
XAgilentNetworkAnalyzer::startContSweep() {
	interface()->send("INIT:CONT ON");
}
void
XAgilentNetworkAnalyzer::acquireTrace(shared_ptr<RawData> &writer, unsigned int ch) {
	XScopedLock<XInterface> lock( *interface());
	if(ch >= 2)
		throw XDriver::XSkippedRecordError(__FILE__, __LINE__);
	interface()->queryf("SENS%u:FREQ:START?", ch + 1u);
	double start = interface()->toDouble() / 1e6;
	writer->push(start);
	interface()->queryf("SENS%u:FREQ:STOP?", ch + 1u);
	double stop = interface()->toDouble() / 1e6;
	writer->push(stop);
	interface()->queryf("SENS%u:SWE:POIN?", ch + 1u);
	uint32_t len = interface()->toUInt();
	writer->push(len);
	acquireTraceData(ch, len);
	writer->insert(writer->end(),
					 interface()->buffer().begin(), interface()->buffer().end());
}
void
XAgilentNetworkAnalyzer::convertRaw(RawDataReader &reader, Transaction &tr) throw (XRecordError&) {
	double start = reader.pop<double>();
	double stop = reader.pop<double>();
	unsigned int samples = reader.pop<uint32_t>();
	tr[ *this].m_startFreq = start;
	char c = reader.pop<char>();
	if (c != '#') throw XBufferUnderflowRecordError(__FILE__, __LINE__);
	char buf[11];
	buf[0] = reader.pop<char>();
	unsigned int len;
	sscanf(buf, "%1u", &len);
	for(unsigned int i = 0; i < len; i++) {
		buf[i] = reader.pop<char>();
	}
	buf[len] = '\0';
	sscanf(buf, "%u", &len);
	tr[ *this].m_freqInterval = (stop - start) / (samples - 1);
	tr[ *this].trace_().resize(samples);

	convertRawBlock(reader, tr, len);
}

void
XHP8711::acquireTraceData(unsigned int ch, unsigned int len) {
	interface()->send("FORM:DATA REAL,32;BORD SWAP");
	interface()->sendf("TRAC? CH%uFDATA", ch + 1u);
	interface()->receive(len * sizeof(float) + 12);
	//! \todo complex data.
}
void
XHP8711::convertRawBlock(RawDataReader &reader, Transaction &tr,
	unsigned int len) throw (XRecordError&) {
	unsigned int samples = tr[ *this].trace_().size();
	if(len / sizeof(float) < samples)
		throw XBufferUnderflowRecordError(__FILE__, __LINE__);
	if(len / sizeof(float) > samples)
		throw XRecordError(i18n("Select scalar plot."), __FILE__, __LINE__);
	for(unsigned int i = 0; i < samples; i++) {
		tr[ *this].trace_()[i] = pow(10.0, reader.pop<float>() / 20.0);
	}
}

void
XAgilentE5061::acquireTraceData(unsigned int ch, unsigned int len) {
	interface()->send("FORM:DATA REAL32;BORD SWAP");
	interface()->sendf("CALC%u:FORM  SCOMPLEX", ch + 1u);
	interface()->sendf("CALC%u:DATA:FDAT?", ch + 1u);
	interface()->receive(len * sizeof(float) * 2 + 12);
}
void
XAgilentE5061::convertRawBlock(RawDataReader &reader, Transaction &tr,
	unsigned int len) throw (XRecordError&) {
	unsigned int samples = tr[ *this].trace_().size();
	if(len / sizeof(float) < samples * 2)
		throw XBufferUnderflowRecordError(__FILE__, __LINE__);
	for(unsigned int i = 0; i < samples; i++) {
		tr[ *this].trace_()[i] = std::complex<double>(
			reader.pop<float>(), reader.pop<float>());
	}
}

XVNWA3ENetworkAnalyzer::XVNWA3ENetworkAnalyzer(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
	XCharDeviceDriver<XNetworkAnalyzer>(name, runtime, ref(tr_meas), meas) {
	interface()->setEOS("\n");
    trans( *interface()->device()) = "TCP/IP";
    trans( *interface()->port()) = "127.0.0.1:12333";

	average()->disable();
	points()->disable();

	calOpen()->disable();
	calShort()->disable();
	calTerm()->disable();
	calThru()->disable();
}

void
XVNWA3ENetworkAnalyzer::open() throw (XKameError &) {
	start();
}
void
XVNWA3ENetworkAnalyzer::onStartFreqChanged(const Snapshot &shot, XValueNodeBase *) {
    interface()->sendf("FSTART %f", (double)shot[ *startFreq()] * 1e6);
}
void
XVNWA3ENetworkAnalyzer::onStopFreqChanged(const Snapshot &shot, XValueNodeBase *) {
    interface()->sendf("FSTOP %f", (double)shot[ *stopFreq()] * 1e6);
}
void
XVNWA3ENetworkAnalyzer::getMarkerPos(unsigned int num, double &x, double &y) {
	if(num > 1)
		throw XDriver::XSkippedRecordError(__FILE__, __LINE__);
	interface()->queryf("MARK%u?", num);
	if(interface()->scanf("MARK %*u %lf %lf", &x, &y) != 2)
		throw XInterface::XConvError(__FILE__, __LINE__);
	x *= 1e-6;
	y = log10(y) * 10.0;
}
void
XVNWA3ENetworkAnalyzer::oneSweep() {
    unsigned int num;
	interface()->query("ACQNUM?");
	if(interface()->scanf("ACQNUM %u", &num) != 1)
		throw XInterface::XConvError(__FILE__, __LINE__);
	if(num == 0)
		throw XDriver::XSkippedRecordError(__FILE__, __LINE__);
}
void
XVNWA3ENetworkAnalyzer::startContSweep() {
}
void
XVNWA3ENetworkAnalyzer::acquireTrace(shared_ptr<RawData> &writer, unsigned int ch) {
	XScopedLock<XInterface> lock( *interface());
	unsigned int len;
	interface()->query("DATA?");
	if(interface()->scanf("DATA %u", &len) != 1)
		throw XInterface::XConvError(__FILE__, __LINE__);
	interface()->receive(len);
	writer->insert(writer->end(),
					 interface()->buffer().begin(), interface()->buffer().end());
}
void
XVNWA3ENetworkAnalyzer::convertRaw(RawDataReader &reader, Transaction &tr) throw (XRecordError&) {
	const Snapshot &shot(tr);
    uint32_t hsize = reader.pop<uint32_t>();
	int stype = reader.pop<int32_t>();
	double start = reader.pop<double>() * 1e-6; //[MHz]
	double stop = reader.pop<double>() * 1e-6; //[MHz]
	int samples = reader.pop<int32_t>();
	int rec = reader.pop<int32_t>();
	double tm = reader.pop<double>();
	double temp = reader.pop<double>(); //4*4+8*4 = 48bytes
	for(int cnt = 0; cnt < hsize - 48; ++cnt)
		reader.pop<char>(); //skips remaining header.

	double df = (stop - start) / (samples - 1);
	tr[ *this].m_startFreq = start;
	tr[ *this].m_freqInterval = df;
	tr[ *this].trace_().resize(samples);

	switch(stype) {
	case 1: //Linear sweep.
		break;
	case 2:	//Log sweep.
	case 3: //Listed sweep.
	default:
		throw XRecordError(i18n("Log/Listed sweep is not supported."), __FILE__, __LINE__);
	}
	switch(rec) {
	case 1: //S21
	case 2: //S11
	case 3: //S12
	case 4: //S22
		break;
	case 5: //all
	default:
		throw XRecordError(i18n("Select one of record."), __FILE__, __LINE__);
	}

	for(unsigned int i = 0; i < samples; i++) {
		tr[ *this].trace_()[i] = std::complex<double>(reader.pop<double>(), reader.pop<double>());
	}
}


XVNWA3ENetworkAnalyzerTCPIP::XVNWA3ENetworkAnalyzerTCPIP(const char *name, bool runtime,
    Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
    XCharDeviceDriver<XNetworkAnalyzer>(name, runtime, ref(tr_meas), meas),
    m_interface2(XNode::create<XCharInterface>("Interface2", false,
        dynamic_pointer_cast<XDriver>(this->shared_from_this()))) {
    meas->interfaces()->insert(tr_meas, m_interface2);
    interface()->setEOS("");
    interface()->device()->setUIEnabled(false);
    trans( *interface()->device()) = "TCP/IP";
    trans( *interface()->port()) = "127.0.0.1:55555";
    interface2()->setEOS("");
    interface2()->control()->setUIEnabled(false);
    interface2()->device()->setUIEnabled(false);
    trans( *interface2()->device()) = "TCP/IP";
    trans( *interface2()->port()) = "127.0.0.1:55556";

    iterate_commit([=](Transaction &tr){
        tr[ *points()].add({"3", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"});
        tr[ *points()].str("512");
    });

    average()->disable();

    calOpen()->disable();
    calShort()->disable();
    calTerm()->disable();
    calThru()->disable();
}

void
XVNWA3ENetworkAnalyzerTCPIP::open() throw (XKameError &) {
    interface2()->start();
//    interface()->write("stop", 5); //with null.
//    interface()->receive(); //"sweep stopped "
    start();
}
void
XVNWA3ENetworkAnalyzerTCPIP::close() throw (XKameError &) {
    interface2()->stop();
    XCharDeviceDriver<XNetworkAnalyzer>::close();
}
void
XVNWA3ENetworkAnalyzerTCPIP::onStartFreqChanged(const Snapshot &shot, XValueNodeBase *) {
    Snapshot shot_this( *this);
    auto buf = formatString("range %g %g",
        (double)shot_this[ *startFreq()] * 1e6, (double)shot_this[ *stopFreq()] * 1e6);
    interface()->write(buf.c_str(), buf.length() + 1); //with null.
    interface()->receive();
    int err;
    if(interface()->scanf("Error Code: %d", &err) != 1)
        throw XInterface::XConvError(__FILE__, __LINE__);
    if(err != 0)
        throw XInterface::XInterfaceError( &interface()->buffer()[0], __FILE__, __LINE__);
}
void
XVNWA3ENetworkAnalyzerTCPIP::onStopFreqChanged(const Snapshot &shot, XValueNodeBase *node) {
    onStartFreqChanged(shot, node);
}
void
XVNWA3ENetworkAnalyzerTCPIP::onPointsChanged(const Snapshot &shot, XValueNodeBase *) {
    Snapshot shot_this( *this);
    auto buf = formatString("setgrid lin %g %g %u",
        (double)shot_this[ *startFreq()] * 1e6, (double)shot_this[ *stopFreq()] * 1e6,
        atoi(shot_this[ *points()].to_str().c_str()));
    interface()->write(buf.c_str(), buf.length() + 1); //with null.
    interface()->receive();
    int err;
    if(interface()->scanf("Error Code: %d", &err) != 1)
        throw XInterface::XConvError(__FILE__, __LINE__);
    if(err != 0)
        throw XInterface::XInterfaceError( &interface()->buffer()[0], __FILE__, __LINE__);
}

void
XVNWA3ENetworkAnalyzerTCPIP::getMarkerPos(unsigned int num, double &x, double &y) {
    throw XDriver::XSkippedRecordError(__FILE__, __LINE__);
}
void
XVNWA3ENetworkAnalyzerTCPIP::oneSweep() {
    XString buf = "sweep S21 S11";
    interface()->write(buf.c_str(), buf.length() + 1); //with null.
    interface()->receive();
    int err;
    if(interface()->scanf("Error Code: %d", &err) != 1)
        throw XInterface::XConvError(__FILE__, __LINE__);
    if(err != 0)
        throw XInterface::XInterfaceError( &interface()->buffer()[0], __FILE__, __LINE__);
}
void
XVNWA3ENetworkAnalyzerTCPIP::startContSweep() {
}
void
XVNWA3ENetworkAnalyzerTCPIP::acquireTrace(shared_ptr<RawData> &writer, unsigned int ch) {
    XScopedLock<XInterface> lock( *interface());
    interface2()->receive();
    unsigned int num_pts, sweep_type;
    double start_freq, stop_freq;
    if(interface2()->scanf("sweep_start %u %lf %lf %u",
        &num_pts, &start_freq, &stop_freq, &sweep_type) != 4)
        throw XDriver::XSkippedRecordError(__FILE__, __LINE__);
    writer->push((uint32_t)num_pts);
    writer->push((int32_t)sweep_type);
    writer->push(start_freq);
    writer->push(stop_freq);
    for(unsigned int cnt = 0; cnt < num_pts; cnt++) {
        interface2()->receive();
        double freq, res11, ims11, res21, ims21;
        if(interface2()->scanf("data %*u %lf %lf %lf %lf %lf",
            &freq, &res11, &ims11, &res21, &ims21) != 5)
            throw XInterface::XConvError(__FILE__, __LINE__);
        writer->push(freq);
        writer->push(res11);
        writer->push(ims11);
        writer->push(res21);
        writer->push(ims21);
    }
    interface2()->receive(); //sweep_complete
    unsigned int subswp;
    if(interface2()->scanf("sweep_complete %u", &subswp) != 1)
        throw XInterface::XConvError(__FILE__, __LINE__);
    interface()->receive(); //sweep_complete
}
void
XVNWA3ENetworkAnalyzerTCPIP::convertRaw(RawDataReader &reader, Transaction &tr) throw (XRecordError&) {
    const Snapshot &shot(tr);

    uint32_t samples = reader.pop<uint32_t>();
    int stype = reader.pop<int32_t>();
    double start = reader.pop<double>() * 1e-6; //[MHz]
    double stop = reader.pop<double>() * 1e-6; //[MHz]

    double df = (stop - start) / (samples - 1);
    tr[ *this].m_startFreq = start;
    tr[ *this].m_freqInterval = df;
    tr[ *this].trace_().resize(samples);

    switch(stype & 0xfu) {
    case 1: //Linear sweep.
        break;
    case 2:	//Log sweep.
    case 3: //Listed sweep.
    default:
        throw XRecordError(i18n("Log/Listed sweep is not supported."), __FILE__, __LINE__);
    }
    double min_f = 1e10, max_f = -1e10, min_v = 1e10, max_v = -1e10;
    for(unsigned int i = 0; i < samples; i++) {
        double f = reader.pop<double>() * 1e-6; //freq [MHz]
        auto z = std::complex<double>(reader.pop<double>(), reader.pop<double>());
        tr[ *this].trace_()[i] = z;
        reader.pop<double>(); //s21re
        reader.pop<double>(); //s21im
        if(std::abs(z) < min_v) {
            min_v = std::abs(z);
            min_f = f;
        }
        if(std::abs(z) > max_v) {
            max_v = std::abs(z);
            max_f = f;
        }
    }
    m_marker1X->value(tr, min_f);
    m_marker1Y->value(tr, min_v);
    m_marker2X->value(tr, max_f);
    m_marker2Y->value(tr, max_v);
}
