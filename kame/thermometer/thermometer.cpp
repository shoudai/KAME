//---------------------------------------------------------------------------
#include <math.h>
#include <klocale.h>

#include "thermometer.h"
#include "cspline.h"
//---------------------------------------------------------------------------
XThermometerList::XThermometerList(const char *name, bool runtime)
 : XCustomTypeListNode<XThermometer>(name, runtime) {
}

#define LIST XThermometerList
DECLARE_TYPE_HOLDER

REGISTER_TYPE(RawThermometer)
REGISTER_TYPE(LakeShore)
REGISTER_TYPE(CryoConcept)
REGISTER_TYPE(ScientificInstruments)
REGISTER_TYPE(ApproxThermometer)

XThermometer::XThermometer(const char *name, bool runtime) : 
    XNode(name, runtime),
    m_tempMin(create<XDoubleNode>("TMin", false)),
    m_tempMax(create<XDoubleNode>("TMax", false))
  {
    tempMin()->value(1e-3);
    tempMax()->value(1e3);
  }

XLakeShore::XLakeShore(const char *name, bool runtime) :
 XThermometer(name, runtime),
 m_resMin(create<XDoubleNode>("RMin", false)),
 m_resMax(create<XDoubleNode>("RMax", false)),
 m_zu(create<XDoubleListNode>("Zu", false)),
 m_zl(create<XDoubleListNode>("Zl", false)),
 m_ai(create<XDouble2DNode>("Ai", false))
{
}

double
XLakeShore::getRawValue(double temp) const
{
  //using Newton's method
  double x, y, dypdx, val;
  if(temp < *tempMin()) return *resMax();
  if(temp > *tempMax()) return *resMin();
  //    x = (log10(RMin) + log10(RMax)) / 2;
  val = *resMin();
  for(double dy = 0.0001;;dy *= 2)
    {
      if(dy > 1.0) return *resMin();
      double t = (double)KAME::rand() / (RAND_MAX - 1);
      x = (log10(*resMax()) * t + log10(*resMin()) * (1 - t));
      for(int i = 0; i < 100; i++)
	{
	  y = getTemp(pow(10, x)) - temp;
	  if(fabs(y) < dy)
	    {
	      val = pow(10, x);
	      return val;
	    }
	  dypdx = (y - (getTemp(pow(10, x - 0.00001)) - temp)) / 0.00001;
	  if(dypdx != 0) x -= y / dypdx;
	  if((x > log10(*resMax())) || (x < log10(*resMin())) || (dypdx == 0))
	    {
	      double t = (double)KAME::rand() / (RAND_MAX - 1);
	      x = (log10(*resMax()) * t + log10(*resMin()) * (1 - t));
	    }
	}
    }
  return val;
}

double
XLakeShore::getTemp(double res) const
{
  double temp = 0, z, u = 0;
  if(res > *resMax()) return *tempMin();
  if(res < *resMin()) return *tempMax();
  z = log10(res);
  unsigned int n;
  atomic_shared_ptr<const XNode::NodeList> zu_list(zu()->children());
  atomic_shared_ptr<const XNode::NodeList> zl_list(zl()->children());
  for(n = 0; n < zu_list->size(); n++)
    {
        double zu = *dynamic_pointer_cast<XDoubleNode>(zu_list->at(n));
        double zl = *dynamic_pointer_cast<XDoubleNode>(zl_list->at(n));
      u = (z - zu + z - zl) / (zu - zl);
      if((u >= -1) && (u <= 1)) break;
    }
  if(n >= zu_list->size())
    return 0;
  atomic_shared_ptr<const XNode::NodeList> ai_list(ai()->children());
  atomic_shared_ptr<const XNode::NodeList> ai_n_list(ai_list->at(n)->children());
  for(unsigned int i = 0; i < ai_n_list->size(); i++)
    {
        double ai_n_i = *dynamic_pointer_cast<XDoubleNode>(ai_n_list->at(i));
        temp += ai_n_i * cos(i * acos(u));
    }
  return temp;
}

