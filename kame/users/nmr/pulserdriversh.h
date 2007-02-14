#include "pulserdriver.h"
#include "chardevicedriver.h"
#include <vector>

//! My pulser driver
class XSHPulser : public XCharDeviceDriver<XPulser>
{
 XNODE_OBJECT
 protected:
  XSHPulser(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XSHPulser() {}

 protected:
    //! send patterns to pulser or turn-off
    virtual void changeOutput(bool output, unsigned int blankpattern);
    //! convert RelPatList to native patterns
    virtual void createNativePatterns();
    //! time resolution [ms]
    virtual double resolution() const;
    //! minimum period of pulses [ms]
    virtual double minPulseWidth() const {return resolution();}
    //! existense of AO ports.
    virtual bool haveQAMPorts() const {return false;}
 private:
  int setAUX2DA(double volt, int addr);
  int makeWaveForm(int num, double pw, tpulsefunc func, double dB, double freq = 0.0, double phase = 0.0);
  int insertPreamble(unsigned short startpattern);
  int finishPulse();
  //! Add 1 pulse pattern
  //! \param msec a period to next pattern
  //! \param pattern a pattern for digital, to appear
  int pulseAdd(double msec, uint32_t pattern, bool firsttime);
  uint32_t m_lastPattern;
  double m_dmaTerm;  
  
  std::vector<unsigned char> m_zippedPatterns;
  
  int m_waveformPos[3];
  
};
