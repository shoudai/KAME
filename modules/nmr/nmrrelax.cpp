/***************************************************************************
		Copyright (C) 2002-2015 Kentaro Kitagawa
		                   kitagawa@phys.s.u-tokyo.ac.jp

		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.

		You should have received a copy of the GNU Library General
		Public License and a list of authors along with this program;
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "nmrrelax.h"
#include "nmrrelaxfit.h"
#include "ui_nmrrelaxform.h"
#include "nmrpulse.h"
#include "pulserdriver.h"
#include "analyzer.h"
#include "graph.h"
#include "graphwidget.h"
#include "rand.h"

REGISTER_TYPE(XDriverList, NMRT1, "NMR relaxation measurement");

#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>

const char XNMRT1::P1DIST_LINEAR[] = "Linear";
const char XNMRT1::P1DIST_LOG[] = "Log";
const char XNMRT1::P1DIST_RECIPROCAL[] = "Reciprocal";

const char XNMRT1::P1STRATEGY_RANDOM[] = "Random";
const char XNMRT1::P1STRATEGY_FLATTEN[] = "Flatten";


class XRelaxFuncPlot : public XFuncPlot {
public:
	XRelaxFuncPlot(const char *name, bool runtime, Transaction &tr, const shared_ptr<XGraph> &graph
				   , const shared_ptr<XItemNode < XRelaxFuncList, XRelaxFunc > >  &item,
				   const shared_ptr<XNMRT1> &owner)
		: XFuncPlot(name, runtime, tr, graph), m_item(item), m_owner(owner)
	{}
	~XRelaxFuncPlot() {}
	virtual double func(double t) const {
		shared_ptr<XNMRT1> owner = m_owner.lock();
		if( !owner) return 0;
		Snapshot shot( *owner);
		shared_ptr<XRelaxFunc> func1 = shot[ *m_item];
		if( !func1) return 0;
		double f, df;
		double it1 = shot[ *owner].m_params[0];
		double c = shot[ *owner].m_params[1];
		double a = shot[ *owner].m_params[2];
		func1->relax( &f, &df, t, it1);
		return c * f + a;
	}
private:
	shared_ptr<XItemNode < XRelaxFuncList, XRelaxFunc > > m_item;
	weak_ptr<XNMRT1> m_owner;
};

XNMRT1::XNMRT1(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas)
	: XSecondaryDriver(name, runtime, ref(tr_meas), meas),
	  m_relaxFuncs(create<XRelaxFuncList>("RelaxFuncs", true)),
	  m_t1inv(create<XScalarEntry>("T1inv", false,
								   dynamic_pointer_cast<XDriver>(shared_from_this()))),
	  m_t1invErr(create<XScalarEntry>("T1invErr", false,
									  dynamic_pointer_cast<XDriver>(shared_from_this()))),
	  m_pulser(create<XItemNode < XDriverList, XPulser > >(
		  "Pulser", false, ref(tr_meas), meas->drivers(), true)),
	  m_pulse1(create<XItemNode < XDriverList, XNMRPulseAnalyzer > >(
		  "NMRPulseAnalyzer1", false, ref(tr_meas), meas->drivers(), true)),
	  m_pulse2(create<XItemNode < XDriverList, XNMRPulseAnalyzer > >(
		  "NMRPulseAnalyzer2", false, ref(tr_meas), meas->drivers())),
	  m_active(create<XBoolNode>("Active", false)),
	  m_autoPhase(create<XBoolNode>("AutoPhase", false)),
	  m_mInftyFit(create<XBoolNode>("MInftyFit", false)),
	  m_absFit(create<XBoolNode>("AbsFit", false)),
      m_trackPeak(create<XBoolNode>("TrackPeakFreq", false)),
      m_p1Min(create<XDoubleNode>("P1Min", false)),
	  m_p1Max(create<XDoubleNode>("P1Max", false)),
	  m_p1Next(create<XDoubleNode>("P1Next", true)),
	  m_p1AltNext(create<XDoubleNode>("P1Next", true)),
	  m_phase(create<XDoubleNode>("Phase", false, "%.2f")),
      m_freq(create<XDoubleNode>("Freq", false, "%.3f")),
	  m_windowFunc(create<XComboNode>("WindowFunc", false, true)),
	  m_autoWindow(create<XBoolNode>("AutoWindow", false)),
	  m_windowWidth(create<XComboNode>("WindowWidth", false, true)),
	  m_mode(create<XComboNode>("Mode", false, true)),
	  m_smoothSamples(create<XUIntNode>("SmoothSamples", false)),
	  m_p1Strategy(create<XComboNode>("P1Strategy", false, true)),
	  m_p1Dist(create<XComboNode>("P1Dist", false, true)),
	  m_resetFit(create<XTouchableNode>("ResetFit", true)),
	  m_clearAll(create<XTouchableNode>("ClearAll", true)),
	  m_fitStatus(create<XStringNode>("FitStatus", true)),
	  m_solver(create<SpectrumSolverWrapper>("SpectrumSolver", true, shared_ptr<XComboNode>(), m_windowFunc, shared_ptr<XDoubleNode>())),
      m_form(new FrmNMRT1),
	  m_statusPrinter(XStatusPrinter::create(m_form.get())),
      m_wave(create<XWaveNGraph>("Wave", true, m_form->m_graph, m_form->m_edDump, m_form->m_tbDump, m_form->m_btnDump)) {

    iterate_commit([=](Transaction &tr){
		m_relaxFunc = create<XItemNode < XRelaxFuncList, XRelaxFunc > >(
						  tr, "RelaxFunc", false, tr, m_relaxFuncs, true);
    });

    m_form->m_btnClear->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogResetButton));
    m_form->m_btnResetFit->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));

    m_form->setWindowTitle(i18n("NMR Relaxation Measurement - ") + getLabel() );

    meas->scalarEntries()->insert(tr_meas, t1inv());
    meas->scalarEntries()->insert(tr_meas, t1invErr());

    connect(pulser());
    connect(pulse1());
    connect(pulse2());

    iterate_commit([=](Transaction &tr){
	    tr[ *m_windowFunc] = SpectrumSolverWrapper::WINDOW_FUNC_HAMMING;
        m_windowWidthList = {0.25, 0.5, 1.0, 1.5, 2.0};
        tr[ *m_windowWidth].add({"25%", "50%", "100%", "150%", "200%"});
	    tr[ *m_windowWidth] = 2;

	    const char *labels[] = {"P1 [ms] or 2Tau [us]", "Intens [V]",
								"Weight [1/V]", "Abs [V]", "Re [V]", "Im [V]"};
		tr[ *m_wave].setColCount(6, labels);
		tr[ *m_wave].insertPlot(i18n("Relaxation"), 0, 1, -1, 2);
		tr[ *m_wave].insertPlot(i18n("Out-of-Phase"), 0, 5, -1, 2);
		shared_ptr<XAxis> axisx = tr[ *m_wave].axisx();
		shared_ptr<XAxis> axisy = tr[ *m_wave].axisy();
		tr[ *axisx->logScale()] = true;
		tr[ *axisy->label()] = i18n("Relaxation");
		tr[ *tr[ *m_wave].plot(0)->drawLines()] = false;
		tr[ *tr[ *m_wave].plot(1)->drawLines()] = false;
        tr[ *tr[ *m_wave].plot(1)->intensity()] = 1.0;
		shared_ptr<XFuncPlot> plot3 = m_wave->graph()->plots()->create<XRelaxFuncPlot>(
			tr, "FittedCurve", true, tr, m_wave->graph(),
			relaxFunc(), static_pointer_cast<XNMRT1>(shared_from_this()));
		tr[ *plot3->label()] = i18n("Fitted Curve");
		tr[ *plot3->axisX()] = axisx;
		tr[ *plot3->axisY()] = axisy;
		tr[ *m_wave].clearPoints();

        tr[ *mode()].add({"T1 Measurement", "T2 Measurement", "St.E. Measurement"});
		tr[ *mode()] = MEAS_T1;

        tr[ *p1Strategy()].add({P1STRATEGY_RANDOM, P1STRATEGY_FLATTEN});
		tr[ *p1Strategy()] = 1;

        tr[ *p1Dist()].add({P1DIST_LINEAR, P1DIST_LOG, P1DIST_RECIPROCAL});
		tr[ *p1Dist()] = 1;

		tr[ *relaxFunc()].str(XString("NMR I=1/2"));
		tr[ *p1Min()] = 1.0;
		tr[ *p1Max()] = 100.0;
		tr[ *autoPhase()] = true;
		tr[ *autoWindow()] = true;
		tr[ *mInftyFit()] = true;
        tr[ *trackPeak()] = false;
        tr[ *smoothSamples()] = 40;
    });

    //Ranges should be preset in prior to connectors.
    m_form->m_dblPhase->setRange( -360.0, 360.0);
    m_form->m_dblPhase->setSingleStep(10.0);

    m_conUIs = {
        xqcon_create<XQLineEditConnector>(m_p1Min, m_form->m_edP1Min),
        xqcon_create<XQLineEditConnector>(m_p1Max, m_form->m_edP1Max),
        xqcon_create<XQLineEditConnector>(m_p1Next, m_form->m_edP1Next),
        xqcon_create<XQDoubleSpinBoxConnector>(m_phase, m_form->m_dblPhase, m_form->m_slPhase),
        xqcon_create<XQLineEditConnector>(m_freq, m_form->m_edFreq),
        xqcon_create<XQToggleButtonConnector>(m_autoWindow, m_form->m_ckbAutoWindow),
        xqcon_create<XQComboBoxConnector>(m_windowFunc, m_form->m_cmbWindowFunc, Snapshot( *m_windowFunc)),
        xqcon_create<XQComboBoxConnector>(m_windowWidth, m_form->m_cmbWindowWidth, Snapshot( *m_windowWidth)),
        xqcon_create<XQLineEditConnector>(m_smoothSamples, m_form->m_edSmoothSamples),
        xqcon_create<XQComboBoxConnector>(m_p1Strategy, m_form->m_cmbP1Strategy, Snapshot( *m_p1Strategy)),
        xqcon_create<XQComboBoxConnector>(m_p1Dist, m_form->m_cmbP1Dist, Snapshot( *m_p1Dist)),
        xqcon_create<XQButtonConnector>(m_clearAll, m_form->m_btnClear),
        xqcon_create<XQButtonConnector>(m_resetFit, m_form->m_btnResetFit),
        xqcon_create<XQToggleButtonConnector>(m_active, m_form->m_ckbActive),
        xqcon_create<XQToggleButtonConnector>(m_autoPhase, m_form->m_ckbAutoPhase),
        xqcon_create<XQToggleButtonConnector>(m_mInftyFit, m_form->m_ckbMInftyFit),
        xqcon_create<XQToggleButtonConnector>(m_absFit, m_form->m_ckbAbsFit),
        xqcon_create<XQToggleButtonConnector>(m_trackPeak, m_form->m_ckbTrackPeak),
        xqcon_create<XQTextBrowserConnector>(m_fitStatus, m_form->m_txtFitStatus),
        xqcon_create<XQComboBoxConnector>(m_relaxFunc, m_form->m_cmbRelaxFunc, Snapshot( *m_relaxFuncs)),
        xqcon_create<XQComboBoxConnector>(m_mode, m_form->m_cmbMode, Snapshot( *m_mode)),
        xqcon_create<XQComboBoxConnector>(m_pulser, m_form->m_cmbPulser, ref(tr_meas)),
        xqcon_create<XQComboBoxConnector>(m_pulse1, m_form->m_cmbPulse1, ref(tr_meas)),
        xqcon_create<XQComboBoxConnector>(m_pulse2, m_form->m_cmbPulse2, ref(tr_meas))
    };

    iterate_commit([=](Transaction &tr){
		m_lsnOnActiveChanged = tr[ *active()].onValueChanged().connectWeakly(
			shared_from_this(), &XNMRT1::onActiveChanged);
		m_lsnOnP1CondChanged = tr[ *p1Max()].onValueChanged().connectWeakly(
			shared_from_this(), &XNMRT1::onP1CondChanged);
        for(auto &&x: std::vector<shared_ptr<XValueNodeBase>>(
            {p1Min(), p1Strategy(), p1Dist(), smoothSamples()}))
            tr[ *x].onValueChanged().connect(m_lsnOnP1CondChanged);
		m_lsnOnCondChanged = tr[ *phase()].onValueChanged().connectWeakly(
			shared_from_this(), &XNMRT1::onCondChanged);
        for(auto &&x: std::vector<shared_ptr<XValueNodeBase>>(
            {mInftyFit(), absFit(), relaxFunc(), autoPhase(), freq(), autoWindow(),
            windowFunc(), windowWidth(), mode()}))
            tr[ *x].onValueChanged().connect(m_lsnOnCondChanged);

		m_lsnOnClearAll = tr[ *m_clearAll].onTouch().connectWeakly(
			shared_from_this(), &XNMRT1::onClearAll);
		m_lsnOnResetFit = tr[ *m_resetFit].onTouch().connectWeakly(
			shared_from_this(), &XNMRT1::onResetFit);
    });
}
void
XNMRT1::showForms() {
    m_form->showNormal();
	m_form->raise();
}

void
XNMRT1::onClearAll(const Snapshot &shot, XTouchableNode *) {
    trans( *this).m_timeClearRequested = XTime::now();
    requestAnalysis();
}
double
XNMRT1::distributeP1(const Snapshot &shot, double uniform_x_0_to_1) {
	double p1min = shot[ *p1Min()];
	double p1max = shot[ *p1Max()];
	double p1;
	if(shot[ *p1Dist()].to_str() == P1DIST_LINEAR)
		p1 = (1-(uniform_x_0_to_1)) * p1min + (uniform_x_0_to_1) * p1max;
	else if(shot[ *p1Dist()].to_str() == P1DIST_LOG)
		p1 = p1min * exp((uniform_x_0_to_1) * log(p1max/p1min));
	else
		//P1DIST_RECIPROCAL
		p1 =1/((1-uniform_x_0_to_1)/p1min + (uniform_x_0_to_1)/p1max);

	p1 = llrint(p1 * 1e4) / 1e4;//rounds.
	return p1;
}
void
XNMRT1::onResetFit(const Snapshot &shot, XTouchableNode *) {
    iterate_commit([=](Transaction &tr){
		const Snapshot &shot(tr);
		double x = randMT19937();
		double p1min = shot[ *p1Min()];
		double p1max = shot[ *p1Max()];
		if((p1min <= 0) || (p1min >= p1max)) {
	      	gErrPrint(i18n("Invalid P1Min or P1Max."));
	      	return;
		}
		tr[ *this].m_params[0] = 1.0 / distributeP1(shot, x);
		tr[ *this].m_params[1] = 0.1;
		tr[ *this].m_params[2] = 0.0;
    });
	requestAnalysis();
}
void
XNMRT1::obtainNextP1(Transaction &tr) {
	const Snapshot &shot(tr);
	double x_0_to_1;
	if(shot[ *p1Strategy()] .to_str() == P1STRATEGY_RANDOM) {
		x_0_to_1 = randMT19937();
	}
	else {
	//FLATTEN
		int samples = shot[ *this].m_sumpts.size();
		if( !samples) {
			x_0_to_1 = 0.5;
		}
		else {
			//binary search for area having small sum isigma.
			double p1min = shot[ *p1Min()];
			double p1max = shot[ *p1Max()];
			int lb = 0, ub = samples;
			double k_0 = samples / log(p1max/p1min);
			int idx_p1next = lrint(log(shot[ *p1Next()] / p1min) * k_0);
			idx_p1next = std::min(std::max(idx_p1next, 0), samples);
			bool p1dist_linear = (shot[ *p1Dist()].to_str() == P1DIST_LINEAR);
			bool p1dist_log = (shot[ *p1Dist()].to_str() == P1DIST_LOG);
			const auto &sumpts = shot[ *this].m_sumpts;
			for(;;) {
				int mid;
				if(p1dist_log)
					mid = (lb + ub) / 2;  //m_sumpts has been stored in log scale.
				else {
					double xlb = exp(lb / k_0) * p1min;
					double xub = exp(ub / k_0) * p1min;
					double xhalf;
					if(p1dist_linear)
						xhalf = (xlb + xub) / 2;
					else
						xhalf = 1.0/((1/xlb + 1/xub) / 2); //reciprocal
					mid = lrint(log(xhalf / p1min) * k_0);
				}
				assert((mid >= lb) && (mid <= ub));
				int isigma_0 = 0;
				for(int idx = lb; idx < mid; ++idx)
					isigma_0 += sumpts[idx].isigma;
				int isigma_1 = 0;
				for(int idx = mid; idx < ub; ++idx)
					isigma_1 += sumpts[idx].isigma;
				if((lb <= idx_p1next) && (mid > idx_p1next))
					isigma_0++;
				if((mid <= idx_p1next) && (ub > idx_p1next))
					isigma_1++;
				if(isigma_0 == isigma_1) {
					if(randMT19937() < 0.5)
						ub = mid;
					else
						lb = mid;
				}
				else {
					if(isigma_0 < isigma_1)
						ub = mid;
					else
						lb = mid;
				}
				if(ub - lb <= 1) {
					x_0_to_1 = (double)lb / samples;
					break;
				}
			}
		}
	}
	tr[ *p1Next()] = distributeP1(shot, x_0_to_1);
    tr[ *p1AltNext()] = distributeP1(shot, 1 - x_0_to_1);
}
void
XNMRT1::onP1CondChanged(const Snapshot &shot, XValueNodeBase *node) {
	requestAnalysis();
    iterate_commit([=](Transaction &tr){
		const Snapshot &shot(tr);
		double p1min = shot[ *p1Min()];
		double p1max = shot[ *p1Max()];
		if((p1min <= 0) || (p1min >= p1max)) {
	      	gErrPrint(i18n("Invalid P1Min or P1Max."));
	      	return;
		}
		obtainNextP1(tr);
    });
}
void
XNMRT1::onCondChanged(const Snapshot &shot, XValueNodeBase *node) {
//    if((node == phase()) && *autoPhase()) return;
//    if(((node == windowWidth()) || (node == windowFunc())) && *autoWindow()) return;
    if(
		(node == mode().get()) ||
		(node == freq().get())) {
        trans( *this).m_timeClearRequested = XTime::now();
    }
	requestAnalysis();
}
void
XNMRT1::analyzeSpectrum(Transaction &tr,
	const std::vector< std::complex<double> >&wave, int origin, double cf,
	std::deque<std::complex<double> > &value_by_cond) {
	const Snapshot &shot_this(tr);

	value_by_cond.clear();
	std::deque<FFT::twindowfunc> funcs;
	m_solver->windowFuncs(funcs);

	int idx = 0;
	for(std::deque<double>::const_iterator wit = m_windowWidthList.begin(); wit != m_windowWidthList.end(); wit++) {
		for(std::deque<FFT::twindowfunc>::iterator fit = funcs.begin(); fit != funcs.end(); fit++) {
			if(shot_this[ *this].m_convolutionCache.size() <= idx) {
				tr[ *this].m_convolutionCache.push_back(
                    std::make_shared<Payload::ConvolutionCache>());
			}
			shared_ptr<Payload::ConvolutionCache> cache = tr[ *this].m_convolutionCache[idx];
			if((cache->windowwidth != *wit) || (cache->origin != origin) ||
				(cache->windowfunc != *fit) || (cache->cfreq != cf) ||
				(cache->wave.size() != wave.size())) {
				cache->windowwidth = *wit;
				cache->origin = origin;
				cache->windowfunc = *fit;
				cache->cfreq = cf;
				cache->power = 0.0;
				cache->wave.resize(wave.size());
				double wk = 1.0 / FFTSolver::windowLength(wave.size(), -origin, *wit);
				for(int i = 0; i < (int)wave.size(); i++) {
					double w = ( *fit)((i - origin) * wk) / (double)wave.size();
					cache->wave[i] = std::polar(w, -2.0*M_PI*cf*(i - origin));
					cache->power += w*w;
				}
			}

			std::complex<double> z(0.0);
			for(int i = 0; i < (int)cache->wave.size(); i++) {
				z += wave[i] * cache->wave[i];
			}

//			m_solver->solver()->exec(wave, fftout, -origin, 0.0, *fit, *wit);
//			value_by_cond.push_back(fftout[(cf + fftlen) % fftlen]);
			value_by_cond.push_back(z);
			idx++;
		}
	}
}
bool
XNMRT1::checkDependency(const Snapshot &shot_this,
	const Snapshot &shot_emitter, const Snapshot &shot_others,
	XDriver *emitter) const {
    shared_ptr<XPulser> pulser__ = shot_this[ *pulser()];
    shared_ptr<XNMRPulseAnalyzer> pulse1__ = shot_this[ *pulse1()];
    shared_ptr<XNMRPulseAnalyzer> pulse2__ = shot_this[ *pulse2()];
    if( !pulser__ || !pulse1__) return false;
    if(emitter == this) return true;
    if(emitter == pulser__.get()) return false;
	assert((emitter == pulse1__.get()) || (emitter == pulse2__.get()));

    const Snapshot &shot_pulse1((emitter == pulse1__.get()) ? shot_emitter : shot_others);
    const Snapshot &shot_pulse2((emitter == pulse2__.get()) ? shot_emitter : shot_others);

    if(shot_others[ *pulser__].time() > shot_pulse1[ *pulse1__].time()) return false;

	switch(shot_others[ *pulser__].combMode()) {
	default:
		if(emitter == pulse2__.get())
			return false;
		return true;
	case XPulser::N_COMB_MODE_COMB_ALT:
	case XPulser::N_COMB_MODE_P1_ALT:
		if( !pulse2__) {
			m_statusPrinter->printError(i18n("2 Pulse Analyzers needed."));
			return false;
		}
	    if(shot_pulse1[ *pulse1__].time() != shot_pulse2[ *pulse2__].time()) return false;
		return true;
	}
//    return (pulser__->time() < pulse1__->timeAwared()) && (pulser__->time() < pulse1__->time());
}

void
XNMRT1::analyze(Transaction &tr, const Snapshot &shot_emitter, const Snapshot &shot_others,
	XDriver *emitter) throw (XRecordError&) {
	Snapshot &shot_this(tr);

	double p1min = shot_this[ *p1Min()];
	double p1max = shot_this[ *p1Max()];

	if((p1min <= 0) || (p1min >= p1max)) {
		throw XRecordError(i18n("Invalid P1Min or P1Max."), __FILE__, __LINE__);
	}

	int samples = shot_this[ *smoothSamples()];
	if(samples <= 10) {
		throw XRecordError(i18n("Invalid # of Samples."), __FILE__, __LINE__);
	}
	if(samples >= 100000) {
		m_statusPrinter->printWarning(i18n("Too many Samples."), true);
	}

	int mode__ = shot_this[ *mode()];
	shared_ptr<XNMRPulseAnalyzer> pulse1__ = shot_this[ *pulse1()];
	shared_ptr<XNMRPulseAnalyzer> pulse2__ = shot_this[ *pulse2()];
    const Snapshot &shot_pulse1((emitter == pulse1__.get()) ? shot_emitter : shot_others);
    const Snapshot &shot_pulse2((emitter == pulse2__.get()) ? shot_emitter : shot_others);

	shared_ptr<XPulser> pulser__ = shot_this[ *pulser()];
	const Snapshot &shot_pulser(shot_others);
	assert( pulser__ );
	if(shot_pulser[ *pulser__].time()) {
		//Check consitency.
		switch (mode__) {
		case MEAS_T1:
			break;
		case MEAS_T2:
			break;
		case MEAS_ST_E:
			if((shot_pulser[ *pulser__].tau() != shot_pulser[ *pulser__].combPT()) ||
					(shot_pulser[ *pulser__].combNum() != 2) ||
					( !shot_pulser[ *pulser__->conserveStEPhase()]) ||
					(shot_pulser[ *pulser__].pw1() != 0.0) ||
					(shot_pulser[ *pulser__].pw2() != shot_pulser[ *pulser__].combPW()))
				m_statusPrinter->printWarning(i18n("Strange St.E. settings."));
			break;
		}
	}

	//Reads spectra from NMRPulseAnalyzers
	if( emitter != this) {
		assert( pulse1__ );
		assert( shot_pulse1[ *pulse1__].time() );
		assert( shot_pulser[ *pulser__].time() );
		assert( emitter != pulser__.get() );

		if(shot_pulse1[ *pulse1__->exAvgIncr()]) {
			m_statusPrinter->printWarning(i18n("Do NOT use incremental avg. Skipping."));
			throw XSkippedRecordError(__FILE__, __LINE__);
		}

		std::deque<std::complex<double> > cmp1, cmp2;
        double cfreq = shot_this[ *freq()] * 1e3 * shot_pulse1[ *pulse1__].interval();
        if(shot_this[ *trackPeak()]) {
            if(((mode__ == MEAS_T1) && (shot_pulser[ *pulser__].combP1() > distributeP1(shot_this, 0.66))) ||
               ((mode__ == MEAS_T2) && (shot_pulser[ *pulser__].combP1() < distributeP1(shot_this, 0.33))) ||
               shot_this[ *this].m_sumpts.empty()) {
                tr[ *freq()] = (double)shot_pulse1[ *pulse1__->entryPeakFreq()->value()];
                tr.unmark(m_lsnOnCondChanged); //avoiding recursive signaling.
            }
        }

		analyzeSpectrum(tr, shot_pulse1[ *pulse1__].wave(),
			shot_pulse1[ *pulse1__].waveFTPos(), cfreq, cmp1);        
        Payload::RawPt pt1, pt2;
        if(pulse2__) {
			analyzeSpectrum(tr, shot_pulse2[ *pulse2__].wave(),
				shot_pulse2[ *pulse2__].waveFTPos(), cfreq, cmp2);
			pt2.value_by_cond.resize(cmp2.size());
		}
        pt1.value_by_cond.resize(cmp1.size());
		switch(shot_pulser[ *pulser__].combMode()) {
        default:
			throw XRecordError(i18n("Unknown Comb Mode!"), __FILE__, __LINE__);
        case XPulser::N_COMB_MODE_COMB_ALT:
			if(mode__ != MEAS_T1) throw XRecordError(i18n("Use T1 mode!"), __FILE__, __LINE__);
			assert(pulse2__);
            pt1.p1 = shot_pulser[ *pulser__].combP1();
            for(int i = 0; i < cmp1.size(); i++)
            	pt1.value_by_cond[i] = (cmp1[i] - cmp2[i]) / cmp1[i];
            tr[ *this].m_pts.push_back(pt1);
            break;
        case XPulser::N_COMB_MODE_P1_ALT:
			if(mode__ == MEAS_T2)
                throw XRecordError(i18n("Do not use T2 mode!"), __FILE__, __LINE__);
			assert(pulse2__);
            pt1.p1 = shot_pulser[ *pulser__].combP1();
            std::copy(cmp1.begin(), cmp1.end(), pt1.value_by_cond.begin());
            tr[ *this].m_pts.push_back(pt1);
            pt2.p1 = shot_pulser[ *pulser__].combP1Alt();
            std::copy(cmp2.begin(), cmp2.end(), pt2.value_by_cond.begin());
            tr[ *this].m_pts.push_back(pt2);
            break;
        case XPulser::N_COMB_MODE_ON:
			if(mode__ != MEAS_T2) {
                pt1.p1 = shot_pulser[ *pulser__].combP1();
                std::copy(cmp1.begin(), cmp1.end(), pt1.value_by_cond.begin());
                tr[ *this].m_pts.push_back(pt1);
                break;
			}
			m_statusPrinter->printWarning(i18n("T2 mode with comb pulse!"));
        case XPulser::N_COMB_MODE_OFF:
			if(mode__ != MEAS_T2) {
				m_statusPrinter->printWarning(i18n("Do not use T1 mode! Skipping."));
				throw XSkippedRecordError(__FILE__, __LINE__);
			}
			//T2 measurement
            pt1.p1 = 2.0 * shot_pulser[ *pulser__].tau();
            std::copy(cmp1.begin(), cmp1.end(), pt1.value_by_cond.begin());
            tr[ *this].m_pts.push_back(pt1);
            break;
        }
    }

	tr[ *this].m_sumpts.clear();

    if(shot_this[ *this].m_timeClearRequested) {
        tr[ *this].m_timeClearRequested = {};
        tr[ *this].m_pts.clear();
		tr[ *m_wave].clearPoints();
		tr[ *m_fitStatus] = "";
		trans( *pulse1__->avgClear()).touch();
		if(pulse2__)
			trans( *pulse2__->avgClear()).touch();
		throw XSkippedRecordError(__FILE__, __LINE__);
	}

	shared_ptr<XRelaxFunc> func = shot_this[ *relaxFunc()];
	if( !func) {
		throw XRecordError(i18n("Please select relaxation func."), __FILE__, __LINE__);
	}

	tr[ *this].m_sumpts.resize(samples);
	auto &sumpts(tr[ *this].m_sumpts);

    {
		Payload::Pt dummy;
		dummy.c = 0; dummy.p1 = 0; dummy.isigma = 0;
		dummy.value_by_cond.resize(shot_this[ *this].m_convolutionCache.size());
		std::fill(tr[ *this].m_sumpts.begin(), tr[ *this].m_sumpts.end(), dummy);
        double k = shot_this[ *this].m_sumpts.size() / log(p1max/p1min);
		auto pts_begin(shot_this[ *this].m_pts.begin());
		auto pts_end(shot_this[ *this].m_pts.end());
		int sum_size = (int)shot_this[ *this].m_sumpts.size();
		for(auto it = pts_begin; it != pts_end; it++) {
            int idx = lrint(log(it->p1 / p1min) * k);
			if((idx < 0) || (idx >= sum_size)) continue;
			double p1 = it->p1;
			//For St.E., T+tau = P1+3*tau.
			if(mode__ == MEAS_ST_E)
				p1 += 3 * shot_pulser[ *pulser__].tau() * 1e-3;
			sumpts[idx].isigma += 1;
			sumpts[idx].p1 += p1;
			for(unsigned int i = 0; i < it->value_by_cond.size(); i++)
				sumpts[idx].value_by_cond[i] += it->value_by_cond[i];
		}
	}

	std::deque<std::complex<double> > sum_c(
		shot_this[ *this].m_convolutionCache.size()), corr(shot_this[ *this].m_convolutionCache.size());
	double sum_t = 0.0;
	int n = 0;
	for(auto it = shot_this[ *this].m_sumpts.begin();
		it != shot_this[ *this].m_sumpts.end(); it++) {
		if(it->isigma == 0) continue;
        double t = log10(it->p1 / it->isigma);
		for(unsigned int i = 0; i < it->value_by_cond.size(); i++) {
			sum_c[i] += it->value_by_cond[i];
			corr[i] += it->value_by_cond[i] * t;
		}
        sum_t += t * it->isigma;
        n += it->isigma;
	}
	if(n) {
        //correlation for y_i * log(t_i)
		for(unsigned int i = 0; i < corr.size(); i++) {
            corr[i] -= sum_c[i]*sum_t/(double)n;
			corr[i] *= ((mode__ == MEAS_T1) ? 1 : -1);
		}

		bool absfit__ = shot_this[ *absFit()];

		std::deque<double> phase_by_cond(corr.size(), shot_this[ *phase()] / 180.0 * M_PI);
		int cond = -1;
		double maxsn2 = 0.0;
		for(unsigned int i = 0; i < corr.size(); i++) {
			if(shot_this[ *autoPhase()]) {
				phase_by_cond[i] = std::arg(corr[i]);
			}
			if(shot_this[ *autoWindow()]) {
				double sn2 = absfit__ ? std::abs(corr[i]) : std::real(corr[i] * std::polar(1.0, -phase_by_cond[i]));
				sn2 = sn2 * sn2 / shot_this[ *this].m_convolutionCache[i]->power;
				if(maxsn2 < sn2) {
					maxsn2 = sn2;
					cond = i;
				}
			}
		}
		if(cond < 0) {
			cond = 0;
			for(std::deque<shared_ptr<Payload::ConvolutionCache> >::const_iterator it
				= shot_this[ *this].m_convolutionCache.begin();
				it != shot_this[ *this].m_convolutionCache.end(); ++it) {
				if((m_windowWidthList[std::max(0, (int)shot_this[ *windowWidth()])] == ( *it)->windowwidth) &&
					(m_solver->windowFunc(shot_this) == ( *it)->windowfunc)) {
					break;
				}
				cond++;
			}
		}
		if(cond >= (shot_this[ *this].m_convolutionCache.size())) {
			throw XSkippedRecordError(__FILE__, __LINE__);
		}
		double ph = phase_by_cond[cond];
		if(shot_this[ *autoPhase()]) {
			tr[ *phase()] = ph / M_PI * 180;
			tr.unmark(m_lsnOnCondChanged); //avoiding recursive signaling.
		}
		if(shot_this[ *autoWindow()]) {
			for(unsigned int i = 0; i < m_windowWidthList.size(); i++) {
				if(m_windowWidthList[i] == shot_this[ *this].m_convolutionCache[cond]->windowwidth)
					tr[ *windowWidth()] = i;
			}
			std::deque<FFT::twindowfunc> funcs;
			m_solver->windowFuncs(funcs);
			for(unsigned int i = 0; i < funcs.size(); i++) {
				if(funcs[i] == shot_this[ *this].m_convolutionCache[cond]->windowfunc)
					tr[ *windowFunc()] = i;
			}
			tr.unmark(m_lsnOnCondChanged); //avoiding recursive signaling.
		}
		std::complex<double> cph(std::polar(1.0, -phase_by_cond[cond]));
		for(auto it = sumpts.begin(); it != sumpts.end(); it++) {
			if(it->isigma == 0) continue;
			it->p1 = it->p1 / it->isigma;
			it->c =  it->value_by_cond[cond] * cph / (double)it->isigma;
			it->var = (absfit__) ? std::abs(it->c) : std::real(it->c);
			it->isigma = sqrt(it->isigma);
		}

		tr[ *m_fitStatus] = iterate(tr, func, 4);

		t1inv()->value(tr, 1000.0 * shot_this[ *this].m_params[0]);
		t1invErr()->value(tr, 1000.0 * shot_this[ *this].m_errors[0]);
	}

	m_isPulserControlRequested = (emitter != this);
}
void
XNMRT1::setNextP1(const Snapshot &shot) {
    shared_ptr<XPulser> pulser__ = shot[ *pulser()];
    if(pulser__) {
        pulser__->iterate_commit([=](Transaction &tr){
            switch(shot[ *mode()]) {
            case MEAS_T1:
            case MEAS_ST_E:
                tr[ *pulser__->combP1()] = (double)shot[ *p1Next()];
                tr[ *pulser__->combP1Alt()] = (double)shot[ *p1AltNext()];
                break;
            case MEAS_T2:
                tr[ *pulser__->tau()] = shot[ *p1Next()] / 2.0;
                break;
            }
        });
        iterate_commit([=](Transaction &tr){
            obtainNextP1(tr);
        });
    }
}
void
XNMRT1::visualize(const Snapshot &shot) {
	if( !shot[ *this].time()) {
        iterate_commit([=](Transaction &tr){
			tr[ *m_wave].clearPoints();
        });
		return;
	}

	//set new P1s
    if(shot[ *active()] && m_isPulserControlRequested.compare_set_strong((int)true, (int)false)) {
        setNextP1(shot);
	}

    m_wave->iterate_commit([=](Transaction &tr){
		XString label;
		switch(shot[ *mode()]) {
		case MEAS_T1:
			label = "P1 [ms]";
			break;
		case MEAS_T2:
			label = "2tau [us]";
			break;
		case MEAS_ST_E:
			label = "T+tau [ms]";
			break;
		}
		tr[ *m_wave].setLabel(0, label.c_str());
		tr[ *tr[ *m_wave].axisx()->label()] = label;
        size_t length = shot[ *this].m_sumpts.size();
        tr[ *m_wave].setRowCount(length);
        std::vector<float> colp1(length), colval(length),
            colabs(length), colre(length), colim(length),
            colisigma(length);
		int i = 0;
		for(auto it = shot[ *this].m_sumpts.begin();
			it != shot[ *this].m_sumpts.end(); it++) {
			if(it->isigma == 0) {
				colval[i] = 0;
				colabs[i] = 0;
				colre[i] = 0;
				colim[i] = 0;
				colp1[i] = 0;
			}
			else {
				colval[i] = it->var;
				colabs[i] = std::abs(it->c);
				colre[i] = std::real(it->c);
				colim[i] = std::imag(it->c);
				colp1[i] = it->p1;
			}
			colisigma[i] = it->isigma;
			i++;
		}
        tr[ *m_wave].setColumn(0, std::move(colp1), 5);
        tr[ *m_wave].setColumn(1, std::move(colval), 5);
        tr[ *m_wave].setColumn(2, std::move(colisigma), 4);
        tr[ *m_wave].setColumn(3, std::move(colabs), 4);
        tr[ *m_wave].setColumn(4, std::move(colre), 4);
        tr[ *m_wave].setColumn(5, std::move(colim), 4);
        m_wave->drawGraph(tr);
    });
}

void
XNMRT1::onActiveChanged(const Snapshot &shot, XValueNodeBase *) {
	Snapshot shot_this( *this);
	if(shot_this[ *active()]) {
		const shared_ptr<XPulser> pulser__ = shot_this[ *pulser()];
		const shared_ptr<XNMRPulseAnalyzer> pulse1__ = shot_this[ *pulse1()];
		const shared_ptr<XNMRPulseAnalyzer> pulse2__ = shot_this[ *pulse2()];

		onClearAll(shot_this, m_clearAll.get());
		if( !pulser__ || !pulse1__) {
			gErrPrint(getLabel() + ": " + i18n("No pulser or No NMR Pulse Analyzer."));
			return;
		}

		Snapshot shot_pulse1( *pulse1__);
		Snapshot shot_pulser( *pulser__);
		if(pulse2__ &&
		   ((shot_pulser[ *pulser__->combMode()] == XPulser::N_COMB_MODE_COMB_ALT) ||
			(shot_pulser[ *pulser__->combMode()] == XPulser::N_COMB_MODE_P1_ALT))) {
            pulse2__->iterate_commit([=](Transaction &tr){
				tr[ *pulse2__->fromTrig()] =
					shot_pulse1[ *pulse1__->fromTrig()] + shot_pulser[ *pulser__->altSep()];
				tr[ *pulse2__->width()] = (double)shot_pulse1[ *pulse1__->width()];
				tr[ *pulse2__->phaseAdv()] = (double)shot_pulse1[ *pulse1__->phaseAdv()];
				tr[ *pulse2__->bgPos()] =
					shot_pulse1[ *pulse1__->bgPos()] + shot_pulser[ *pulser__->altSep()];
				tr[ *pulse2__->bgWidth()] = (double)shot_pulse1[ *pulse1__->bgWidth()];
				tr[ *pulse2__->fftPos()] =
					shot_pulse1[ *pulse1__->fftPos()] + shot_pulser[ *pulser__->altSep()];
				tr[ *pulse2__->fftLen()] = (int)shot_pulse1[ *pulse1__->fftLen()];
				tr[ *pulse2__->windowFunc()] = (int)shot_pulse1[ *pulse1__->windowFunc()];
				tr[ *pulse2__->usePNR()] = (bool)shot_pulse1[ *pulse1__->usePNR()];
				tr[ *pulse2__->pnrSolverList()] = (int)shot_pulse1[ *pulse1__->pnrSolverList()];
				tr[ *pulse2__->solverList()] = (int)shot_pulse1[ *pulse1__->solverList()];
				tr[ *pulse2__->numEcho()] = (int)shot_pulse1[ *pulse1__->numEcho()];
				tr[ *pulse2__->echoPeriod()] = (double)shot_pulse1[ *pulse1__->echoPeriod()];
            });
		}
        setNextP1(shot_this);
	}
}


