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
#ifndef MATRIX_H_
#define MATRIX_H_
//---------------------------------------------------------------------------

#include <vector>
#include <complex>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/triangular.hpp>
#include <boost/numeric/ublas/symmetric.hpp>

#include "support.h"

using namespace boost::numeric;

//! RRR (Relative Robast Representation) eigenvalue driver for Hermite matrix.
void eigHermiteRRR(const ublas::matrix<std::complex<double> > &a,
	ublas::vector<double> &lambda, ublas::matrix<std::complex<double> > &v,
	double tol);

#endif /*MATRIX_H_*/
