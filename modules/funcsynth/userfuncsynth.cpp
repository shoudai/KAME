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
#include "userfuncsynth.h"
#include "charinterface.h"
#include "analyzer.h"

REGISTER_TYPE(XDriverList, WAVEFACTORY, "NF WAVE-FACTORY pulse generator");

XWAVEFACTORY::XWAVEFACTORY(const char *name, bool runtime, 
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
    XCharDeviceDriver<XFuncSynth>(name, runtime, ref(tr_meas), meas) {

	function()->add("SINUSOID");
	function()->add("TRIANGLE");
	function()->add("SQUARE");
	function()->add("PRAMP");
	function()->add("NRAMP");
	function()->add("USER");
	function()->add("VSQUARE");
	mode()->add("NORMAL");
	mode()->add("BURST");
	mode()->add("SWEEP");
	mode()->add("MODULATION");
	mode()->add("NOISE");
	mode()->add("DC");
}
/*
  double
  XWAVEFACTORY::Read(void)
  {
  string buf;
  Query("?PHS", &buf);
  double x = 0;
  sscanf(buf.c_str(), "%*s %lf", &x);
  return x;
  }
*/
void
XWAVEFACTORY::onOutputChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("SIG %d", *output() ? 1 : 0);
}

void
XWAVEFACTORY::onTrigTouched(const Snapshot &shot, XTouchableNode *) {
	interface()->send("TRG 1");
}

void
XWAVEFACTORY::onModeChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("OMO %d", (int)*mode());
}

void
XWAVEFACTORY::onFunctionChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("FNC %d", (int)*function() + 1);
}

void
XWAVEFACTORY::onFreqChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("FRQ %e" , (double)*freq());
}

void
XWAVEFACTORY::onAmpChanged(const Snapshot &shot, XValueNodeBase *)
{
	interface()->sendf("AMV %e" , (double)*amp());
}

void
XWAVEFACTORY::onPhaseChanged(const Snapshot &shot, XValueNodeBase *) {
	interface()->sendf("PHS %e" , (double)*phase());
}


void
XWAVEFACTORY::onOffsetChanged(const Snapshot &shot, XValueNodeBase *) {
    interface()->sendf("OFS %e" , (double)*offset());
}
