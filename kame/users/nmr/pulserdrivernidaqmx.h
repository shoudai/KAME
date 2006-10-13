#ifdef HAVE_NI_DAQMX

#include "pulserdriver.h"
#include <vector>

class XNIDAQmxPulser : public XPulser
{
 XNODE_OBJECT
 protected:
  XNIDAQmxPulser(const char *name, bool runtime,
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers);
 public:
  virtual ~XNIDAQmxPulser() {}

 protected:
    virtual void afterStart() {}
    
    //! send patterns to pulser or turn-off
    virtual void changeOutput(bool output);
    //! convert RelPatList to native patterns
    virtual void createNativePatterns();
    //! time resolution [ms]
    virtual double resolution();
    //! create RelPatList
    virtual void rawToRelPat() throw (XRecordError&);
 private:
    //A pettern at absolute time
    class tpat {
          public:
          tpat(double npos, uint32_t newpat, uint32_t nmask) {
              pos = npos; pat = newpat; mask = nmask;
          }
          tpat(const tpat &x) {
              pos = x.pos; pat = x.pat; mask = x.mask;
          }
          double pos;
          //this pattern bits will be outputted at 'pos'
          uint32_t pat;
          //mask bits
          uint32_t mask;
          
  
        bool operator< (const tpat &y) const {
              return pos < y.pos;
        }          
    }; 
 
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
#endif //HAVE_NI_DAQMX