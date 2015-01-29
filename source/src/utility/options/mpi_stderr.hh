// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   utility/options/ScalarOption_T_.hh
/// @brief  Program scalar-valued option abstract base class
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)
/// @author Modified by Sergey Lyskov


#ifndef INCLUDED_utility_options_mpi_stderr_hh
#define INCLUDED_utility_options_mpi_stderr_hh


// Unit headers

// C++ headers
#include <utility/assert.hh>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>

namespace utility {
namespace options {


extern void mpi_safe_std_err( std::string msg );

}
}
#endif
