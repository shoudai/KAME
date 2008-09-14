/***************************************************************************
		Copyright (C) 2002-2008 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
//---------------------------------------------------------------------------
#include "nmrspectrumbase.h"
#include "nmrpulse.h"

#include <graph.h>
#include <graphwidget.h>
#include <xwavengraph.h>

#include <qpushbutton.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <kapplication.h>
#include <knuminput.h>
#include <kiconloader.h>
//---------------------------------------------------------------------------
template <class FRM>
XNMRSpectrumBase<FRM>::XNMRSpectrumBase(const char *name, bool runtime,
						   const shared_ptr<XScalarEntryList> &scalarentries,
						   const shared_ptr<XInterfaceList> &interfaces,
						   const shared_ptr<XThermometerList> &thermometers,
						   const shared_ptr<XDriverList> &drivers)
	: XSecondaryDriver(name, runtime, scalarentries, interfaces, thermometers, drivers),
	  m_pulse(create<XItemNode<XDriverList, XNMRPulseAnalyzer> >("PulseAnalyzer", false, drivers, true)),
	  m_bandWidth(create<XDoubleNode>("BandWidth", false)),
	  m_autoPhase(create<XBoolNode>("AutoPhase", false)),
	  m_phase(create<XDoubleNode>("Phase", false, "%.2f")),
	  m_clear(create<XNode>("Clear", true)),
	  m_solverList(create<XComboNode>("SpectrumSolver", false, true)),
	  m_windowFunc(create<XComboNode>("WindowFunc", false, true)),
	  m_windowWidth(create<XDoubleNode>("WindowWidth", false)),
	  m_form(new FRM(g_pFrmMain)),
	  m_statusPrinter(XStatusPrinter::create(m_form.get())),
	  m_spectrum(create<XWaveNGraph>("Spectrum", true,
									 m_form->m_graph, m_form->m_urlDump, m_form->m_btnDump)),
	  m_solver(create<SpectrumSolverWrapper>("SpectrumSolver", true, m_solverList, m_windowFunc, m_windowWidth)) {
    m_form->m_btnClear->setIconSet(
		KApplication::kApplication()->iconLoader()->loadIconSet("editdelete", 
																KIcon::Toolbar, KIcon::SizeSmall, true ) );      
    
	connect(pulse());

	{
		const char *labels[] = {"X", "Re [V]", "Im [V]", "Weights", "Abs [V]"};
		m_spectrum->setColCount(5, labels);
		m_spectrum->insertPlot(labels[4], 0, 4, -1, 3);
		m_spectrum->insertPlot(labels[1], 0, 1, -1, 3);
		m_spectrum->insertPlot(labels[2], 0, 2, -1, 3);
		m_spectrum->axisy()->label()->value(KAME::i18n("Intens. [V]"));
		m_spectrum->plot(1)->label()->value(KAME::i18n("real part"));
		m_spectrum->plot(1)->drawPoints()->value(false);
		m_spectrum->plot(1)->lineColor()->value(clRed);
		m_spectrum->plot(2)->label()->value(KAME::i18n("imag. part"));
		m_spectrum->plot(2)->drawPoints()->value(false);
		m_spectrum->plot(2)->lineColor()->value(clGreen);
		m_spectrum->plot(0)->label()->value(KAME::i18n("abs."));
		m_spectrum->plot(0)->drawPoints()->value(false);
		m_spectrum->plot(0)->drawLines()->value(true);
		m_spectrum->plot(0)->drawBars()->value(true);
		m_spectrum->plot(0)->barColor()->value(QColor(0x60, 0x60, 0xc0).rgb());
		m_spectrum->plot(0)->lineColor()->value(QColor(0x60, 0x60, 0xc0).rgb());
		m_spectrum->plot(0)->intensity()->value(0.5);
		m_spectrum->clear();
		{
			shared_ptr<XXYPlot> plot = m_spectrum->graph()->plots()->create<XXYPlot>(
				"Peaks", true, m_spectrum->graph());
			m_peakPlot = plot;
			plot->label()->value(KAME::i18n("Peaks"));
			plot->axisX()->value(m_spectrum->axisx());
			plot->axisY()->value(m_spectrum->axisy());
			plot->drawPoints()->value(false);
			plot->drawLines()->value(false);
			plot->drawBars()->value(true);
			plot->intensity()->value(2.0);
			plot->displayMajorGrid()->value(false);
			plot->pointColor()->value(QColor(0xa0, 0x00, 0xa0).rgb());
			plot->barColor()->value(QColor(0xa0, 0x00, 0xa0).rgb());
			plot->clearPoints()->setUIEnabled(false);
			plot->maxCount()->setUIEnabled(false);
		}
	}
  
	bandWidth()->value(50);
	autoPhase()->value(true);
	
	windowFunc()->str(std::string(SpectrumSolverWrapper::WINDOW_FUNC_DEFAULT));
	windowWidth()->value(100.0);

	m_lsnOnClear = m_clear->onTouch().connectWeak(
		shared_from_this(), &XNMRSpectrumBase<FRM>::onClear);
	m_lsnOnCondChanged = bandWidth()->onValueChanged().connectWeak(
		shared_from_this(), &XNMRSpectrumBase<FRM>::onCondChanged);
	autoPhase()->onValueChanged().connect(m_lsnOnCondChanged);
	phase()->onValueChanged().connect(m_lsnOnCondChanged);
	solverList()->onValueChanged().connect(m_lsnOnCondChanged);
	windowWidth()->onValueChanged().connect(m_lsnOnCondChanged);
	windowFunc()->onValueChanged().connect(m_lsnOnCondChanged);

	m_conBandWidth = xqcon_create<XQLineEditConnector>(m_bandWidth, m_form->m_edBW);
	m_conPhase = xqcon_create<XKDoubleNumInputConnector>(m_phase, m_form->m_numPhase);
	m_form->m_numPhase->setRange(-360.0, 360.0, 1.0, true);
	m_conAutoPhase = xqcon_create<XQToggleButtonConnector>(m_autoPhase, m_form->m_ckbAutoPhase);
	m_conPulse = xqcon_create<XQComboBoxConnector>(m_pulse, m_form->m_cmbPulse);
	m_conClear = xqcon_create<XQButtonConnector>(m_clear, m_form->m_btnClear);
	m_conSolverList = xqcon_create<XQComboBoxConnector>(m_solverList, m_form->m_cmbSolver);
	m_conWindowWidth = xqcon_create<XKDoubleNumInputConnector>(m_windowWidth,
		m_form->m_numWindowWidth);
	m_form->m_numWindowWidth->setRange(0.1, 200.0, 1.0, true);
	m_conWindowFunc = xqcon_create<XQComboBoxConnector>(m_windowFunc,
		m_form->m_cmbWindowFunc);
}
template <class FRM>
XNMRSpectrumBase<FRM>::~XNMRSpectrumBase() {
}
template <class FRM>
void
XNMRSpectrumBase<FRM>::showForms()
{
	m_form->show();
	m_form->raise();
}
template <class FRM>
void
XNMRSpectrumBase<FRM>::onCondChanged(const shared_ptr<XValueNodeBase> &node)
{
    if((node == phase()) && *autoPhase()) return;
	if(onCondChangedImpl(node))
        m_timeClearRequested = XTime::now();
    requestAnalysis();
}
template <class FRM>
void
XNMRSpectrumBase<FRM>::onClear(const shared_ptr<XNode> &)
{
    m_timeClearRequested = XTime::now();
    requestAnalysis();
}
template <class FRM>
bool
XNMRSpectrumBase<FRM>::checkDependency(const shared_ptr<XDriver> &emitter) const {
    shared_ptr<XNMRPulseAnalyzer> _pulse = *pulse();
    if(!_pulse) return false;
    if(emitter == shared_from_this()) return true;
    return (emitter == _pulse) && checkDependencyImpl(emitter);
}
template <class FRM>
void
XNMRSpectrumBase<FRM>::analyze(const shared_ptr<XDriver> &emitter) throw (XRecordError&)
{
	shared_ptr<XNMRPulseAnalyzer> _pulse = *pulse();
	ASSERT( _pulse );
 
	if(*_pulse->exAvgIncr()) {
		m_statusPrinter->printWarning(KAME::i18n("Do NOT use incremental avg. Skipping."));
		throw XSkippedRecordError(__FILE__, __LINE__);
	}

	bool clear = (m_timeClearRequested > _pulse->timeAwared());
  
	double interval = _pulse->interval();
	double df = _pulse->dFreq();
	
	double res = getFreqResHint();
	res = df * std::max(1L, lrint(res / df - 0.5));
	
	double _max = getMaxFreq();
	double _min = getMinFreq();
	
	if(_max <= _min) {
		throw XSkippedRecordError(KAME::i18n("Invalid min. and max."), __FILE__, __LINE__);
	}
	if(res * 65536 * 2 < _max - _min) {
		throw XSkippedRecordError(KAME::i18n("Too small resolution."), __FILE__, __LINE__);
	}

	if(fabs(log(resRecorded() / res)) < log(2.0))
		res = resRecorded();
	
	if((resRecorded() != res) || clear) {
		m_resRecorded = res;
		m_accum.clear();
		m_weights.clear();
	}
	else {
		int diff = lrint(minRecorded() / res) - lrint(_min / res);
		for(int i = 0; i < diff; i++) {
			m_accum.push_front(0.0);
			m_weights.push_front(0);
		}
		for(int i = 0; i < -diff; i++) {
			if(!m_accum.empty()) {
				m_accum.pop_front();
				m_weights.pop_front();
			}
		}
	}
	m_minRecorded = _min;
	int length = lrint((_max - _min) / res);
	m_accum.resize(length, 0.0);
	m_weights.resize(length, 0);
	m_wave.resize(length);
	std::fill(m_wave.begin(), m_wave.end(), 0.0);

	if(clear) {
		m_spectrum->clear();
		m_peaks.clear();
		throw XSkippedRecordError(__FILE__, __LINE__);
	}

	if(emitter == _pulse) {
		fssum();
		afterFSSum();
	}
	
	analyzeIFT();
	if(*autoPhase()) {
		std::complex<double> csum(0.0, 0.0);
		for(unsigned int i = 0; i < wave().size(); i++) {
			csum += wave()[i] * weights()[i];
		}
		double ph = 180.0 / M_PI * atan2(std::imag(csum), std::real(csum));
		if(fabs(ph) < 180.0)
			phase()->value(ph);
	}
	double ph = *phase() / 180.0 * M_PI;
	std::complex<double> cph = std::polar(1.0, -ph);
	for(unsigned int i = 0; i < wave().size(); i++) {
		m_wave[i] *= cph;
	}
}
template <class FRM>
void
XNMRSpectrumBase<FRM>::visualize()
{
	if(!time()) {
		m_spectrum->clear();
		m_peakPlot->maxCount()->value(0);
		return;
	}

	int length = wave().size();
	std::vector<double> values;
	getValues(values);
	ASSERT(values.size() == length);
	{   XScopedWriteLock<XWaveNGraph> lock(*m_spectrum);
		m_spectrum->setRowCount(length);
		for(int i = 0; i < length; i++) {
			m_spectrum->cols(0)[i] = values[i];
			m_spectrum->cols(1)[i] = std::real(wave()[i]);
			m_spectrum->cols(2)[i] = std::imag(wave()[i]);
			m_spectrum->cols(3)[i] = weights()[i];
			m_spectrum->cols(4)[i] = std::abs(wave()[i]);
		}
		m_peakPlot->maxCount()->value(m_peaks.size());
		std::deque<XGraph::ValPoint> &points(m_peakPlot->points());
		points.resize(m_peaks.size());
		for(int i = 0; i < m_peaks.size(); i++) {
			double x = m_peaks[i].second;
			int j = lrint(x - 0.5);
			j = std::min(std::max(0, j), length - 2);
			double a = values[j] + (values[j + 1] - values[j]) * (x - j);
			points[i] = XGraph::ValPoint(a, m_peaks[i].first);
		}
	}
}

template <class FRM>
void
XNMRSpectrumBase<FRM>::fssum()
{
	shared_ptr<XNMRPulseAnalyzer> _pulse = *pulse();

	int len = _pulse->ftWidth();
	double df = _pulse->dFreq();
	if((len == 0) || (df == 0)) {
		throw XRecordError(KAME::i18n("Invalid waveform."), __FILE__, __LINE__);  
	}
	//bw *= 1.8; // for Hamming.
	//	bw *= 3.6; // for FlatTop.
	//bw *= 2.2; // for Kaiser3.
	int bw = abs(lrint(*bandWidth() * 1000.0 / df * 2.2));
	if(bw >= len) {
		throw XRecordError(KAME::i18n("BW beyond Nyquist freq."), __FILE__, __LINE__);  
	}
	double cfreq = getCurrentCenterFreq();
	if(cfreq == 0) {
		throw XRecordError(KAME::i18n("Invalid center freq."), __FILE__, __LINE__);  
	}
	std::vector<std::complex<double> > ftwave(len);
	m_preFFT.exec(_pulse->wave(), ftwave, -_pulse->waveFTPos(), 0.0, &FFT::windowFuncRect, 1.0);
	for(int i = -bw / 2; i <= bw / 2; i++) {
		double freq = i * df;
		int idx = lrint((cfreq + freq - minRecorded()) / resRecorded());
		if((idx >= (int)m_accum.size()) || (idx < 0))
			continue;
		double w = FFT::windowFuncKaiser1((double)i / bw);
		m_accum[idx] += ftwave[(i + len) % len] * w / (double)_pulse->wave().size();
		m_weights[idx] += w;
	}
}
template <class FRM>
void
XNMRSpectrumBase<FRM>::analyzeIFT() {
	double th = FFT::windowFuncHamming(0.49);
	int max_idx = 0;
	int min_idx = m_accum.size() - 1;
	int taps_max = 0; 
	for(int i = 0; i < m_accum.size(); i++) {
		if(weights()[i] > th) {
			min_idx = std::min(min_idx, i);
			max_idx = std::max(max_idx, i);
			taps_max++;
		}
	}
	if(max_idx <= min_idx)
		throw XSkippedRecordError(__FILE__, __LINE__);
	shared_ptr<XNMRPulseAnalyzer> _pulse = *pulse();
	double res = resRecorded();
	int iftlen = max_idx - min_idx + 1;
	int npad = lrint(6.0 / (res * _pulse->waveWidth() * _pulse->interval()) + 0.5); //# of pads in frequency domain.
	//Truncation factor for IFFT.
	int trunc2 = lrint(pow(2.0, ceil(log(iftlen * 0.03) / log(2.0))));
	if(trunc2 < 1)
		throw XSkippedRecordError(__FILE__, __LINE__);
	iftlen = ((iftlen * 3 / 2 + npad) / trunc2 + 1) * trunc2;
	int tdsize = lrint(_pulse->waveWidth() * _pulse->interval() * res * iftlen);
	int iftorigin = lrint(_pulse->waveFTPos() * _pulse->interval() * res * iftlen);
	int bwinv = abs(lrint(1.0/(*bandWidth() * 1000.0 * _pulse->interval() * res * iftlen)));
	if(abs(iftorigin) > iftlen/2)
		throw XSkippedRecordError(__FILE__, __LINE__);
	
	if(!m_ift || (m_ift->length() != iftlen)) {
		m_ift.reset(new FFT(1, iftlen));
	}
	
	std::vector<std::complex<double> > fftwave(iftlen), iftwave(iftlen);
	std::fill(fftwave.begin(), fftwave.end(), 0.0);
	for(int i = min_idx; i <= max_idx; i++) {
		int k = (i - (max_idx + min_idx) / 2 + iftlen) % iftlen;
		if(weights()[i] > th)
			fftwave[k] = m_accum[i] / m_weights[i];
	}
	m_ift->exec(fftwave, iftwave);
	
	shared_ptr<SpectrumSolver> solver = m_solver->solver();
	std::vector<std::complex<double> > solverin;
	double windowwidth = *windowWidth() / 100.0;
	if(solver->isFT()) {
		solverin.resize(iftlen);
		double wlen = SpectrumSolver::windowLength(tdsize, -iftorigin, windowwidth);
		wlen += bwinv * 2; //effect of convolution.
		windowwidth = wlen / solverin.size();
		iftorigin = solverin.size()/2;
	}
	else
		solverin.resize(tdsize);		
	for(int i = 0; i < (int)solverin.size(); i++) {
		int k = (-iftorigin + i + iftlen) % iftlen;
		ASSERT(k >= 0);
		solverin[i] = iftwave[k];
	}
	try {
		solver->exec(solverin, fftwave, -iftorigin, 0.1e-2, m_solver->windowFunc(), windowwidth);
	}
	catch (XKameError &e) {
		throw XSkippedRecordError(e.msg(), __FILE__, __LINE__);
	}

	for(int i = min_idx; i <= max_idx; i++) {
		int k = (i - (max_idx + min_idx) / 2 + iftlen) % iftlen;
		m_wave[i] = fftwave[k] / (double)iftlen;
	}
	th = FFT::windowFuncHamming(0.1);
	for(int i = 0; i < m_accum.size(); i++) {
		if(weights()[i] < th) {
			m_weights[i] = 0.0;
		}
	}
	m_peaks.clear();
	for(int i = 0; i < solver->peaks().size(); i++) {
		double k = solver->peaks()[i].second;
		double j = (k > iftlen/2) ? (k - iftlen) : k;
		j += (max_idx + min_idx) / 2;
		int l = lrint(j);
		if((l >= 0) && (l < m_accum.size()) && (weights()[l]))
			m_peaks.push_back(std::pair<double, double>(
				solver->peaks()[i].first / (double)iftlen, j));
	}
}
