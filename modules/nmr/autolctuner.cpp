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

class LCRFit {
public:
    //std::abs(s11) < 1.0
    LCRFit(const std::vector<double> &s11, double fstart, double fstep, double fintended, double init_f0 = -1.0);
    LCRFit(const LCRFit &) = default;
    double r1() const {return m_r1;}
    double r2() const {return m_r2;}
    double c1() const {return m_c1;}
    double c2() const {return m_c2;}
    double c1err() const {return m_c1_err;}
    double c2err() const {return m_c2_err;}
    double l1() const {return m_l1;}
    double f0() const {return m_f0;}
    double rl0() const {return m_rl0;}
    double dRLdC2() const {return m_dRLdC2;}
private:
    double m_r1 = 50.0, m_r2 = 1000.0;
    double m_c1 = 10e-12, m_c2 = 10e-12, m_l1 = 1e-6, m_f0 = 0.0;
    double m_c1_err, m_c2_err;
    double m_dRLdC2;
    double m_rl0;
};

LCRFit::LCRFit(const std::vector<double> &s11, double fstart, double fstep, double fintended, double init_f0) {
    if(init_f0 > 0) {
        m_l1 = 50.0 / 2 / M_PI / init_f0;
        m_c1 = 1.0 / 50.0 / 2 / M_PI / init_f0;
        m_c2 = m_c1 * 0.05;
    }
    auto func = [&s11, this, fstart, fstep](const double*params, size_t n, size_t p,
            double *f, std::vector<double *> &df) -> bool {
        m_r1 = params[0];
        m_r2 = params[1];
        m_c1 = params[2];
        m_c2 = params[3];
        m_l1 = params[4];

        double freq = fstart;
        for(size_t i = 0; i < n; ++i) {
            double omega = 2 * M_PI * freq;
            auto jomega = std::complex<double>(0.0, omega);
            auto zLCR = r1() + jomega * l1() + 1.0 / c1() / jomega;
            auto zl0 = 1.0 / (1.0 / r2() + jomega * c2() + 1.0 / zLCR);
            auto rl0 = (zl0 - 50.0) / (zl0 + 50.0);
            double rl0_abs = std::abs(rl0);
            if(f) {
                f[i] = rl0_abs - s11[i];
            }
            auto nom = 1.0 / 50 + 1.0 / zl0;
            nom = ( -2.0 / 50.0) / (nom * nom);
            nom *= std::conj(rl0) / rl0_abs;
            auto nom2 = -nom / (zLCR * zLCR);
            df[0][i] = std::real(nom2);
            df[1][i] = std::real(nom * (-1.0 / (r2() * r2())));
            df[2][i] = std::real(nom2 * (-1.0) / c1() / c1() / jomega);
            df[3][i] = std::real(nom * jomega);
            df[4][i] = std::real(nom2 * jomega);

            auto rl00 = rl0;
            zl0 = 1.0 / (1.0 / (0.1 + r2()) + jomega * c2() + 1.0 / zLCR);
            rl0 = (zl0 - 50.0) / (zl0 + 50.0);
            df[1][i] = (std::abs(rl0) - std::abs(rl00)) / 0.1;
            zl0 = 1.0 / (1.0 / r2() + jomega * (c2() + 1e-14) + 1.0 / zLCR);
            rl0 = (zl0 - 50.0) / (zl0 + 50.0);
            df[3][i] = (std::abs(rl0) - std::abs(rl00)) / 1e-14;
            zLCR = r1() + 0.1 + jomega * l1() + 1.0 / c1() / jomega;
            zl0 = 1.0 / (1.0 / r2() + jomega * (c2()) + 1.0 / zLCR);
            rl0 = (zl0 - 50.0) / (zl0 + 50.0);
            df[0][i] = (std::abs(rl0) - std::abs(rl00)) / 0.1;
            zLCR = r1() + jomega * l1() + 1.0 / (c1() + 1e-14) / jomega;
            zl0 = 1.0 / (1.0 / r2() + jomega * (c2()) + 1.0 / zLCR);
            rl0 = (zl0 - 50.0) / (zl0 + 50.0);
            df[2][i] = (std::abs(rl0) - std::abs(rl00)) / 1e-14;
            zLCR = r1() + jomega * (l1() + 1e-11) + 1.0 / (c1()) / jomega;
            zl0 = 1.0 / (1.0 / r2() + jomega * (c2()) + 1.0 / zLCR);
            rl0 = (zl0 - 50.0) / (zl0 + 50.0);
            df[4][i] = (std::abs(rl0) - std::abs(rl00)) / 1e-11;

            freq += fstep;
        }
        return true;
    };
    auto nlls = NonLinearLeastSquare(func, {m_r1, m_r2, m_c1, m_c2, m_l1}, s11.size());
    m_r1 = nlls.params()[0];
    m_r2 = nlls.params()[1];
    m_c1 = nlls.params()[2];
    m_c2 = nlls.params()[3];
    m_l1 = nlls.params()[4];
    m_c1_err = nlls.errors()[2];
    m_c2_err = nlls.errors()[3];
    m_f0 = 1.0 / 2.0 / M_PI / sqrt(m_l1 * m_c1); //Im(zLCR) = 0
    //Im(nom) = 0
    fstart = fintended;
    double dummy;
    std::vector<double *> df0 = { &dummy, &dummy, &dummy, &m_dRLdC2, &dummy};
    func( &nlls.params()[0], 1, 5, &m_rl0, df0);
    fprintf(stderr, "%s, rms of residuals = %.3g\n", nlls.status().c_str(), sqrt(nlls.chiSquare() / s11.size()));
    fprintf(stderr, "R1:%.3g+-%.2g, R2:%.3g+-%.2g, L:%.3g+-%.2g, C1:%.3g+-%.2g, C2:%.3g+-%.2g\n",
            r1(), nlls.errors()[0], r2(), nlls.errors()[1], l1(), nlls.errors()[4],
            c1(), c1err(), c2(), c2err());
    fprintf(stderr, "f0:%.3g, DC1=%.3g\n", f0(), 1.0 / pow(2.0 * M_PI * fintended, 2.0) / l1() - f0());
    fprintf(stderr, "RL=%.3g, DC2=%.3g\n", m_rl0, m_rl0/m_dRLdC2);
}

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
	double fmin_err;
	double fmin;
	std::complex<double> reffmin(0.0);
	double f0 = shot_this[ *target()];
	std::complex<double> reff0(0.0);
	double reffmin_sigma, reff0_sigma;
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

		//Takes averages around the minimum.
		reffmin = reffmin_peak;
		double ref_sigma_sq = ref_sigma * ref_sigma;
		for(int cnt = 0; cnt < 2; ++cnt) {
			double wsum = 0.0;
			double wsqsum = 0.0;
			std::complex<double> refsum = 0.0;
			double fsum = 0.0;
			double fsqsum = 0.0;
			for(int i = 0; i < trace_len; ++i) {
				double f = trace_start + i * trace_dfreq;
				double zsq = std::norm(trace[i] - reffmin);
				if(zsq < ref_sigma_sq * 10) {
					double w = exp( -zsq / (2.0 * ref_sigma_sq));
					wsum += w;
					wsqsum += w * w;
					refsum += w * trace[i];
					fsum += w * f;
					fsqsum += w * w * (f - fmin) * (f - fmin);
				}
			}
			fmin = fsum / wsum;
			fmin_err = sqrt(fsqsum / wsum / wsum + trace_dfreq * trace_dfreq);
			reffmin = refsum / wsum;
			reffmin_sigma = ref_sigma * sqrt(wsqsum) / wsum;
		}
		//Takes averages around the target frequency.
		double wsum = 0.0;
		double wsqsum = 0.0;
		std::complex<double> refsum = 0.0;
		double f0_err = std::min(trace_dfreq * 5, fmin_err);
		for(int i = 0; i < trace_len; ++i) {
			double f = trace_start + i * trace_dfreq;
			if(fabs(f - f0) < f0_err * 10) {
				double w = exp( -(f - f0) * (f - f0) / (2.0 * f0_err * f0_err));
				wsum += w;
				wsqsum += w * w;
				refsum += w * trace[i];
			}
		}
		reff0 = refsum / wsum;
		reff0_sigma = ref_sigma * sqrt(wsqsum / wsum * wsum);
		fprintf(stderr, "LCtuner: fmin=%.4f+-%.4f, reffmin=%.3f+-%.3f, reff0=%.3f+-%.3f\n",
				fmin, fmin_err, std::abs(reffmin), reffmin_sigma, std::abs(reff0), reff0_sigma);

        std::vector<double> rl(trace_len);
        for(int i = 0; i < trace_len; ++i)
            rl[i] = std::abs(trace[i]);
        auto lcr = LCRFit(rl, trace_start, trace_dfreq, f0, f0);

		tr[ *this].trace.clear();
	}

	if(std::abs(reff0) < reff0_sigma * 2) {
		tr[ *succeeded()] = true;
		fprintf(stderr, "LCtuner: tuning done within errors.\n");
		return;
	}

	if(( !stm1__ || !stm2__) && (std::abs(fmin - f0) < fmin_err)) {
		tr[ *succeeded()] = true;
		fprintf(stderr, "LCtuner: tuning done within errors.\n");
		return;
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
		ref_sigma = reff0_sigma;
		tune_drot_required_nsigma = TUNE_DROT_REQUIRED_N_SIGMA_FINETUNE;
		tune_drot_mul = TUNE_DROT_MUL_FINETUNE;
		break;
	case Payload::TUNE_APPROACHING:
		ref_targeted = reffmin;
		ref_sigma = reffmin_sigma;
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

