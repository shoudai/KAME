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
//---------------------------------------------------------------------------
#include "analyzer.h"
#include "graph.h"
#include "autolctuner.h"
#include "ui_autolctunerform.h"

REGISTER_TYPE(XDriverList, AutoLCTuner, "NMR LC autotuner");

static const double TUNE_DROT_APPROACH = 5.0,
	TUNE_DROT_FINETUNE = 2.0, TUNE_DROT_ABORT = 180.0; //[deg.]
static const double TUNE_TRUST_APPROACH = 720.0, TUNE_TRUST_FINETUNE = 360.0; //[deg.]
static const double TUNE_FINETUNE_START = 0.7; //-3dB@f0
static const double TUNE_DROT_REQUIRED_N_SIGMA_FINETUNE = 1.0;
static const double TUNE_DROT_REQUIRED_N_SIGMA_APPROACH = 2.0;
static const double SOR_FACTOR_MAX = 0.9;
static const double SOR_FACTOR_MIN = 0.3;
static const double TUNE_DROT_MUL_FINETUNE = 2.5;
static const double TUNE_DROT_MUL_APPROACH = 3.5;

#include "nllsfit.h"
#include "rand.h"

//! Series LCR circuit. Additional R in series with a port.
class LCRFit {
public:
    LCRFit(double f0, double rl, bool tight_couple);
    LCRFit(const LCRFit &) = default;
    void fit(const std::vector<double> &s11, double fstart, double fstep);
    void computeResidualError(const std::vector<double> &s11, double fstart, double fstep);
    double r1() const {return m_r1;} //!< R of LCR circuit
    double r2() const {return m_r2;} //!< R in series with a port.
    double c1() const {return m_c1;} //!< C of LCR circuit
    double c2() const {return m_c2;} //!< C in parallel to a port.
    double l1() const {return m_l1;} //!< Fixed value for L.
    double c1err() const {return m_c1_err;}
    double c2err() const {return m_c2_err;}
    double residualError() const {return m_resErr;}

    //! Resonance freq.
    double f0() const {
        double f = 1.0 / 2 / M_PI / sqrt(l1() * c1());
        for(int it = 0; it < 5; ++it)
            f = 1.0 / 2 / M_PI / sqrt(l1() * c1() -
                c1() * c2() / (1.0/pow(50.0 + r2(), 2.0) + pow(2 * M_PI * f * c2(), 2.0)));
        return f;
    }
    double f0err() const {
        double v = 0.0;
        LCRFit lcr( *this);
        lcr.m_c1 += c1err();
        v += std::norm(lcr.f0() - f0());
        lcr.m_c1 = m_c1;
        lcr.m_c2 += c2err();
        v += std::norm(lcr.f0() - f0());
        return sqrt(v);
    }
    double qValue() const {
        return 2 * M_PI * f0() * l1() / r1();
    }
    //! Reflection
    std::complex<double> rl(double omega, std::complex<double> zlcr1_inv) const {
        auto zL =  1.0 / (std::complex<double>(0.0, omega * c2()) + zlcr1_inv) + r2();
        return (zL - 50.0) / (zL + 50.0);
    }
    std::complex<double> rl(double omega) const {
        return rl(omega, 1.0 / zlcr1(omega));
    }
    double rlerr(double omega) const {
        double v = 0.0;
        LCRFit lcr( *this);
        lcr.m_c1 += c1err();
        v += std::norm(lcr.rl(omega) - rl(omega));
        lcr.m_c1 = m_c1;
        lcr.m_c2 += c2err();
        v += std::norm(lcr.rl(omega) - rl(omega));
        return sqrt(v);
    }
    std::pair<double, double> tuneCaps(double target_freq, double target_rl = 0.0, bool tight_couple = true) const; //!< Obtains expected C1 and C2.
private:
    static constexpr double POW_ON_FIT = 1.0; //exaggarates small values during the fit.
    double rlpow(double omega) const {
        return pow(std::norm(rl(omega)), POW_ON_FIT / 2.0);
    }
    std::complex<double> zlcr1(double omega) const {
        return std::complex<double>(r1(), omega * l1() - 1.0 / (omega * c1()));
    }
    double m_r1, m_r2, m_l1;
    double m_c1, m_c2;
    double m_c1_err, m_c2_err;
    double m_resErr;
};

std::pair<double, double> LCRFit::tuneCaps(double f1, double target_rl, bool tight_couple) const {
    LCRFit nlcr( *this);
    double omega = 2 * M_PI * f1;
    double omegasq = pow(2 * M_PI * f1, 2.0);
    for(int it = 0; it < 100; ++it) {
         //self-consistent eq. to fix resonant freq.
        nlcr.m_c1 = 1.0 / omegasq / (nlcr.l1() - nlcr.c2() /  (1/ pow(50.0 + nlcr.r2(), 2.0) + omegasq * nlcr.c2() * nlcr.c2()));
         //self-consistent eq. to fix rl.
        auto zlcr1_inv = 1.0 / nlcr.zlcr1(omega);
        auto rl1 = nlcr.rl(omega, zlcr1_inv);
        rl1 = target_rl * rl1 / std::abs(rl1); //rearranges |RL|, leaving a phase.
        if((tight_couple ? 1 : -1) * std::real(rl1) < 0)
            rl1 = (tight_couple ? 1 : -1) * target_rl; //forces to select tight or coarse coupling.
        auto target_zlinv = 1.0 / (100.0 / (1.0 - rl1) - 50.0 - r2());
        nlcr.m_c2 = std::imag(target_zlinv - zlcr1_inv) / omega;
    }
    return {nlcr.m_c1, nlcr.m_c2};
}