XCryoConcept::XCryoConcept(const char *name, bool runtime) :
 XThermometer(name, runtime),
 m_resMin(create<XDoubleNode>("RMin", false)),
 m_resMax(create<XDoubleNode>("RMax", false)),
 m_ai(create<XDoubleListNode>("Ai", false)),
 m_a(create<XDoubleNode>("A", false)),
 m_t0(create<XDoubleNode>("T0", false)),
 m_r0(create<XDoubleNode>("R0", false)),
 m_rCrossover(create<XDoubleNode>("RCrossover", false))
 {
  }
  
double
XCryoConcept::getRawValue(double temp) const
{
  //using Newton's method
  double x, y, dypdx, val;
  if(temp < *tempMin()) return *resMax();
  if(temp > *tempMax()) return *resMin();
  //    x = (log10(RMin) + log10(RMax)) / 2;
  val = *resMin();
  for(double dy = 0.0001;;dy *= 2)
    {
      if(dy > 1.0) return *resMin();
      double t = (double)KAME::rand() / (RAND_MAX - 1);
      x = (log10(*resMax()) * t + log10(*resMin()) * (1 - t));
      for(int i = 0; i < 100; i++)
	{
	  y = getTemp(pow(10, x)) - temp;
	  if(fabs(y) < dy)
	    {
	      val = pow(10, x);
	      return val;
	    }
	  dypdx = (y - (getTemp(pow(10, x - 0.00001)) - temp)) / 0.00001;
	  if(dypdx != 0) x -= y / dypdx;
	  if((x > log10(*resMax())) || (x < log10(*resMin())) || (dypdx == 0))
	    {
	      double t = (double)KAME::rand() / (RAND_MAX - 1);
	      x = (log10(*resMax()) * t + log10(*resMin()) * (1 - t));
	    }
	}
    }
  return val;
}
double
XCryoConcept::getTemp(double res) const
{
  if(res > *resMax()) return *tempMin();
  if(res < *resMin()) return *tempMax();
  if(res < *rCrossover())
    { 
      double y = 0, r = 1.0;
      double x = log10(res);
      atomic_shared_ptr<const XNode::NodeList> ai_list(ai()->children());
      for(XNode::NodeList::const_iterator it = ai_list->begin(); it != ai_list->end(); it++) {
          double ai_i = *dynamic_pointer_cast<XDoubleNode>(*it);
        	  y += ai_i * r;
        	  r *= x;
      }
      return pow(10.0, y);
    }
  else
    {
      return *t0() / pow(log10(res / *r0()), *a());
    }
}

XScientificInstruments::XScientificInstruments(const char *name, bool runtime) : 
  XThermometer(name, runtime),
 m_resMin(create<XDoubleNode>("RMin", false)),
 m_resMax(create<XDoubleNode>("RMax", false)),
 m_abcde(create<XDoubleListNode>("ABCDE", false)),
 m_abc(create<XDoubleListNode>("ABC", false)),
 m_rCrossover(create<XDoubleNode>("RCrossover", false))
 {
  }

double
XScientificInstruments
::getRawValue(double temp) const
{
  //using Newton's method
  double x, y, dypdx, val;
  if(temp < *tempMin()) return *resMax();
  if(temp > *tempMax()) return *resMin();
  //    x = (log10(RMin) + log10(RMax)) / 2;
  val = *resMin();
  for(double dy = 0.0001;;dy *= 2)
    {
      if(dy > 1.0) return *resMin();
      double t = (double)KAME::rand() / (RAND_MAX - 1);
      x = (log10(*resMax()) * t + log10(*resMin()) * (1 - t));
      for(int i = 0; i < 100; i++)
	{
	  y = getTemp(pow(10, x)) - temp;
	  if(fabs(y) < dy)
	    {
	      val = pow(10, x);
	      return val;
	    }
	  dypdx = (y - (getTemp(pow(10, x - 0.00001)) - temp)) / 0.00001;
	  if(dypdx != 0) x -= y / dypdx;
	  if((x > log10(*resMax())) || (x < log10(*resMin())) || (dypdx == 0))
	    {
	      double t = (double)KAME::rand() / (RAND_MAX - 1);
	      x = (log10(*resMax()) * t + log10(*resMin()) * (1 - t));
	    }
	}
    }
  return val;
}

