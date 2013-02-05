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
#include "primarydriver.h"

XPrimaryDriver::XPrimaryDriver(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
    XDriver(name, runtime, tr_meas, meas) {
}

void
XPrimaryDriver::finishWritingRaw(const shared_ptr<const RawData> &rawdata,
    const XTime &time_awared, const XTime &time_recorded_org) {

    XTime time_recorded = time_recorded_org;
	for(Transaction tr( *this);; ++tr) {
		XKameError err;
		bool skipped = false;
		if(time_recorded) {
			try {
				RawDataReader reader( *rawdata);
				tr[ *this].m_rawData = rawdata;
				analyzeRaw(reader, tr);
			}
			catch (XSkippedRecordError& e) {
				skipped = true;
				err = e;
			}
			catch (XRecordError& e) {
				time_recorded = XTime(); //record is invalid
				err = e;
			}
		}
		if( !skipped)
			record(tr, time_awared, time_recorded);
		if(tr.commit()) {
			if(e.msg().length())
				e.print(getLabel() + ": ");
			visualize(tr);
			break;
		}
	}
}