LCRFit::LCRFit(double init_f0, double init_rl, bool tight_couple) {
    m_l1 = 50.0 / (2.0 * M_PI * init_f0);
    m_c1 = 1.0 / 50.0 / (2.0 * M_PI * init_f0);
    m_c2 = m_c1 * 10;
    double q = randMT19937() * 100 + 2;
    m_r1 = 2.0 * M_PI * init_f0 * l1() / q;
    m_r2 = 1.0;
    std::tie(m_c1, m_c2) = tuneCaps(init_f0, init_rl, tight_couple);
    double omega = 2 * M_PI * init_f0;
//    fprintf(stderr, "Target (%.4g, %.2g) -> (%.4g, %.2g)\n", init_f0, init_rl, f0(), std::abs(rl(omega)));
    m_resErr = 1.0;
    if((fabs(init_f0 - f0()) / f0() < 1e-3) && (fabs(init_rl - std::abs(rl(omega))) < 0.1))
        m_resErr = 0.01;
}

void
LCRFit::computeResidualError(const std::vector<double> &s11, double fstart, double fstep) {
    double x;
    double freq = fstart;
    for(size_t i = 0; i < s11.size(); ++i) {
        double omega = 2 * M_PI * freq;
        x += std::norm(pow(s11[i], POW_ON_FIT) - rlpow(omega));
        freq += fstep;
    }
    m_resErr = sqrt(x / s11.size()) / POW_ON_FIT;
}

void
LCRFit::fit(const std::vector<double> &s11, double fstart, double fstep) {
    auto func = [&s11, this, fstart, fstep](const double*params, size_t n, size_t p,
            double *f, std::vector<double *> &df) -> bool {
        m_r1 = params[0];
        if(p >= 2) m_c2 = params[1];
        if(p >= 3) m_c1 = fabs(params[2]);
        if(p >= 4) m_r2 = params[3];

        constexpr double DR1 = 1e-2, DR2 = 1e-2, DC1 = 1e-15, DC2 = 1e-15;
        LCRFit plusDR1( *this), plusDR2( *this), plusDC1( *this), plusDC2( *this);
        plusDR1.m_r1 += DR1;
        plusDR2.m_r2 += DR2;
        plusDC1.m_c1 += DC1;
        plusDC2.m_c2 += DC2;
        double freq = fstart;
        for(size_t i = 0; i < n; ++i) {
            double omega = 2 * M_PI * freq;
            double rlpow0 = rlpow(omega);
            if(f) {
                f[i] = rlpow0 - pow(s11[i], POW_ON_FIT);
            }
            else {
                df[0][i] = (plusDR1.rlpow(omega) - rlpow0) / DR1;
                if(p >= 2) df[1][i] = (plusDC2.rlpow(omega) - rlpow0) / DC2;
                if(p >= 3) df[2][i] = (plusDC1.rlpow(omega) - rlpow0) / DC1;
                if(p >= 4) df[3][i] = (plusDR2.rlpow(omega) - rlpow0) / DR2;
            }
            freq += fstep;
        }
        return true;
    };

    LCRFit lcr_orig( *this);
    double residualerr = 1.0;
    NonLinearLeastSquare nlls;
    int retry = 0;
    for(auto start = XTime::now(); XTime::now() - start < 0.5;) {
        retry++;
        if(retry % 4 == 1) {
            double f0org = lcr_orig.f0();
            *this = LCRFit(f0org, std::abs(lcr_orig.rl(2.0 * M_PI * f0org)), retry % 8 == 0);
            computeResidualError(s11, fstart, fstep);
            if(residualError() < residualerr) {
                residualerr  = residualError();
                lcr_orig = *this;
                fprintf(stderr, "Found best tune: rms of residuals = %.3g\n", residualError());
            }
        }
        if((fabs(r2()) > 10) || (c1() < 0) || (c2() < 0) || (qValue() > 1e4))
            continue;
        auto nlls1 = NonLinearLeastSquare(func, {m_r1, m_c2, m_c1, m_r2}, s11.size());
        m_r1 = fabs(nlls1.params()[0]);
        m_c2 = nlls1.params()[1];
        m_c1 = nlls1.params()[2];
        m_r2 = nlls1.params()[3];
        double re = sqrt(nlls1.chiSquare() / s11.size()) / POW_ON_FIT;
        if(re < residualerr) {
            residualerr = re;
            nlls = std::move(nlls1);
        }
//        {
//            double re = sqrt(nlls1.chiSquare() / s11.size()) / POW_ON_FIT;
//            fprintf(stderr, "%d: rms of residuals = %.3g;", retry, re);
//        }
//        nlls1 = NonLinearLeastSquare(func, {m_r1}, s11.size());
//        m_r1 = fabs(nlls1.params()[0]);
//        {
//            double re = sqrt(nlls1.chiSquare() / s11.size()) / POW_ON_FIT;
//            fprintf(stderr, " %.3g;", re);
//        }
//        nlls1 = NonLinearLeastSquare(func, {m_r1, m_c2}, s11.size());
//        m_r1 = fabs(nlls1.params()[0]);
//        m_c2 = nlls1.params()[1];
//        {
//            double re = sqrt(nlls1.chiSquare() / s11.size()) / POW_ON_FIT;
//            fprintf(stderr, " %.3g;", re);
//        }
        if((XTime::now() - start > 0.01) && (residualerr < 1e-5))
            break;
    }
    if(lcr_orig.residualError() == residualerr) {
        *this = lcr_orig; //Fitting was worse than the initial value.
        fprintf(stderr, "Fitting was worse than the initial value.\n");
    }
    computeResidualError(s11, fstart, fstep);
    if(nlls.errors().size() == 4) {
        m_c2_err = nlls.errors()[1];
        m_c1_err = nlls.errors()[2];
        fprintf(stderr, "R1:%.3g+-%.2g, R2:%.3g+-%.2g, L:%.3g, C1:%.3g+-%.2g, C2:%.3g+-%.2g\n",
                r1(), nlls.errors()[0], r2(), nlls.errors()[3], l1(),
                c1(), c1err(), c2(), c2err());
    }
    fprintf(stderr, "rms of residuals = %.3g\n", residualError());
}

