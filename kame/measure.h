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
//! Class XMeasure
//! The root node of KAME
//---------------------------------------------------------------------------
#ifndef measureH
#define measureH

#include "xnode.h"
#include "xnodeconnector.h"

class XThermometerList;
class XDriverList;
class XInterfaceList;
class XStatusPrinter;
class XDriverList;
class XScalarEntryList;
class XGraphList;
class XChartList;
class XTextWriter;
class XRawStreamRecorder;
class XRawStreamRecordReader;
class XRuby;

/*! The root node of KAME.
*/
class XMeasure : public XNode
{
 XNODE_OBJECT
 protected:
  XMeasure(const char *name, bool runtime);
 public:
  virtual ~XMeasure();

  //! call me before loading a measurement file.
  void initialize();
  //! clean all drivers, thermometers.
  void terminate();
  //! stop all drivers.
  void stop();

  const shared_ptr<XThermometerList> &thermometerList() const {return m_thermometers;}
  const shared_ptr<XDriverList> &driverList() const {return m_drivers;}
  const shared_ptr<XInterfaceList> &interfaceList() const {return m_interfaces;} 
  const shared_ptr<XScalarEntryList> &scalarEntryList() const {return m_scalarEntries;}
  const shared_ptr<XGraphList> &graphList() const {return m_graphList;}
  const shared_ptr<XChartList> &chartList() const {return m_chartList;}
  const shared_ptr<XTextWriter> &textWriter() const {return m_textWriter;}
  const shared_ptr<XRawStreamRecorder> &rawStreamRecorder() const {return m_rawStreamRecorder;}
  const shared_ptr<XRawStreamRecordReader> &rawStreamRecordReader() const {return m_rawStreamRecordReader;}
  
  const shared_ptr<XRuby> &ruby() const {return m_ruby;}
 private:
  shared_ptr<XRuby> m_ruby;
 
  const shared_ptr<XThermometerList> m_thermometers;
  const shared_ptr<XScalarEntryList> m_scalarEntries;
  const shared_ptr<XGraphList> m_graphList;
  const shared_ptr<XChartList> m_chartList;
  const shared_ptr<XInterfaceList> m_interfaces;
  const shared_ptr<XDriverList> m_drivers;
  const shared_ptr<XTextWriter> m_textWriter;
  const shared_ptr<XRawStreamRecorder> m_rawStreamRecorder;
  const shared_ptr<XRawStreamRecordReader> m_rawStreamRecordReader;
  
  const xqcon_ptr m_conRecordReader,
   m_conDrivers, m_conInterfaces, m_conEntries, m_conGraphs,
   m_conTextWrite, m_conTextURL, m_conTextLastLine,
   m_conBinURL, m_conBinWrite, m_conUrlRubyThread,
   m_conCalTable;
  shared_ptr<XListener> m_lsnOnReleaseDriver;
  void onReleaseDriver(const shared_ptr<XNode> &driver);
};

//! use this to show a floating information at the front of the main window.
//! \sa XStatusPrinter
extern shared_ptr<XStatusPrinter> g_statusPrinter;

//---------------------------------------------------------------------------
#endif
