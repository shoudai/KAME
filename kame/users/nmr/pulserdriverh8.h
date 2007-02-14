#include "pulserdriver.h"
#include "chardevicedriver.h"
#include <vector>

//! My pulser driver
class XH8Pulser : public XCharDeviceDriver<XPulser>
{
 XNODE_OBJECT
 protected:
  XH8Pulser(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XH8Pulser() {}

 protected:
  //! Be called just after opening interface. Call start() inside this routine appropriately.
  virtual void open() throw (XInterface::XInterfaceError &);
    //! send patterns to pulser or turn-off
    virtual void changeOutput(bool output, unsigned int blankpattern);
    //! convert RelPatList to native patterns
    virtual void createNativePatterns();
    //! time resolution [ms]
    virtual double resolution() const;
    //! minimum period of pulses [ms]
    virtual double minPulseWidth() const;
    //! existense of AO ports.
    virtual bool haveQAMPorts() const {return false;}
 private:
  //! Add 1 pulse pattern
  //! \param msec a period to next pattern
  //! \param pattern a pattern for digital, to appear
  int pulseAdd(double msec, unsigned short pattern);

  struct h8ushort {unsigned char msb; unsigned char lsb;};
  std::vector<h8ushort> m_zippedPatterns;
};