class XLCRPlot : public XFuncPlot {
public:
    XLCRPlot(const char *name, bool runtime, Transaction &tr, const shared_ptr<XGraph> &graph)
        : XFuncPlot(name, runtime, tr, graph), m_graph(graph)
    {}
    void setLCR(const shared_ptr<LCRFit> &lcr) {
        m_lcr = lcr;
        if(auto graph = m_graph.lock()) {
            Snapshot shot( *graph);
            shot.talk(shot[ *graph].onUpdate(), graph.get());
        }
    }
    virtual double func(double mhz) const {
        if( !m_lcr) return 0.0;
        return 20.0 * log10(std::abs(m_lcr->rl(2.0 * M_PI * mhz * 1e6)));
    }
private:
    shared_ptr<LCRFit> m_lcr;
    weak_ptr<XGraph> m_graph;
};
//---------------------------------------------------------------------------
XAutoLCTuner::XAutoLCTuner(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
	XSecondaryDriver(name, runtime, ref(tr_meas), meas),
		m_stm1(create<XItemNode<XDriverList, XMotorDriver> >("STM1", false, ref(tr_meas), meas->drivers(), true)),
		m_stm2(create<XItemNode<XDriverList, XMotorDriver> >("STM2", false, ref(tr_meas), meas->drivers(), false)),
		m_netana(create<XItemNode<XDriverList, XNetworkAnalyzer> >("NetworkAnalyzer", false, ref(tr_meas), meas->drivers(), true)),
		m_tuning(create<XBoolNode>("Tuning", true)),
		m_succeeded(create<XBoolNode>("Succeeded", true)),
		m_target(create<XDoubleNode>("Target", true)),
		m_reflectionTargeted(create<XDoubleNode>("ReflectionTargeted", false)),
		m_reflectionRequired(create<XDoubleNode>("ReflectionRequired", false)),
		m_useSTM1(create<XBoolNode>("UseSTM1", false)),
		m_useSTM2(create<XBoolNode>("UseSTM2", false)),
		m_abortTuning(create<XTouchableNode>("AbortTuning", true)),
		m_form(new FrmAutoLCTuner(g_pFrmMain))  {
	connect(stm1());
	connect(stm2());
	connect(netana());

	m_form->setWindowTitle(i18n("NMR LC autotuner - ") + getLabel() );

    m_conUIs = {
        xqcon_create<XQComboBoxConnector>(stm1(), m_form->m_cmbSTM1, ref(tr_meas)),
        xqcon_create<XQComboBoxConnector>(stm2(), m_form->m_cmbSTM2, ref(tr_meas)),
        xqcon_create<XQComboBoxConnector>(netana(), m_form->m_cmbNetAna, ref(tr_meas)),
        xqcon_create<XQLineEditConnector>(target(), m_form->m_edTarget),
        xqcon_create<XQLineEditConnector>(reflectionTargeted(), m_form->m_edReflectionTargeted),
        xqcon_create<XQLineEditConnector>(reflectionRequired(), m_form->m_edReflectionRequired),
        xqcon_create<XQButtonConnector>(m_abortTuning, m_form->m_btnAbortTuning),
        xqcon_create<XQLedConnector>(m_tuning, m_form->m_ledTuning),
        xqcon_create<XQLedConnector>(m_succeeded, m_form->m_ledSucceeded),
        xqcon_create<XQToggleButtonConnector>(m_useSTM1, m_form->m_ckbUseSTM1),
        xqcon_create<XQToggleButtonConnector>(m_useSTM2, m_form->m_ckbUseSTM2)
    };

	iterate_commit([=](Transaction &tr){
		tr[ *m_tuning] = false;
		tr[ *m_succeeded] = false;
		tr[ *m_reflectionTargeted] = -15.0;
		tr[ *m_reflectionRequired] = -7.0;
		tr[ *m_useSTM1] = true;
		tr[ *m_useSTM2] = true;
		m_lsnOnTargetChanged = tr[ *m_target].onValueChanged().connectWeakly(
			shared_from_this(), &XAutoLCTuner::onTargetChanged);
		m_lsnOnAbortTouched = tr[ *m_abortTuning].onTouch().connectWeakly(
			shared_from_this(), &XAutoLCTuner::onAbortTuningTouched);
    });
}
XAutoLCTuner::~XAutoLCTuner() {
}
void XAutoLCTuner::showForms() {
    m_form->showNormal();
	m_form->raise();
}
void XAutoLCTuner::onTargetChanged(const Snapshot &shot, XValueNodeBase *node) {
	Snapshot shot_this( *this);
	shared_ptr<XMotorDriver> stm1__ = shot_this[ *stm1()];
	shared_ptr<XMotorDriver> stm2__ = shot_this[ *stm2()];
	const unsigned int tunebits = 0xffu;
	if(stm1__) {
        stm1__->iterate_commit([=](Transaction &tr){
			tr[ *stm1__->active()] = true; // Activate motor.
			tr[ *stm1__->auxBits()] = tunebits; //For external RF relays.
        });
	}
	if(stm2__) {
        stm2__->iterate_commit([=](Transaction &tr){
			tr[ *stm2__->active()] = true; // Activate motor.
			tr[ *stm2__->auxBits()] = tunebits; //For external RF relays.
        });
	}
	iterate_commit([=](Transaction &tr){
		tr[ *m_tuning] = true;
		tr[ *succeeded()] = false;
		tr[ *this].iteration_count = 0;
		tr[ *this].ref_f0_best = 1e10;
		tr[ *this].isSTMChanged = true;
		tr[ *this].sor_factor = SOR_FACTOR_MAX;
		tr[ *this].stage = Payload::STAGE_FIRST;
		tr[ *this].trace.clear();
		tr[ *this].dCa = 0.0;
		tr[ *this].dCb = 0.0;
		tr[ *this].started = XTime::now();
		tr[ *this].isTargetAbondoned = false;
    });


    shared_ptr<XNetworkAnalyzer> na__ = shot_this[ *netana()];
    if(na__)
        na__->graph()->iterate_commit([=](Transaction &tr){
        m_lcrPlot = na__->graph()->plots()->create<XLCRPlot>(
            tr, "FittedCurve", true, tr, na__->graph());
        tr[ *m_lcrPlot->label()] = i18n("Fitted Curve");
        tr[ *m_lcrPlot->axisX()] = tr.list(na__->graph()->axes())->at(0);
        tr[ *m_lcrPlot->axisY()] = tr.list(na__->graph()->axes())->at(1);
        tr[ *m_lcrPlot->lineColor()] = clGreen;
    });
}
void XAutoLCTuner::onAbortTuningTouched(const Snapshot &shot, XTouchableNode *) {
    iterate_commit_while([=](Transaction &tr)->bool{
		if( !tr[ *m_tuning])
            return false;
		tr[ *m_tuning] = false;
		tr[ *this].isSTMChanged = false;
        return true;
    });
}
void
XAutoLCTuner::determineNextC(double &deltaC1, double &deltaC2,
	double x, double x_err,
	double y, double y_err,
	double dxdC1, double dxdC2,
	double dydC1, double dydC2) {
	double det = dxdC1 * dydC2 - dxdC2 *dydC1;
	double slimit = 1e-60;
	x -= ((x > 0) ? 1 : -1) * std::min(x_err, fabs(x)); //reduces as large as err.
	y -= ((y > 0) ? 1 : -1) * std::min(y_err, fabs(y));
	if(det > slimit) {
		deltaC1 = -(dydC2 * x - dxdC2 * y) / det;
		deltaC2 = -( -dydC1 * x + dxdC1 * y) / det;
	}
	else {
		if(fabs(x) > x_err) {
			//superior to y
			if(fabs(dxdC2) > slimit) {
				deltaC2 = -x / dxdC2;
			}
			if(fabs(dxdC1) > slimit) {
				deltaC1 = -x / dxdC1;
			}
		}
		else {
			if(fabs(dydC2) > slimit) {
				deltaC2 = -y / dydC2;
			}
			if(fabs(dydC1) > slimit) {
				deltaC1 = -y / dydC1;
			}
		}
	}
}
bool XAutoLCTuner::checkDependency(const Snapshot &shot_this,
	const Snapshot &shot_emitter, const Snapshot &shot_others,
	XDriver *emitter) const {
	const shared_ptr<XMotorDriver> stm1__ = shot_this[ *stm1()];
	const shared_ptr<XMotorDriver> stm2__ = shot_this[ *stm2()];
	const shared_ptr<XNetworkAnalyzer> na__ = shot_this[ *netana()];
	if( !na__)
		return false;
	if(stm1__ == stm2__)
		return false;
	if(emitter != na__.get())
		return false;

	return true;
}
void
XAutoLCTuner::rollBack(Transaction &tr) {
	fprintf(stderr, "LCtuner: Rolls back.\n");
	//rolls back to good positions.
	tr[ *this].isSTMChanged = true;
	tr[ *this].dCa = 0.0;
	tr[ *this].dCb = 0.0;
	tr[ *this].stm1 = tr[ *this].stm1_best;
	tr[ *this].stm2 = tr[ *this].stm2_best;
	tr[ *this].ref_f0_best = 1e10; //resets the best pos.
	throw XSkippedRecordError(__FILE__, __LINE__);
}
void
XAutoLCTuner::abortTuningFromAnalyze(Transaction &tr, std::complex<double> reff0) {
	double tune_approach_goal2 = pow(10.0, 0.05 * tr[ *reflectionRequired()]);
	if(tune_approach_goal2 > std::abs(tr[ *this].ref_f0_best)) {
		fprintf(stderr, "LCtuner: Softens target value.\n");
		tr[ *this].iteration_count = 0;
		tr[ *this].ref_f0_best = 1e10;
		tr[ *this].isSTMChanged = true;
		tr[ *this].sor_factor = SOR_FACTOR_MAX;
		tr[ *this].stage = Payload::STAGE_FIRST;
		tr[ *this].trace.clear();
		tr[ *this].started = XTime::now();
		tr[ *this].isTargetAbondoned = true;
		rollBack(tr); //rolls back and skps.
	}
	tr[ *m_tuning] = false;
	if(std::abs(reff0) > std::abs(tr[ *this].ref_f0_best)) {
		tr[ *this].isSTMChanged = true;
		tr[ *this].stm1 = tr[ *this].stm1_best;
		tr[ *this].stm2 = tr[ *this].stm2_best;
		throw XRecordError(i18n("Aborting. Out of tune, or capacitors have sticked. Back to better positions."), __FILE__, __LINE__);
	}
	throw XRecordError(i18n("Aborting. Out of tune, or capacitors have sticked."), __FILE__, __LINE__);
}
void
XAutoLCTuner::analyze(Transaction &tr, const Snapshot &shot_emitter,
	const Snapshot &shot_others,
	XDriver *emitter) throw (XRecordError&) {
	const Snapshot &shot_this(tr);
	const Snapshot &shot_na(shot_emitter);

	shared_ptr<XMotorDriver> stm1__ = shot_this[ *stm1()];
	shared_ptr<XMotorDriver> stm2__ = shot_this[ *stm2()];
	//remembers original position.
	if(stm1__)
		tr[ *this].stm1 = shot_others[ *stm1__->position()->value()];
	if(stm2__)
		tr[ *this].stm2 = shot_others[ *stm2__->position()->value()];
	if( !shot_this[ *useSTM1()]) stm1__.reset();
	if( !shot_this[ *useSTM2()]) stm2__.reset();

	if( (stm1__ && !shot_others[ *stm1__->ready()]) ||
			( stm2__  && !shot_others[ *stm2__->ready()]))
		throw XSkippedRecordError(__FILE__, __LINE__); //STM is moving. skip.
	if( shot_this[ *this].isSTMChanged) {
		tr[ *this].isSTMChanged = false;
		throw XSkippedRecordError(__FILE__, __LINE__); //the present data may involve one before STM was moved. reload.
	}
	if( !shot_this[ *tuning()]) {
		throw XSkippedRecordError(__FILE__, __LINE__);
	}

	if( !stm1__ && !stm2__) {
		tr[ *m_tuning] = false;
		throw XSkippedRecordError(__FILE__, __LINE__);
	}

	const shared_ptr<XNetworkAnalyzer> na__ = shot_this[ *netana()];

	int trace_len = shot_na[ *na__].length();
	double ref_sigma = 0.0;
	{
		const std::complex<double> *trace = shot_na[ *na__].trace();
		if(shot_this[ *this].trace.size() != trace_len) {
			//copies trace.
			tr[ *this].trace.resize(trace_len);
			for(int i = 0; i < trace_len; ++i) {
				tr[ *this].trace[i] = trace[i];
			}
			//re-acquires the same situation.
			throw XSkippedRecordError(__FILE__, __LINE__);
		}

		//estimates errors.
		for(int i = 0; i < trace_len; ++i) {
			ref_sigma += std::norm(trace[i] - shot_this[ *this].trace[i]);
			tr[ *this].trace[i] = (shot_this[ *this].trace[i] + trace[i]) / 2.0; //takes averages.
		}
		ref_sigma = sqrt(ref_sigma / trace_len);
		if(ref_sigma > 0.1) {
			tr[ *this].trace.clear();
			throw XSkippedRecordError(i18n("Too large errors in the trace."), __FILE__, __LINE__);
		}
	}
	double trace_dfreq = shot_na[ *na__].freqInterval();
	double trace_start = shot_na[ *na__].startFreq();
    double fmin, fmin_err;
	std::complex<double> reffmin(0.0);
	double f0 = shot_this[ *target()];
	std::complex<double> reff0(0.0);
    double reff0_sigma;
	//analyzes trace.
	{
		const std::complex<double> *trace = &shot_this[ *this].trace[0];

		std::complex<double> reffmin_peak(1e10);
		//searches for minimum in reflection.
		double fmin_peak = 0;
		for(int i = 0; i < trace_len; ++i) {
			double z = std::abs(trace[i]);
			if(std::abs(reffmin_peak) > z) {
				reffmin_peak = trace[i];
				fmin_peak = trace_start + i * trace_dfreq;
			}
		}

        //Fits to LCR circuit.
        std::vector<double> rl(trace_len);
        for(int i = 0; i < trace_len; ++i)
            rl[i] = std::abs(trace[i]);
        auto lcr1 = std::make_shared<LCRFit>(fmin_peak * 1e6, std::abs(reffmin_peak), false);
        lcr1->fit(rl, trace_start * 1e6, trace_dfreq * 1e6);
        fprintf(stderr, "fres=%.4f+-%.4f MHz, RL=%.3f, Q=%.2g\n", lcr1->f0() * 1e-6, lcr1->f0err() * 1e-6, std::abs(lcr1->rl(2.0 * M_PI * lcr1->f0())), lcr1->qValue());
        auto newcaps = lcr1->tuneCaps(f0 * 1e6);
        fprintf(stderr, "Suggest: C1=%.3g, C2=%.3g\n", newcaps.first, newcaps.second);
        m_lcrPlot->setLCR(lcr1);

        fmin = lcr1->f0() * 1e-6;
        fmin_err = lcr1->f0err() * 1e-6;
        reffmin = std::abs(lcr1->rl(2.0 * M_PI * lcr1->f0()));
        reff0 = std::abs(lcr1->rl(2.0 * M_PI * f0 * 1e6));
        reff0_sigma = lcr1->rlerr(2.0 * M_PI * f0 * 1e6);

		tr[ *this].trace.clear();
	}

	double tune_approach_goal = pow(10.0, 0.05 * shot_this[ *reflectionTargeted()]);
	if(shot_this[ *this].isTargetAbondoned)
		tune_approach_goal = pow(10.0, 0.05 * shot_this[ *reflectionRequired()]);
	if(std::abs(reff0) < tune_approach_goal) {
		fprintf(stderr, "LCtuner: tuning done satisfactorily.\n");
		tr[ *succeeded()] = true;
		return;
	}

	tr[ *this].iteration_count++;
	if(std::abs(shot_this[ *this].ref_f0_best) > std::abs(reff0)) {
		tr[ *this].iteration_count = 0;
		//remembers good positions.
		tr[ *this].stm1_best = tr[ *this].stm1;
		tr[ *this].stm2_best = tr[ *this].stm2;
		tr[ *this].fmin_best = fmin;
		tr[ *this].ref_f0_best = std::abs(reff0) + reff0_sigma;
		tr[ *this].sor_factor = (tr[ *this].sor_factor + SOR_FACTOR_MAX) / 2;
	}
//	else
//		tr[ *this].sor_factor = std::min(tr[ *this].sor_factor, (SOR_FACTOR_MAX + SOR_FACTOR_MIN) / 2);

	bool timeout = (XTime::now() - shot_this[ *this].started > 360); //6min.
	if(timeout) {
		fprintf(stderr, "LCtuner: Time out.\n");
		abortTuningFromAnalyze(tr, reff0);//Aborts.
		return;
	}

	Payload::STAGE stage = shot_this[ *this].stage;

	if(stage ==  Payload::STAGE_FIRST) {
		if((std::abs(shot_this[ *this].ref_f0_best) + reff0_sigma < std::abs(reff0)) &&
			((shot_this[ *this].iteration_count > 10) ||
			((shot_this[ *this].iteration_count > 4) && (std::abs(shot_this[ *this].ref_f0_best) * 1.5 <  std::abs(reff0) - reff0_sigma)))) {
			tr[ *this].iteration_count = 0;
			tr[ *this].sor_factor = (tr[ *this].sor_factor + SOR_FACTOR_MIN) / 2;
			if(shot_this[ *this].sor_factor < (SOR_FACTOR_MAX - SOR_FACTOR_MIN) * pow(2.0, -6.0) + SOR_FACTOR_MIN) {
				abortTuningFromAnalyze(tr, reff0);//Aborts.
				return;
			}
			rollBack(tr); //rolls back and skps.
		}
	}

	if(stage ==  Payload::STAGE_FIRST) {
		fprintf(stderr, "LCtuner: the first stage\n");
		//Ref(0, 0)
		if(std::abs(reff0) < TUNE_FINETUNE_START) {
			fprintf(stderr, "LCtuner: finetune mode\n");
			tr[ *this].mode = Payload::TUNE_FINETUNE;
		}
		else {
			fprintf(stderr, "LCtuner: approach mode\n");
			tr[ *this].mode = Payload::TUNE_APPROACHING;
		}
	}
	//Selects suitable reflection point to be minimized.
	std::complex<double> ref_targeted;
	double tune_drot_required_nsigma;
	double tune_drot_mul;
	switch(shot_this[ *this].mode) {
	case Payload::TUNE_FINETUNE:
		ref_targeted = reff0;
		tune_drot_required_nsigma = TUNE_DROT_REQUIRED_N_SIGMA_FINETUNE;
		tune_drot_mul = TUNE_DROT_MUL_FINETUNE;
		break;
	case Payload::TUNE_APPROACHING:
		ref_targeted = reffmin;
		tune_drot_required_nsigma = TUNE_DROT_REQUIRED_N_SIGMA_APPROACH;
		tune_drot_mul = TUNE_DROT_MUL_APPROACH;
		break;
	}
	switch(stage) {
	default:
	case Payload::STAGE_FIRST:
		//Ref(0, 0)
		tr[ *this].fmin_first = fmin;
		tr[ *this].ref_first = ref_targeted;
		double tune_drot;
		switch(shot_this[ *this].mode) {
		case Payload::TUNE_APPROACHING:
			tune_drot = TUNE_DROT_APPROACH;
			break;
		case Payload::TUNE_FINETUNE:
			tune_drot = TUNE_DROT_FINETUNE;
			break;
		}
		tr[ *this].dCa /= tune_drot_mul * tune_drot_mul; //considers the last change.
		tr[ *this].dCb /= tune_drot_mul * tune_drot_mul;
		if(fabs(tr[ *this].dCa) < tune_drot)
			tr[ *this].dCa = tune_drot * ((shot_this[ *this].dfmin_dCa * (fmin - f0) < 0) ? 1.0 : -1.0); //direction to approach.
		if(fabs(tr[ *this].dCb) < tune_drot)
			tr[ *this].dCb = tune_drot * ((shot_this[ *this].dfmin_dCb * (fmin - f0) < 0) ? 1.0 : -1.0);

		tr[ *this].isSTMChanged = true;
		tr[ *this].stage = Payload::STAGE_DCA;
		if(stm1__)
			tr[ *this].stm1 += tr[ *this].dCa;
		else
			tr[ *this].stm2 += tr[ *this].dCa;
		throw XSkippedRecordError(__FILE__, __LINE__);  //rotate Ca
		break;
	case Payload::STAGE_DCA:
	{
		fprintf(stderr, "LCtuner: +dCa\n");
		//Ref( +dCa, 0), averaged with the previous.
		tr[ *this].fmin_plus_dCa = fmin;
		tr[ *this].ref_plus_dCa = ref_targeted;

		//derivative of freq_min.
		double dfmin = shot_this[ *this].fmin_plus_dCa - shot_this[ *this].fmin_first;
		tr[ *this].dfmin_dCa = dfmin / shot_this[ *this].dCa;

		//derivative of reflection.
		std::complex<double> dref;
		dref = shot_this[ *this].ref_plus_dCa - shot_this[ *this].ref_first;
		tr[ *this].dref_dCa = dref / shot_this[ *this].dCa;

		if((fabs(dfmin) < fmin_err * tune_drot_required_nsigma) &&
			(std::abs(dref) < ref_sigma * tune_drot_required_nsigma)) {
			if(fabs(tr[ *this].dCa) < TUNE_DROT_ABORT) {
				tr[ *this].dCa *= tune_drot_mul; //increases rotation angle to measure derivative.
				tr[ *this].fmin_first = fmin; //the present data may be influenced by backlashes.
				tr[ *this].ref_first = ref_targeted;
				if(stm1__)
					tr[ *this].stm1 += tr[ *this].dCa;
				else
					tr[ *this].stm2 += tr[ *this].dCa;
				tr[ *this].isSTMChanged = true;
				tr[ *this].stage = Payload::STAGE_DCA; //rotate C1 more and try again.
				fprintf(stderr, "LCtuner: increasing dCa to %f\n", (double)tr[ *this].dCa);
				throw XSkippedRecordError(__FILE__, __LINE__);
			}
			if( !stm1__ || !stm2__) {
				abortTuningFromAnalyze(tr, reff0);//C1/C2 is useless. Aborts.
				return;
			}
			//Ca is useless, try Cb.
		}
		if(stm1__ && stm2__) {
			tr[ *this].isSTMChanged = true;
			tr[ *this].stage = Payload::STAGE_DCB; //to next stage.
			tr[ *this].stm2 += tr[ *this].dCb;
			throw XSkippedRecordError(__FILE__, __LINE__);
		}
		break; //to final.
	}
	case Payload::STAGE_DCB:
		fprintf(stderr, "LCtuner: +dCb\n");
		//Ref( 0, +dCb)
		//derivative of freq_min.
		double dfmin = fmin - shot_this[ *this].fmin_plus_dCa;
		tr[ *this].dfmin_dCb = dfmin / shot_this[ *this].dCb;

		//derivative of reflection.
		std::complex<double> dref;
		dref = ref_targeted - shot_this[ *this].ref_plus_dCa;
		tr[ *this].dref_dCb = dref / shot_this[ *this].dCb;

		if((std::min(fabs(shot_this[ *this].dfmin_dCa * shot_this[ *this].dCa), fabs(dfmin)) < fmin_err * tune_drot_required_nsigma) &&
			(std::min(std::abs(shot_this[ *this].dref_dCa * shot_this[ *this].dCa), std::abs(dref)) < ref_sigma * tune_drot_required_nsigma)) {
			if(fabs(tr[ *this].dCb) < TUNE_DROT_ABORT) {
				tr[ *this].dCb *= tune_drot_mul; //increases rotation angle to measure derivative.
				tr[ *this].fmin_plus_dCa = fmin; //the present data may be influenced by backlashes.
				tr[ *this].ref_plus_dCa = ref_targeted;
				tr[ *this].stm2 += tr[ *this].dCb;
				tr[ *this].isSTMChanged = true;
				fprintf(stderr, "LCtuner: increasing dCb to %f\n", (double)tr[ *this].dCb);
				//rotate Cb more and try again.
				throw XSkippedRecordError(__FILE__, __LINE__);
			}
			if(fabs(tr[ *this].dCa) >= TUNE_DROT_ABORT) {
				abortTuningFromAnalyze(tr, reff0);//C1/C2 is useless. Aborts.
				return;
			}
		}
		break;
	}
	//Final stage.
	tr[ *this].stage = Payload::STAGE_FIRST;

	std::complex<double> dref_dCa = shot_this[ *this].dref_dCa;
	std::complex<double> dref_dCb = shot_this[ *this].dref_dCb;
	double gamma;
	switch(shot_this[ *this].mode) {
	case Payload::TUNE_FINETUNE:
//		gamma = 0.5;
//		break;
	case Payload::TUNE_APPROACHING:
		gamma = 1.0;
		break;
	}
	double a = gamma * 2.0 * pow(std::norm(ref_targeted), gamma - 1.0);
	// d |ref^2|^gamma / dCa,b
	double drefgamma_dCa = a * (std::real(ref_targeted) * std::real(dref_dCa) + std::imag(ref_targeted) * std::imag(dref_dCa));
	double drefgamma_dCb = a * (std::real(ref_targeted) * std::real(dref_dCb) + std::imag(ref_targeted) * std::imag(dref_dCb));

	double dfmin_dCa = shot_this[ *this].dfmin_dCa;
	double dfmin_dCb = shot_this[ *this].dfmin_dCb;
	if( !stm1__ || !stm2__) {
		dref_dCb = 0.0;
		drefgamma_dCb = 0.0;
		dfmin_dCb = 0.0;
	}
	double dCa_next = 0;
	double dCb_next = 0;

	fprintf(stderr, "LCtuner: dref_dCa=%.2g, dref_dCb=%.2g, dfmin_dCa=%.2g, dfmin_dCb=%.2g\n",
		drefgamma_dCa, drefgamma_dCb, dfmin_dCa, dfmin_dCb);

	determineNextC( dCa_next, dCb_next,
		pow(std::norm(ref_targeted), gamma), pow(ref_sigma, gamma * 2.0),
		(fmin - f0), fmin_err,
		drefgamma_dCa, drefgamma_dCb,
		dfmin_dCa, dfmin_dCb);

	fprintf(stderr, "LCtuner: deltaCa=%f, deltaCb=%f\n", dCa_next, dCb_next);

	//restricts changes within the trust region.
	double dc_trust;
	switch(shot_this[ *this].mode) {
	case Payload::TUNE_APPROACHING:
		dc_trust = TUNE_TRUST_APPROACH;
		break;
	case Payload::TUNE_FINETUNE:
		dc_trust = TUNE_TRUST_FINETUNE;
		break;
	}
	double dca_trust = fabs(shot_this[ *this].dCa) * 50;
	double dcb_trust = fabs(shot_this[ *this].dCb) * 50;
	//mixes with the best result.
	double sor = shot_this[ *this].sor_factor;
	double red_fac = 1.0;
	if(fabs(tr[ *this].dCa) < fabs(dCa_next)) {
		if(stm1__)
			dCa_next = dCa_next * sor + (tr[ *this].stm1_best - tr[ *this].stm1)  * (1.0 - sor);
		else
			dCa_next = dCa_next * sor + (tr[ *this].stm2_best - tr[ *this].stm2)  * (1.0 - sor);
		red_fac =std::min(red_fac, std::min(dc_trust, dca_trust) / fabs(dCa_next));
	}
	if(fabs(tr[ *this].dCb) < fabs(dCb_next)) {
		if(stm1__ && stm2__)
				dCb_next = dCb_next * sor + (tr[ *this].stm2_best - tr[ *this].stm2)  * (1.0 - sor);
		red_fac =std::min(red_fac, std::min(dc_trust, dcb_trust) / fabs(dCb_next));
	}
	dCa_next *= red_fac;
	dCb_next *= red_fac;
	fprintf(stderr, "LCtuner: deltaCa=%f, deltaCb=%f\n", dCa_next, dCb_next);

	tr[ *this].isSTMChanged = true;
	if(stm1__)
		tr[ *this].stm1 += dCa_next;
	if(stm2__) {
		if(stm1__)
			tr[ *this].stm2 += dCb_next;
		else
			tr[ *this].stm2 += dCa_next;
	}
	tr[ *this].dCa = dCa_next;
	tr[ *this].dCb = dCb_next;
	throw XSkippedRecordError(__FILE__, __LINE__);
}
void
XAutoLCTuner::visualize(const Snapshot &shot_this) {
	const shared_ptr<XMotorDriver> stm1__ = shot_this[ *stm1()];
	const shared_ptr<XMotorDriver> stm2__ = shot_this[ *stm2()];
	if(shot_this[ *tuning()]) {
		if(shot_this[ *succeeded()]){
			const unsigned int tunebits = 0;
			if(stm1__) {
                stm1__->iterate_commit([=](Transaction &tr){
					tr[ *stm1__->active()] = false; //Deactivates motor.
					tr[ *stm1__->auxBits()] = tunebits; //For external RF relays.
                });
			}
			if(stm2__) {
                stm2__->iterate_commit([=](Transaction &tr){
					tr[ *stm2__->active()] = false; //Deactivates motor.
					tr[ *stm2__->auxBits()] = tunebits; //For external RF relays.
                });
			}
			msecsleep(50); //waits for relays.
			trans( *tuning()) = false; //finishes tuning successfully.
		}
	}
    else {
        const shared_ptr<XNetworkAnalyzer> na__ = shot_this[ *netana()];
        if(na__ && m_lcrPlot) {
            try {
                na__->graph()->plots()->release(m_lcrPlot);
            }
            catch (NodeNotFoundError &) {
            }
            m_lcrPlot.reset();
        }
    }
	if(shot_this[ *this].isSTMChanged) {
		if(stm1__) {
            stm1__->iterate_commit_while([=](Transaction &tr)->bool{
				if(tr[ *stm1__->position()->value()] == shot_this[ *this].stm1)
                    return false;
				tr[ *stm1__->target()] = shot_this[ *this].stm1;
                return true;
            });
		}
		if(stm2__) {
            stm2__->iterate_commit_while([=](Transaction &tr)->bool{
				if(tr[ *stm2__->position()->value()] == shot_this[ *this].stm2)
                    return false;
				tr[ *stm2__->target()] = shot_this[ *this].stm2;
                return true;
            });
		}
		msecsleep(50); //waits for ready indicators.
		if( !shot_this[ *tuning()]) {
			trans( *this).isSTMChanged = false;
		}
	}
}

