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
#include "nidaqmxdriver.h"
#include <sys/errno.h>

#ifdef HAVE_NI_DAQMX


const XNIDAQmxInterface::ProductInfo
XNIDAQmxInterface::sc_productInfoList[] = {
	{"PCI-6110", "S", 5000uL, 2500uL, 0, 0},
	{"PXI-6110", "S", 5000uL, 2500uL, 0, 0},
//	{"PCI-6111", "S", 5000uL, 2500uL, 0, 0},
	{"PCI-6111", "S", 5000uL, 100uL, 0, 0},
	{"PXI-6111", "S", 5000uL, 2500uL, 0, 0},
	{"PCI-6115", "S", 10000uL, 2500uL, 10000uL, 10000uL},
	{"PCI-6120", "S", 800uL, 2500uL, 10000uL, 10000uL},
	{"PCI-6220", "M", 250uL, 0, 1000uL, 1000uL},
	{"PXI-6220", "M", 250uL, 0, 1000uL, 1000uL},
	{"PCI-6221", "M", 250uL, 740uL, 1000uL, 1000uL},
	{"PXI-6221", "M", 250uL, 740uL, 1000uL, 1000uL},
	{"PCI-6224", "M", 250uL, 0, 1000uL, 1000uL},
	{"PXI-6224", "M", 250uL, 0, 1000uL, 1000uL},
	{"PCI-6229", "M", 250uL, 625uL, 1000uL, 1000uL},
	{"PXI-6229", "M", 250uL, 625uL, 1000uL, 1000uL},
	{0, 0, 0, 0, 0, 0},
};

//for synchronization.
static std::string g_pciClockMaster("");
static float64 g_pciClockMasterRate = 0.0;
static int g_daqmx_open_cnt;
static XMutex g_daqmx_mutex;
static std::deque<shared_ptr<XNIDAQmxInterface::XNIDAQmxRoute> > g_daqmx_sync_routes;

atomic_shared_ptr<XNIDAQmxInterface::VirtualTrigger::VirtualTriggerList>
XNIDAQmxInterface::VirtualTrigger::s_virtualTrigList;

void
XNIDAQmxInterface::VirtualTrigger::registerVirtualTrigger(const shared_ptr<VirtualTrigger> &item)
{
     for(;;) {
        atomic_shared_ptr<VirtualTriggerList> old_list(s_virtualTrigList);
        atomic_shared_ptr<VirtualTriggerList> new_list(
            old_list ? (new VirtualTriggerList(*old_list)) : (new VirtualTriggerList));
        // clean-up dead listeners.
        for(VirtualTriggerList_it it = new_list->begin(); it != new_list->end();) {
            if(!it->lock())
                it = new_list->erase(it);
            else
                it++;
        }
        new_list->push_back(item);
        if(new_list.compareAndSwap(old_list, s_virtualTrigList)) break;
    }	
}

XNIDAQmxInterface::VirtualTrigger::VirtualTrigger(const char *label, unsigned int bits)
 : m_label(label), m_bits(bits) {

}
XNIDAQmxInterface::VirtualTrigger::~VirtualTrigger() {
}
void
XNIDAQmxInterface::VirtualTrigger::start(float64 freq) {
	{
		XScopedLock<XMutex> lock(m_mutex);
		m_endOfBlank = 0;
		m_freq = freq;
	}
	onStart().talk(shared_from_this());
}

void
XNIDAQmxInterface::VirtualTrigger::stop() {
	XScopedLock<XMutex> lock(m_mutex);
	m_stamps.clear();
	disable();
}
void
XNIDAQmxInterface::VirtualTrigger::connect(uint32_t rising_edge_mask, 
	uint32_t falling_edge_mask) throw (XInterface::XInterfaceError &) {
	XScopedLock<XMutex> lock(m_mutex);
	m_stamps.clear();
	if(m_risingEdgeMask || m_fallingEdgeMask)
		throw XInterface::XInterfaceError(
			KAME::i18n("Duplicated connection to virtual trigger is not supported."), __FILE__, __LINE__);
	m_risingEdgeMask = rising_edge_mask;
	m_fallingEdgeMask = falling_edge_mask;
}
void
XNIDAQmxInterface::VirtualTrigger::disconnect() {
	XScopedLock<XMutex> lock(m_mutex);
	m_stamps.clear();
	m_risingEdgeMask = 0;
	m_fallingEdgeMask = 0;
}
uint64_t
XNIDAQmxInterface::VirtualTrigger::front(float64 _freq) {
	XScopedLock<XMutex> lock(m_mutex);
	if(m_stamps.empty())
		return 0uLL;
	uint64_t cnt = m_stamps.front();
	if(_freq > freq())
		cnt *= lrint(_freq / freq());
	else
		cnt /= lrint(freq() / _freq);
	ASSERT(cnt != 0);
	return cnt;
}
void
XNIDAQmxInterface::VirtualTrigger::pop() {
	XScopedLock<XMutex> lock(m_mutex);
	m_stamps.pop_front();
}
void
XNIDAQmxInterface::VirtualTrigger::clear(uint64_t now, float64 _freq) {
	if(_freq > freq())
		now /= lrint(_freq / freq());
	else
		now *= lrint(freq() / _freq);

	XScopedLock<XMutex> lock(m_mutex);
	if(m_stamps.size()) {
		while(m_stamps.front() <= now)
			m_stamps.pop_front();
	}
}

