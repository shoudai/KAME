/***************************************************************************
		Copyright (C) 2002-2012 Kentaro Kitagawa
		                   kitag@kochi-u.ac.jp

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

//---------------------------------------------------------------------------
XAutoLCTuner::XAutoLCTuner(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
	XSecondaryDriver(name, runtime, ref(tr_meas), meas),
		m_stm1(create<XItemNode<XDriverList, XMotorDriver> >("STM1", false, ref(tr_meas), meas->drivers(), true)),
		m_stm2(create<XItemNode<XDriverList, XMotorDriver> >("STM2", false, ref(tr_meas), meas->drivers(), false)),
		m_netana(create<XItemNode<XDriverList, XNetworkAnalyzer> >("NetworkAnalyzer", false, ref(tr_meas), meas->drivers(), true)),
		m_tuning(create<XBoolNode>("Tuning", true)),
		m_target(create<XDoubleNode>("Target", true)),
		m_useSTM1(create<XBoolNode>("UseSTM1", false)),
		m_useSTM2(create<XBoolNode>("UseSTM2", false)),
		m_abortTuning(create<XTouchableNode>("AbortTuning", true)),
		m_form(new FrmAutoLCTuner(g_pFrmMain)) {
	connect(stm1());
	connect(stm2());
	connect(netana());

	m_form->setWindowTitle(i18n("NMR LC autotuner - ") + getLabel() );

	m_conSTM1 = xqcon_create<XQComboBoxConnector>(stm1(), m_form->m_cmbSTM1, ref(tr_meas));
	m_conSTM2 = xqcon_create<XQComboBoxConnector>(stm2(), m_form->m_cmbSTM2, ref(tr_meas));
	m_conNetAna = xqcon_create<XQComboBoxConnector>(netana(), m_form->m_cmbNetAna, ref(tr_meas));
	m_conTarget = xqcon_create<XQLineEditConnector>(target(), m_form->m_edTarget);
	m_conAbortTuning = xqcon_create<XQButtonConnector>(m_abortTuning, m_form->m_btnAbortTuning);
	m_conTuning = xqcon_create<XKLedConnector>(m_tuning, m_form->m_ledTuning);
	m_conUseSTM1 = xqcon_create<XQToggleButtonConnector>(m_useSTM1, m_form->m_ckbUseSTM1);
	m_conUseSTM2 = xqcon_create<XQToggleButtonConnector>(m_useSTM2, m_form->m_ckbUseSTM2);

	for(Transaction tr( *this);; ++tr) {
		tr[ *m_tuning] = false;
		tr[ *m_useSTM1] = true;
		tr[ *m_useSTM2] = true;
		m_lsnOnTargetChanged = tr[ *m_target].onValueChanged().connectWeakly(
			shared_from_this(), &XAutoLCTuner::onTargetChanged);
		m_lsnOnAbortTouched = tr[ *m_abortTuning].onTouch().connectWeakly(
			shared_from_this(), &XAutoLCTuner::onAbortTuningTouched);
		if(tr.commit())
			break;
	}
}
XAutoLCTuner::~XAutoLCTuner() {
}
void XAutoLCTuner::showForms() {
	m_form->show();
	m_form->raise();
}
void XAutoLCTuner::onTargetChanged(const Snapshot &shot, XValueNodeBase *node) {
	for(Transaction tr( *this);; ++tr) {
		tr[ *m_tuning] = true;
		tr[ *this].stage = Payload::STAGE_FIRST;
		if(tr.commit())
			break;
	}
}
void XAutoLCTuner::onAbortTuningTouched(const Snapshot &shot, XTouchableNode *) {
	trans( *m_tuning) = false;
}
void
XAutoLCTuner::determineNextC(double &deltaC1, double &deltaC2, double &err,
	double x, double x_err,
	double y, double y_err,
	double dxdC1, double dxdC2,
	double dydC1, double dydC2) {
	double det = dxdC1 * dydC2 - dxdC2 *dydC1;
	double esq = (dydC2 * x_err * dydC2 * x_err + dxdC2 * y_err * dxdC2 * y_err) / det / det
		+ (dydC1 * x_err * dydC1 * x_err + dxdC1 * y_err * dxdC1 * y_err) / det / det;
	if(esq < err * err) {
		err = sqrt(esq);
		deltaC1 = -(dydC2 * x - dxdC2 * y) / det;
		deltaC2 = -( -dydC1 * x + dxdC1 * y) / det;
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
XAutoLCTuner::analyze(Transaction &tr, const Snapshot &shot_emitter,
	const Snapshot &shot_others,
	XDriver *emitter) throw (XRecordError&) {
	const Snapshot &shot_this(tr);
	const Snapshot &shot_na(shot_emitter);

	if( !shot_this[ *tuning()])
		throw XSkippedRecordError(__FILE__, __LINE__);

	shared_ptr<XMotorDriver> stm1__ = shot_this[ *stm1()];
	shared_ptr<XMotorDriver> stm2__ = shot_this[ *stm2()];
	//remembers original position.
	if(stm1__)
		tr[ *this].stm1 = shot_others[ *stm1__->position()->value()];
	if(stm2__)
		tr[ *this].stm2 = shot_others[ *stm2__->position()->value()];

	if( (stm1__ && !shot_others[ *stm1__->ready()]) ||
			( stm2__  && !shot_others[ *stm2__->ready()]))
		throw XSkippedRecordError(__FILE__, __LINE__); //STM is moving. skip.
	if( shot_this[ *this].isSTMChanged) {
		tr[ *this].isSTMChanged = false;
		throw XSkippedRecordError(__FILE__, __LINE__); //this data is not reliable. reload.
	}

	if( !shot_this[ *useSTM1()]) stm1__.reset();
	if( !shot_this[ *useSTM2()]) stm2__.reset();
	if( !stm1__ && !stm2__) {
		tr[ *m_tuning] = false;
		throw XSkippedRecordError(__FILE__, __LINE__);
	}

	const shared_ptr<XNetworkAnalyzer> na__ = shot_this[ *netana()];

	const std::complex<double> *trace = shot_na[ *na__].trace();
	int trace_len = shot_na[ *na__].length();
	double trace_dfreq = shot_na[ *na__].freqInterval();
	double trace_start = shot_na[ *na__].startFreq();
	std::complex<double> reffmin(1e10);
	double f0 = shot_this[ *target()];
	//searches for minimum in reflection.
	double fmin = 0;
	for(int i = 0; i < trace_len; ++i) {
		if(std::abs(reffmin) > std::abs(trace[i])) {
			reffmin = trace[i];
			fmin = trace_start + i * trace_dfreq;
		}
	}
	//Reflection at the target frequency.
	std::complex<double> reff0;
	for(int i = 0; i < trace_len; ++i) {
		if(trace_start + i * trace_dfreq >= f0) {
			reff0 = trace[i];
			break;
		}
	}
	fprintf(stderr, "LCtuner: fmin=%f, reffmin=%f, reff0=%f\n", fmin, std::abs(reffmin), std::abs(reff0));
	if(std::abs(reff0) < TUNE_APPROACH_GOAL) {
		tr[ *m_tuning] = false;
		return;
	}

	Payload::STAGE stage = shot_this[ *this].stage;
	switch(stage) {
	default:
	case Payload::STAGE_FIRST:
		fprintf(stderr, "LCtuner: the first stage\n");
		//Ref(0, 0)
		tr[ *this].fmin_first = fmin;
		tr[ *this].ref_fmin_first = reffmin;
		tr[ *this].ref_f0_first = reff0;
		if( !stm1__ || !stm2__ || (std::abs(reffmin) < TUNE_APPROACH_START)) {
			fprintf(stderr, "LCtuner: approach mode\n");
			tr[ *this].mode = Payload::TUNE_APPROACHING;
			tr[ *this].dCa =TUNE_DROT_APPROACH * ((tr[ *this].dCa > 0) ? 1.0 : -1.0);
			tr[ *this].dCb = TUNE_DROT_APPROACH * ((tr[ *this].dCb > 0) ? 1.0 : -1.0);
		}
		else {
			fprintf(stderr, "LCtuner: minimizing mode\n");
			tr[ *this].mode = Payload::TUNE_MINIMIZING;
			tr[ *this].dCa = TUNE_DROT_MINIMIZING * ((tr[ *this].dCa > 0) ? 1.0 : -1.0);
			tr[ *this].dCb = TUNE_DROT_MINIMIZING * ((tr[ *this].dCb > 0) ? 1.0 : -1.0);
		}
		tr[ *this].isSTMChanged = true;
		tr[ *this].stage = Payload::STAGE_DCA_FIRST;
		if(stm1__)
			tr[ *this].stm1 += tr[ *this].dCa;
		else
			tr[ *this].stm2 += tr[ *this].dCa;
		throw XSkippedRecordError(__FILE__, __LINE__);  //rotate Ca
		break;
	case Payload::STAGE_DCA_FIRST:
	{
		fprintf(stderr, "LCtuner: +dCa, 1st\n");
		//Ref( +dCa, 0)
		tr[ *this].fmin_plus_dCa = fmin;
		tr[ *this].ref_fmin_plus_dCa = reffmin;
		tr[ *this].ref_f0_plus_dCa = reff0;
		tr[ *this].stage = Payload::STAGE_DCA_SECOND;
		tr[ *this].trace_prv.resize(trace_len);
		auto *trace_prv = &tr[ *this].trace_prv[0];
		for(int i = 0; i < trace_len; ++i) {
			trace_prv[i] = trace[i];
		}
		throw XSkippedRecordError(__FILE__, __LINE__); //to next stage.
		break;
	}
	case Payload::STAGE_DCA_SECOND:
	{
		fprintf(stderr, "LCtuner: +dCa, 2nd\n");
		//Ref( +dCa, 0), averaged with the previous.
		tr[ *this].fmin_plus_dCa = (tr[ *this].fmin_plus_dCa + fmin) / 2.0;
		tr[ *this].ref_fmin_plus_dCa = (tr[ *this].ref_fmin_plus_dCa + reffmin) / 2.0;
		tr[ *this].ref_f0_plus_dCa = (tr[ *this].ref_f0_plus_dCa + reff0) / 2.0;
		//estimates errors.
		if(shot_this[ *this].trace_prv.size() != trace_len) {
			tr[ *m_tuning] = false;
			throw XRecordError(i18n("Record is inconsistent."), __FILE__, __LINE__);
		}
		double ref_sigma = 0.0;
		for(int i = 0; i < trace_len; ++i) {
			ref_sigma += std::norm(trace[i] - shot_this[ *this].trace_prv[i]);
		}
		ref_sigma = sqrt(ref_sigma / trace_len);
		tr[ *this].ref_sigma = ref_sigma;
		tr[ *this].trace_prv.clear();
		if(std::abs(reff0) < ref_sigma * 2) {
			tr[ *m_tuning] = false;
			fprintf(stderr, "LCtuner: tuning done within errors.\n");
			return;
		}

		double fmin_err = trace_dfreq;
		for(int i = 0; i < trace_len; ++i) {
			double flen_from_fmin = fabs(trace_start + i * trace_dfreq - fmin);
			if((flen_from_fmin > fmin_err) &&
				(std::abs(reffmin) + ref_sigma * TUNE_DROT_REQUIRED_N_SIGMA > std::abs(trace[i]))) {
					fmin_err = flen_from_fmin;
			}
		}
		tr[ *this].fmin_err = fmin_err;
		fprintf(stderr, "LCtuner: ref_sigma=%f, fmin_err=%f\n", ref_sigma, fmin_err);

		double dfmin = shot_this[ *this].fmin_plus_dCa - shot_this[ *this].fmin_first;
		tr[ *this].dfmin_dCa = dfmin / shot_this[ *this].dCa;
		std::complex<double> dref = (shot_this[ *this].mode == Payload::TUNE_APPROACHING) ?
				(shot_this[ *this].ref_f0_plus_dCa - shot_this[ *this].ref_f0_first) :
				(shot_this[ *this].ref_fmin_plus_dCa - shot_this[ *this].ref_fmin_first);
		tr[ *this].dref_dCa = dref / shot_this[ *this].dCa;
		if((fabs(dfmin) < fmin_err) && (std::abs(dref) < ref_sigma * TUNE_DROT_REQUIRED_N_SIGMA)) {
			if(tr[ *this].dCa < TUNE_DROT_ABORT) {
				if(stm1__)
					tr[ *this].stm1 += 2.0 * tr[ *this].dCa;
				else
					tr[ *this].stm2 += 2.0 * tr[ *this].dCa;
				tr[ *this].dCa *= 3.0; //increases rotation angle to measure derivative.
				tr[ *this].isSTMChanged = true;
				tr[ *this].stage = Payload::STAGE_DCA_FIRST; //rotate C1 more and try again.
				fprintf(stderr, "LCtuner: increasing dCa to %f\n", (double)tr[ *this].dCa);
				throw XSkippedRecordError(__FILE__, __LINE__);
			}
			if( !stm1__ || !stm2__) {
				tr[ *m_tuning] = false;
				throw XRecordError(i18n("Aborting. the target is out of tune, or capasitors have sticked."), __FILE__, __LINE__); //C1/C2 is useless. Aborts.
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
		//Ref( +dCa, +dCb)
		double ref_sigma = shot_this[ *this].ref_sigma;
		double fmin_err = shot_this[ *this].fmin_err;
		double dfmin = fmin - shot_this[ *this].fmin_plus_dCa;
		tr[ *this].dfmin_dCb = dfmin / shot_this[ *this].dCb;
		std::complex<double> dref = (shot_this[ *this].mode == Payload::TUNE_APPROACHING) ?
				(reff0 - shot_this[ *this].ref_f0_plus_dCa) : (reffmin - shot_this[ *this].ref_fmin_plus_dCa);
		tr[ *this].dref_dCb = dref / shot_this[ *this].dCb;
		if((fabs(dfmin) < fmin_err) && (std::abs(dref) < ref_sigma * TUNE_DROT_REQUIRED_N_SIGMA)) {
			if(tr[ *this].dCb < TUNE_DROT_ABORT) {
				tr[ *this].stm2 += 2.0 * tr[ *this].dCb;
				tr[ *this].dCb *= 3.0; //increases rotation angle to measure derivative.
				tr[ *this].isSTMChanged = true;
				fprintf(stderr, "LCtuner: increasing dCb to %f\n", (double)tr[ *this].dCb);
				//rotate Cb more and try again.
				throw XSkippedRecordError(__FILE__, __LINE__);
			}
			if(tr[ *this].dCa > TUNE_DROT_ABORT)
				tr[ *m_tuning] = false;
				throw XRecordError(i18n("Aborting. the target is out of tune, or capasitors have sticked."), __FILE__, __LINE__); //C1 and C2 are useless. Aborts.
		}
		break;
	}
	//Final stage.
	tr[ *this].stage = Payload::STAGE_FIRST;

	std::complex<double> dref_dCa = shot_this[ *this].dref_dCa;
	std::complex<double> dref_dCb = shot_this[ *this].dref_dCb;
	double dfmin_dCa = shot_this[ *this].dfmin_dCa;
	double dfmin_dCb = shot_this[ *this].dfmin_dCb;
	double fmin_err = shot_this[ *this].fmin_err;
	double ref_sigma = shot_this[ *this].ref_sigma;
	double dCa_next = 0;
	double dCb_next = 0;

	fprintf(stderr, "LCtuner: re dref_dCa=%.2g, re dref_dCb=%.2g, dfmin_dCa=%.2g, dfmin_dCb=%.2g\n",
		std::real(dref_dCa), std::real(dref_dCb),
		dfmin_dCa, dfmin_dCb);
	if(shot_this[ *this].mode == Payload::TUNE_APPROACHING) {
	//Tunes fmin to f0, and/or ref_f0 to 0
		if(( !stm1__ || !stm2__) ||
			(tr[ *this].dCb > TUNE_DROT_ABORT)) {
			dCa_next = -(fmin - f0) / dfmin_dCa;
		}
		else if(tr[ *this].dCa > TUNE_DROT_ABORT) {
			dCb_next = -(fmin - f0) / dfmin_dCb;
		}
		else {
			double dc_err = 1e10;
			//Solves by real(ref) and imag(ref).
//			determineNextC( dc1_next, dc2_next, dc_err,
//				std::real(reff0), ref_sigma * TUNE_DROT_REQUIRED_N_SIGMA,
//				std::imag(reff0), ref_sigma * TUNE_DROT_REQUIRED_N_SIGMA,
//				std::real(dref_dC1), std::real(dref_dC2),
//				std::imag(dref_dC1), std::imag(dref_dC2));
			//Solves by real(ref) and fmin.
			determineNextC( dCa_next, dCb_next, dc_err,
				std::real(reff0), ref_sigma * TUNE_DROT_REQUIRED_N_SIGMA,
				fmin - f0, fmin_err,
				std::real(dref_dCa), std::real(dref_dCb),
				dfmin_dCa, dfmin_dCb);
		}
	}
	else {
	//Tunes ref_fmin to 0
		if(std::abs(dref_dCa) > std::abs(dref_dCb)) {
			dCa_next = -std::real(reffmin) / std::real(dref_dCa);
		}
		else {
			dCb_next = -std::real(reffmin) / std::real(dref_dCb);
		}
	}
	fprintf(stderr, "LCtuner: deltaCa=%f, deltaCb=%f\n", dCa_next, dCb_next);

	//restricts them within the trust region.
	double dc_max = sqrt(dCa_next * dCa_next + dCb_next * dCb_next);
	double dc_trust = (shot_this[ *this].mode == Payload::TUNE_APPROACHING) ? TUNE_TRUST_APPROACH :TUNE_TRUST_MINIMIZING;
	dc_trust = std::max(dc_trust, fabs(shot_this[ *this].dCa) * 4);
	dc_trust = std::max(dc_trust, fabs(shot_this[ *this].dCb) * 4);
	if(dc_max > dc_trust) {
		dCa_next *= dc_trust / dc_max;
		dCb_next *= dc_trust / dc_max;
		fprintf(stderr, "LCtuner: deltaCa=%f, deltaCb=%f\n", dCa_next, dCb_next);
	}
	dCa_next -= shot_this[ *this].dCa / 2;
	dCb_next -= shot_this[ *this].dCb / 2;
	//remembers last direction.
	tr[ *this].dCa = dCa_next;
	tr[ *this].dCb = dCb_next;

	tr[ *this].isSTMChanged = true;
	if(stm1__ && stm2__) {
		tr[ *this].stm1 += dCa_next;
		tr[ *this].stm2 += dCb_next;
	}
	else {
		if(stm1__)
			tr[ *this].stm1 += dCa_next;
		if(stm2__)
			tr[ *this].stm2 += dCa_next;
	}
	throw XSkippedRecordError(__FILE__, __LINE__);
}
void
XAutoLCTuner::visualize(const Snapshot &shot_this) {
	const shared_ptr<XMotorDriver> stm1__ = shot_this[ *stm1()];
	const shared_ptr<XMotorDriver> stm2__ = shot_this[ *stm2()];
	if(shot_this[ *tuning()] && shot_this[ *this].isSTMChanged) {
		if(stm1__) {
			for(Transaction tr( *stm1__);; ++tr) {
				if(tr[ *stm1__->position()->value()] == shot_this[ *this].stm1)
					break;
				tr[ *stm1__->target()] = shot_this[ *this].stm1;
				if(tr.commit())
					break;
			}
		}
		if(stm2__) {
			for(Transaction tr( *stm2__);; ++tr) {
				if(tr[ *stm2__->position()->value()] == shot_this[ *this].stm2)
					break;
				tr[ *stm2__->target()] = shot_this[ *this].stm2;
				if(tr.commit())
					break;
			}
		}
	}
}