double
XScientificInstruments
::getTemp(double res) const
{
  if(res > *resMax()) return *tempMin();
  if(res < *resMin()) return *tempMax();
  double y = 0.0;
  double lx = log(res);
  if(res > *rCrossover())
    { 
      atomic_shared_ptr<const XNode::NodeList> abcde_list(abcde()->children());
      if(abcde_list->size() >= 5) {
        double a = *dynamic_pointer_cast<XDoubleNode>(abcde_list->at(0));
        double b = *dynamic_pointer_cast<XDoubleNode>(abcde_list->at(1));
        double c = *dynamic_pointer_cast<XDoubleNode>(abcde_list->at(2));
        double d = *dynamic_pointer_cast<XDoubleNode>(abcde_list->at(3));
        double e = *dynamic_pointer_cast<XDoubleNode>(abcde_list->at(4));
  		y = (a + c*lx + e*lx*lx)
  			/ (1.0 + b*lx + d*lx*lx);
      }
      return y;
    }
  else
    {
        atomic_shared_ptr<const XNode::NodeList> abc_list(abc()->children());
        if(abc_list->size() >= 3) {
            double a = *dynamic_pointer_cast<XDoubleNode>(abc_list->at(0));
            double b = *dynamic_pointer_cast<XDoubleNode>(abc_list->at(1));
            double c = *dynamic_pointer_cast<XDoubleNode>(abc_list->at(2));
      		y = 1.0/(a + b*res*lx + c*res*res);
        }
	    return y;
    }
}

XApproxThermometer::XApproxThermometer(const char *name, bool runtime) :
 XThermometer(name, runtime),
 m_resList(create<XDoubleListNode>("ResList", false)),
 m_tempList(create<XDoubleListNode>("TempList", false))
{
}

double
XApproxThermometer
::getTemp(double res) const
{
    atomic_shared_ptr<CSplineApprox> approx(m_approx);
    if(!approx) {
        std::map<double, double> pts;
      atomic_shared_ptr<const XNode::NodeList> res_list(m_resList->children());
      atomic_shared_ptr<const XNode::NodeList> temp_list(m_tempList->children());
        for(unsigned int i = 0; i < std::min(res_list->size(), temp_list->size()); i++) {
            double res = *dynamic_pointer_cast<XDoubleNode>(res_list->at(i));
            double temp = *dynamic_pointer_cast<XDoubleNode>(temp_list->at(i));
            pts.insert(std::pair<double, double>(log(res), log(temp)));
        }
        if(pts.size() < 4)
            throw XKameError(
                i18n("XApproxThermometer, Too small number of points"), __FILE__, __LINE__);
        approx.reset(new CSplineApprox(pts));
        m_approx = approx;
    }
    return exp(approx->approx(log(res)));
}
double
XApproxThermometer
::getRawValue(double temp) const
{
    atomic_shared_ptr<CSplineApprox> approx(m_approx_inv);
    if(!approx) {
        std::map<double, double> pts;
      atomic_shared_ptr<const XNode::NodeList> res_list(m_resList->children());
      atomic_shared_ptr<const XNode::NodeList> temp_list(m_tempList->children());
        for(unsigned int i = 0; i < std::min(res_list->size(), temp_list->size()); i++) {
            double res = *dynamic_pointer_cast<XDoubleNode>(res_list->at(i));
            double temp = *dynamic_pointer_cast<XDoubleNode>(temp_list->at(i));
            pts.insert(std::pair<double, double>(log(temp), log(res)));
        }
        if(pts.size() < 4)
            throw XKameError(
                i18n("XApproxThermometer, Too small number of points"), __FILE__, __LINE__);
        approx.reset(new CSplineApprox(pts));
        m_approx_inv = approx;
    }
    return exp(approx->approx(log(temp)));
}