const char *
XNIDAQmxInterface::busArchType() const {
	int32 bus;
	DAQmxGetDevBusType(devName(), &bus);
	switch(bus) {
	case DAQmx_Val_PCI:
	case DAQmx_Val_PCIe:
		return "PCI";
	case DAQmx_Val_PXI:
//	case DAQmx_Val_PXIe:
		return "PXI";
	case DAQmx_Val_USB:
		return "USB";
	default:
		return "Unknown";
	}
}
void
XNIDAQmxInterface::synchronizeClock(TaskHandle task)
{
	const float64 rate = g_pciClockMasterRate;
	const std::string src = formatString("/%s/RTSI7", devName());

	if(devName() == g_pciClockMaster) {
/*	char buf[2048];		
		CHECK_DAQMX_RET(DAQmxGetTaskChannels(task, buf, sizeof(buf)));
	std::deque<std::string> list;
		XNIDAQmxInterface::parseList(buf, list);
		for(std::deque<std::string>::iterator it = list.begin(); it != list.end(); it++) {
			int32 ch_type;
			CHECK_DAQMX_RET(DAQmxGetChanType(task, it->c_str(), &ch_type));
			switch(ch_type) {
			case DAQmx_Val_AI:
			case DAQmx_Val_AO:
			case DAQmx_Val_DI:
			case DAQmx_Val_DO:
				break;
			case DAQmx_Val_CI:
				CHECK_DAQMX_RET(DAQmxSetCICtrTimebaseSrc(task, it->c_str(), src.c_str()));
				CHECK_DAQMX_RET(DAQmxSetCICtrTimebaseRate(task, it->c_str(), rate));
				break;
			case DAQmx_Val_CO:
				CHECK_DAQMX_RET(DAQmxSetCOCtrTimebaseSrc(task, it->c_str(), src.c_str()));
				CHECK_DAQMX_RET(DAQmxSetCOCtrTimebaseRate(task, it->c_str(), rate));
				break;
			}
		}*/
		return;
	}
	
	if(productSeries() == std::string("M")) {
		if(busArchType() == std::string("PCI")) {
			CHECK_DAQMX_RET(DAQmxSetRefClkSrc(task, src.c_str()));
			CHECK_DAQMX_RET(DAQmxSetRefClkRate(task, rate));
		}
		if(busArchType() == std::string("PXI")) {
			CHECK_DAQMX_RET(DAQmxSetRefClkSrc(task,"PXI_Clk10"));
			CHECK_DAQMX_RET(DAQmxSetRefClkRate(task, 10e6));
		}
	}
	if(productSeries() == std::string("S")) {
		if(busArchType() == std::string("PCI")) {
			CHECK_DAQMX_RET(DAQmxSetMasterTimebaseSrc(task, src.c_str()));
			CHECK_DAQMX_RET(DAQmxSetMasterTimebaseRate(task, rate));
		}
		if(busArchType() == std::string("PXI")) {
			CHECK_DAQMX_RET(DAQmxSetMasterTimebaseSrc(task,"PXI_Clk10"));
			CHECK_DAQMX_RET(DAQmxSetMasterTimebaseRate(task, 10e6));
		}
	}
}

QString
XNIDAQmxInterface::getNIDAQmxErrMessage()
{
char str[2048];
	DAQmxGetExtendedErrorInfo(str, sizeof(str));
	errno = 0;
	return QString(str);
}
QString
XNIDAQmxInterface::getNIDAQmxErrMessage(int status)
{
char str[2048];
	DAQmxGetErrorString(status, str, sizeof(str));
	errno = 0;
	return QString(str);
}
int
XNIDAQmxInterface::checkDAQmxError(int ret, const char*file, int line) {
	if(ret >= 0) return ret;
	throw XInterface::XInterfaceError(getNIDAQmxErrMessage(), file, line);
	return 0;
}

void
XNIDAQmxInterface::parseList(const char *str, std::deque<std::string> &list)
{
	list.clear();
	std::string org(str);
	const char *del = ", \t";
	for(unsigned int pos = 0; pos != std::string::npos; ) {
		unsigned int spos = org.find_first_not_of(del, pos);
		if(spos == std::string::npos) break;
		pos = org.find_first_of(del, spos);
		if(pos == std::string::npos)
			list.push_back(org.substr(spos));
		else
			list.push_back(org.substr(spos, pos - spos));
	}
}


