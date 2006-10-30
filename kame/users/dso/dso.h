//---------------------------------------------------------------------------

#ifndef dsoH
#define dsoH
//---------------------------------------------------------------------------
#include "primarydriver.h"
#include "xnodeconnector.h"

class XScalarEntry;

#include "fir.h"

class FrmDSO;
class XWaveNGraph;
class FrmGraphNURL;
class XPulser;

//! Base class for digital storage oscilloscope.
class XDSO : public XPrimaryDriver
{
 XNODE_OBJECT
 protected:
  XDSO(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  //! usually nothing to do.
  virtual ~XDSO() {}
  //! show all forms belonging to driver.
  virtual void showForms();
 protected:
  //! Start up your threads, connect GUI, and activate signals
  virtual void start();
  //! Shut down your threads, unconnect GUI, and deactivate signals
  //! this may be called even if driver has already stopped.
  virtual void stop();
  
  //! this is called after analyze() or analyzeRaw()
  //! record is readLocked
  virtual void visualize();
  
  //! driver specific part below
 public:
  const shared_ptr<XUIntNode> &average() const {return m_average;}
  //! If true, pause acquision after averaging count
  const shared_ptr<XBoolNode> &singleSequence() const {return m_singleSequence;}
  const shared_ptr<XBoolNode> &fetch() const {return m_fetch;}
  const shared_ptr<XComboNode> &trigSource() const {return m_trigSource;}
  const shared_ptr<XDoubleNode> &trigPos() const {return m_trigPos;}
  const shared_ptr<XDoubleNode> &trigLevel() const {return m_trigLevel;}
  const shared_ptr<XBoolNode> &trigFalling() const {return m_trigFalling;}
  const shared_ptr<XDoubleNode> &timeWidth() const {return m_timeWidth;}
  const shared_ptr<XComboNode> &vFullScale1() const {return m_vFullScale1;}
  const shared_ptr<XComboNode> &vFullScale2() const {return m_vFullScale2;}
  const shared_ptr<XDoubleNode> &vOffset1() const {return m_vOffset1;}
  const shared_ptr<XDoubleNode> &vOffset2() const {return m_vOffset2;}
  const shared_ptr<XUIntNode> &recordLength() const {return m_recordLength;}
  const shared_ptr<XNode> &forceTrigger() const {return m_forceTrigger;}  

  const shared_ptr<XComboNode> &trace1() const {return m_trace1;}
  const shared_ptr<XComboNode> &trace2() const {return m_trace2;}
  
  const shared_ptr<XBoolNode> &firEnabled() const {return m_firEnabled;}
  const shared_ptr<XDoubleNode> &firBandWidth() const {return m_firBandWidth;} ///< [kHz]
  const shared_ptr<XDoubleNode> &firCenterFreq() const {return m_firCenterFreq;} ///< [kHz]
  const shared_ptr<XDoubleNode> &firSharpness() const {return m_firSharpness;}

  //! Records below
  double trigPosRecorded() const {return m_trigPosRecorded;} ///< unit is interval
  unsigned int numChannelsRecorded() const {return m_numChannelsRecorded;}
  double timeIntervalRecorded() const {return m_timeIntervalRecorded;} //! [sec]
  unsigned int lengthRecorded() const;
  double *waveRecorded(unsigned int ch);
  
 protected:
  virtual void onAverageChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onSingleChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onTrigSourceChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onTrigPosChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onTrigLevelChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onTrigFallingChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onTimeWidthChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onVFullScale1Changed(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onVFullScale2Changed(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onVOffset1Changed(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onVOffset2Changed(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onRecordLengthChanged(const shared_ptr<XValueNodeBase> &) = 0;
  virtual void onForceTriggerTouched(const shared_ptr<XNode> &) = 0;

  virtual void afterStart() = 0;
  virtual void beforeStop() = 0;
  
  virtual double getTimeInterval() = 0;
  //! clear count or start sequence measurement
  virtual void startSequence() = 0;
  //! \arg seq_busy true if sequence is not finished
  virtual int acqCount(bool *seq_busy) = 0;

  //! load waveform and settings from instrument
  virtual void getWave(std::deque<std::string> &channels) = 0;
  //! convert raw to record
  virtual void convertRaw() throw (XRecordError&) = 0;
  
  //! this is called when raw is written 
  //! unless dependency is broken
  //! convert raw to record
  virtual void analyzeRaw() throw (XRecordError&);
  
  void setRecordDim(unsigned int channels, double startpos, double interval, unsigned int length);
 private:
  const shared_ptr<XWaveNGraph> &waveForm() const {return m_waveForm;}
  
  shared_ptr<XUIntNode> m_average;
  //! If true, pause acquision after averaging count
  shared_ptr<XBoolNode> m_singleSequence;
  shared_ptr<XBoolNode> m_fetch;
  shared_ptr<XComboNode> m_trigSource;
  shared_ptr<XBoolNode> m_trigFalling;
  shared_ptr<XDoubleNode> m_trigPos;
  shared_ptr<XDoubleNode> m_trigLevel;
  shared_ptr<XDoubleNode> m_timeWidth;
  shared_ptr<XComboNode> m_vFullScale1;
  shared_ptr<XComboNode> m_vFullScale2;
  shared_ptr<XDoubleNode> m_vOffset1;
  shared_ptr<XDoubleNode> m_vOffset2;
  shared_ptr<XUIntNode> m_recordLength;
  shared_ptr<XNode> m_forceTrigger;  
  shared_ptr<XComboNode> m_trace1;
  shared_ptr<XComboNode> m_trace2;
  shared_ptr<XBoolNode> m_firEnabled;
  shared_ptr<XDoubleNode> m_firBandWidth; ///< [kHz]
  shared_ptr<XDoubleNode> m_firCenterFreq; ///< [kHz]
  shared_ptr<XDoubleNode> m_firSharpness;

  qshared_ptr<FrmDSO> m_form;
  shared_ptr<XWaveNGraph> m_waveForm;
  
  //! these are record
  double m_trigPosRecorded; ///< unit is interval
  unsigned int m_numChannelsRecorded;
  double m_timeIntervalRecorded; //! [sec]
  std::vector<double> m_wavesRecorded;
  
  shared_ptr<XListener> m_lsnOnSingleChanged;
  shared_ptr<XListener> m_lsnOnAverageChanged;
  shared_ptr<XListener> m_lsnOnTrigSourceChanged;
  shared_ptr<XListener> m_lsnOnTrigPosChanged;
  shared_ptr<XListener> m_lsnOnTrigLevelChanged;
  shared_ptr<XListener> m_lsnOnTrigFallingChanged;
  shared_ptr<XListener> m_lsnOnTimeWidthChanged;
  shared_ptr<XListener> m_lsnOnVFullScale1Changed;
  shared_ptr<XListener> m_lsnOnVFullScale2Changed;
  shared_ptr<XListener> m_lsnOnVOffset1Changed;
  shared_ptr<XListener> m_lsnOnVOffset2Changed;
  shared_ptr<XListener> m_lsnOnRecordLengthChanged;
  shared_ptr<XListener> m_lsnOnForceTriggerTouched;
  shared_ptr<XListener> m_lsnOnCondChanged;
  
  void onCondChanged(const shared_ptr<XValueNodeBase> &);
  
  xqcon_ptr m_conAverage, m_conSingle, m_conTrace1, m_conTrace2;
  xqcon_ptr m_conFetch, m_conTimeWidth, m_conVFullScale1, m_conVFullScale2;
  xqcon_ptr m_conTrigSource, m_conTrigPos, m_conTrigLevel, m_conTrigFalling;
  xqcon_ptr m_conVOffset1, m_conVOffset2, m_conForceTrigger, m_conRecordLength;
  xqcon_ptr m_conFIREnabled, m_conFIRBandWidth, m_conFIRSharpness, m_conFIRCenterFreq;
 
  shared_ptr<XThread<XDSO> > m_thread;
  shared_ptr<XStatusPrinter> m_statusPrinter;

  FIR m_fir;
  
  void *execute(const atomic<bool> &);
  
  static const char *s_trace_names[];
};

//---------------------------------------------------------------------------

#endif
