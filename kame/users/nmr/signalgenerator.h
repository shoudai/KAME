#ifndef signalgeneratorH
#define signalgeneratorH

#include "primarydriver.h"
#include "xnodeconnector.h"

class FrmSG;

class XSG : public XPrimaryDriver
{
 XNODE_OBJECT
 protected:
  XSG(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  //! usually nothing to do
  virtual ~XSG() {}
  //! show all forms belonging to driver
  virtual void showForms();
 protected:
  //! Start up your threads, connect GUI, and activate signals
  virtual void start();
  //! Shut down your threads, unconnect GUI, and deactivate signals
  //! this may be called even if driver has already stopped.
  virtual void stop();
  
  //! this is called when raw is written 
  //! unless dependency is broken
  //! convert raw to record
  virtual void analyzeRaw() throw (XRecordError&);
  //! this is called after analyze() or analyzeRaw()
  //! record is readLocked
  virtual void visualize();
 public:
  //! driver specific part below
  const shared_ptr<XDoubleNode> &freq() const {return m_freq;} //!< freq [MHz]
  const shared_ptr<XDoubleNode> &oLevel() const {return m_oLevel;} //!< Output Level [dBm]
  const shared_ptr<XBoolNode> &fmON() const {return m_fmON;} //!< Activate FM
  const shared_ptr<XBoolNode> &amON() const {return m_amON;} //!< Activate AM
    
  double freqRecorded() const {return m_freqRecorded;} //!< freq [MHz]
 protected:
  virtual void changeFreq(double mhz) = 0;
  virtual void onOLevelChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onFMONChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onAMONChanged(const shared_ptr<XValueNodeBase> &) = 0;
 private:
  void onFreqChanged(const shared_ptr<XValueNodeBase> &);
  
  shared_ptr<XDoubleNode> m_freq;
  shared_ptr<XDoubleNode> m_oLevel;
  shared_ptr<XBoolNode> m_fmON;
  shared_ptr<XBoolNode> m_amON;
  
  double m_freqRecorded;
  
  xqcon_ptr m_conFreq, m_conOLevel, m_conFMON, m_conAMON;
  shared_ptr<XListener> m_lsnFreq, m_lsnOLevel, m_lsnFMON, m_lsnAMON;
  
  qshared_ptr<FrmSG> m_form;
};//---------------------------------------------------------------------------

//! KENWOOD SG-7200
class XSG7200 : public XSG
{
 XNODE_OBJECT
 protected:
  XSG7200(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XSG7200() {}

 protected:
  virtual void changeFreq(double mhz);
  virtual void onOLevelChanged(const shared_ptr<XValueNodeBase> &);
  virtual void onFMONChanged(const shared_ptr<XValueNodeBase> &);
  virtual void onAMONChanged(const shared_ptr<XValueNodeBase> &);
 private:
};

//! KENWOOD SG-7130
class XSG7130 : public XSG7200
{
 XNODE_OBJECT
 protected:
  XSG7130(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XSG7130() {}
};

//! Agilent 8643A, 8644A
class XHP8643 : public XSG
{
 XNODE_OBJECT
 protected:
  XHP8643(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XHP8643() {}
 protected:
  virtual void changeFreq(double mhz);
  virtual void onOLevelChanged(const shared_ptr<XValueNodeBase> &);
  virtual void onFMONChanged(const shared_ptr<XValueNodeBase> &);
  virtual void onAMONChanged(const shared_ptr<XValueNodeBase> &);
 private:
};

//! Agilent 8648
class XHP8648 : public XHP8643
{
 XNODE_OBJECT
 protected:
  XHP8648(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XHP8648() {}
 protected:
  virtual void onOLevelChanged(const shared_ptr<XValueNodeBase> &);
 private:
};

#endif