XNIDAQmxInterface::XNIDAQmxInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver) : 
    XInterface(name, runtime, driver)
{
char buf[2048];
	CHECK_DAQMX_RET(DAQmxGetSysDevNames(buf, sizeof(buf)));
std::deque<std::string> list;
	parseList(buf, list);
	for(std::deque<std::string>::iterator it = list.begin(); it != list.end(); it++) {
		CHECK_DAQMX_RET(DAQmxGetDevProductType(it->c_str(), buf, sizeof(buf)));
		device()->add(*it + " [" + buf + "]");
	}
}
XNIDAQmxInterface::XNIDAQmxRoute::XNIDAQmxRoute(const char*src, const char*dst, int *pret)
 : m_src(src), m_dst(dst)
{
	if(pret) {
	int ret = 0;
	    ret = DAQmxConnectTerms(src, dst, DAQmx_Val_DoNotInvertPolarity);
		*pret = ret;
	}
	else {
		try {
		    CHECK_DAQMX_ERROR(DAQmxConnectTerms(src, dst, DAQmx_Val_DoNotInvertPolarity));
		    dbgPrint(QString("Connect route from %1 to %2.").arg(src).arg(dst));
		}
		catch (XInterface::XInterfaceError &e) {
			e.print();
			m_src.clear();
		}
	}
}
XNIDAQmxInterface::XNIDAQmxRoute::~XNIDAQmxRoute()
{
	if(!m_src.length()) return;
	try {
	    CHECK_DAQMX_RET(DAQmxDisconnectTerms(m_src.c_str(), m_dst.c_str()));
	    dbgPrint(QString("Disconnect route from %1 to %2.").arg(m_src).arg(m_dst));
	}
	catch (XInterface::XInterfaceError &e) {
		e.print();
	}
}
void
XNIDAQmxInterface::open() throw (XInterfaceError &)
{
char buf[256];

	XScopedLock<XMutex> lock(g_daqmx_mutex);
	if(g_daqmx_open_cnt == 0) {
//	    CHECK_DAQMX_RET(DAQmxCreateTask("", &g_task_sync_master));
	char buf[2048];		
		CHECK_DAQMX_RET(DAQmxGetSysDevNames(buf, sizeof(buf)));
	std::deque<std::string> list;
		XNIDAQmxInterface::parseList(buf, list);
		std::deque<std::string> pcidevs;
		for(std::deque<std::string>::iterator it = list.begin(); it != list.end(); it++) {
			// Device reset.
			DAQmxResetDevice(it->c_str());
			// Search for master clock among PCI(e) devices.
			int32 bus;
			DAQmxGetDevBusType(it->c_str(), &bus);
			if((bus == DAQmx_Val_PCI) || (bus == DAQmx_Val_PCIe)) {
				pcidevs.push_back(*it);
			}
		}
		if(pcidevs.size() > 1) {
			for(std::deque<std::string>::iterator it = pcidevs.begin(); it != pcidevs.end(); it++) {
				CHECK_DAQMX_RET(DAQmxGetDevProductType(it->c_str(), buf, sizeof(buf)));
				std::string type = buf;
				for(const ProductInfo *pit = sc_productInfoList; pit->type; pit++) {
					if((pit->type == type) && (pit->series == std::string("S"))) {
						//RTSI synchronizations.
						shared_ptr<XNIDAQmxInterface::XNIDAQmxRoute> route;
						if(!g_pciClockMaster.length()) {
							float64 freq = 20.0e6;
							route.reset(new XNIDAQmxInterface::XNIDAQmxRoute(
//								formatString("/%s/10MHzRefClock", it->c_str()).c_str(),
								formatString("/%s/20MHzTimebase", it->c_str()).c_str(),
								formatString("/%s/RTSI7", it->c_str()).c_str()));
							g_daqmx_sync_routes.push_back(route);
							fprintf(stderr, "20MHz Reference Clock exported from %s\n", it->c_str());
							g_pciClockMaster = *it;
							g_pciClockMasterRate = freq;
						}
					}
				}
			}
		}
	}
	g_daqmx_open_cnt++;

	if(sscanf(device()->to_str().c_str(), "%256s", buf) != 1)
          	throw XOpenInterfaceError(__FILE__, __LINE__);
	std::string devname = buf;
	CHECK_DAQMX_RET(DAQmxGetDevProductType(devname.c_str(), buf, sizeof(buf)));
	std::string type = buf;
	
	for(const ProductInfo *it = sc_productInfoList; it->type; it++) {
		if(it->type == type) {
			m_productInfo = it;
			m_devname = devname;
			return;
		}
	}
	throw XInterfaceError(KAME::i18n("No device info. for product [%1].").arg(type), __FILE__, __LINE__);
}
void
XNIDAQmxInterface::close() throw (XInterfaceError &)
{
	if(m_devname.length()) {
		m_devname.clear();

		XScopedLock<XMutex> lock(g_daqmx_mutex);
		g_daqmx_open_cnt--;
		if(g_daqmx_open_cnt == 0) {
			g_daqmx_sync_routes.clear();
		}
	}
}
#endif //HAVE_NI_DAQMX
