/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitagawa@scphys.kyoto-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
 ***************************************************************************/
#include "secondarydriver.h"
#include <klocale.h>

XSecondaryDriver::XSecondaryDriver(const char *name, bool runtime, 
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers) :
    XDriver(name, runtime, scalarentries, interfaces, thermometers, drivers),
    m_dependency(new XRecordDependency())
{
}

bool
XSecondaryDriver::checkDeepDependency(shared_ptr<XRecordDependency> &dep) const
{
    bool sane = true;
    dep->clear();
    for(tConnection_it it = m_connection_check_deep_dep.begin(); it != m_connection_check_deep_dep.end(); it++) {
        bool is_conflict = dep->merge(*it);
        if(is_conflict) {
            sane = false;
            break;
        }
    }
    return sane;
}


void
XSecondaryDriver::readLockAllConnections() {
    m_connection_mutex.readLock();
    for(tConnection_it it = m_connection.begin(); it != m_connection.end(); it++) {
        (*it)->readLockRecord();
    }
}
void
XSecondaryDriver::readUnlockAllConnections() {
    for(tConnection_it it = m_connection.begin(); it != m_connection.end(); it++) {
        (*it)->readUnlockRecord();
    }
    m_connection_mutex.readUnlock();
}
void
XSecondaryDriver::requestAnalysis()
{
    onConnectedRecorded(dynamic_pointer_cast<XDriver>(shared_from_this()));
}
void
XSecondaryDriver::onConnectedRecorded(const shared_ptr<XDriver> &driver)
{
    readLockAllConnections();
    //! check if emitter has already connected or if self-emission
    if((std::find(m_connection.begin(), m_connection.end(), driver)
             != m_connection.end()) 
       || (driver == shared_from_this())) {
        //! driver-side dependency check
        if(checkDependency(driver)) {
            shared_ptr<XRecordDependency> dep(new XRecordDependency);
            //! check if recorded times don't contradict
            if(checkDeepDependency(dep)) {
            	bool skipped = false;
                startRecording();
                XTime time_recorded = driver->time();
                try {
                    analyze(driver);
                }
                catch (XSkippedRecordError&) {
                	skipped = true;
                }
                catch (XRecordError& e) {
                     time_recorded = XTime(); //record is invalid
                     e.print(getLabel() + ": " + KAME::i18n("Record Error, because "));
                }
                readUnlockAllConnections();
                if(skipped)
                	abortRecording();
            	else {
	                m_dependency = dep;
	                finishRecordingNReadLock(driver->timeAwared(), time_recorded);
	                visualize();
	                readUnlockRecord();
            	}
            }
            else    
                readUnlockAllConnections();
        }
        else    
            readUnlockAllConnections();
    }
    else    
        readUnlockAllConnections();
}
void
XSecondaryDriver::connect(const shared_ptr<XItemNodeBase> &item, bool check_deep_dep)
{
    if(m_lsnBeforeItemChanged)
        item->beforeValueChanged().connect(m_lsnBeforeItemChanged);
    else
        m_lsnBeforeItemChanged = item->beforeValueChanged().connectWeak(
                false, shared_from_this(), &XSecondaryDriver::beforeItemChanged);
	if(check_deep_dep) {
	    if(m_lsnOnItemChangedCheckDeepDep)
	        item->onValueChanged().connect(m_lsnOnItemChangedCheckDeepDep);
	    else
	        m_lsnOnItemChangedCheckDeepDep = item->onValueChanged().connectWeak(
	                false, shared_from_this(), &XSecondaryDriver::onItemChangedCheckDeepDep);
	}
	else {
	    if(m_lsnOnItemChanged)
	        item->onValueChanged().connect(m_lsnOnItemChanged);
	    else
	        m_lsnOnItemChanged = item->onValueChanged().connectWeak(
	                false, shared_from_this(), &XSecondaryDriver::onItemChanged);
	}
}
void
XSecondaryDriver::beforeItemChanged(const shared_ptr<XValueNodeBase> &node) {
    //! changes in items are not allowed while onRecord() is emitting
    m_connection_mutex.writeLock();

    shared_ptr<XPointerItemNode<XDriverList> > item =
         dynamic_pointer_cast<XPointerItemNode<XDriverList> >(node);
    shared_ptr<XNode> nd = *item;
    shared_ptr<XDriver> driver = dynamic_pointer_cast<XDriver>(nd);

    if(driver) {
        driver->onRecord().disconnect(m_lsnOnRecord);
        ASSERT(std::find(m_connection.begin(), m_connection.end(), driver) != m_connection.end());
        m_connection.erase(std::find(m_connection.begin(), m_connection.end(), driver));
        std::vector<shared_ptr<const XDriver> >::iterator it = 
        	std::find(m_connection_check_deep_dep.begin(), m_connection_check_deep_dep.end(), driver);
        if(it != m_connection_check_deep_dep.end())
        	m_connection_check_deep_dep.erase(it);
    }
}
void
XSecondaryDriver::onItemChangedCheckDeepDep(const shared_ptr<XValueNodeBase> &node) {
    shared_ptr<XPointerItemNode<XDriverList> > item = 
        dynamic_pointer_cast<XPointerItemNode<XDriverList> >(node);
    shared_ptr<XNode> nd = *item;
    shared_ptr<XDriver> driver = dynamic_pointer_cast<XDriver>(nd);

    if(driver) {
        m_connection_check_deep_dep.push_back(driver);
    }

    onItemChanged(node);
}
void
XSecondaryDriver::onItemChanged(const shared_ptr<XValueNodeBase> &node) {
    shared_ptr<XPointerItemNode<XDriverList> > item = 
        dynamic_pointer_cast<XPointerItemNode<XDriverList> >(node);
    shared_ptr<XNode> nd = *item;
    shared_ptr<XDriver> driver = dynamic_pointer_cast<XDriver>(nd);

    if(driver) {
        m_connection.push_back(driver);
        if(m_lsnOnRecord)
            driver->onRecord().connect(m_lsnOnRecord);
        else
            m_lsnOnRecord = driver->onRecord().connectWeak(
                false, shared_from_this(), &XSecondaryDriver::onConnectedRecorded);
    }

    m_connection_mutex.writeUnlock();
}
