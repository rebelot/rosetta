// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @brief Declaration of a filter to extract constellations shared by multiple chains
/// @author Andrea Bazzoli

#ifndef INCLUDED_InterfaceFilter_hh
#define INCLUDED_InterfaceFilter_hh

#include <core/pose/Pose.fwd.hh>
#include <utility/vector1.fwd.hh>
#include <core/types.hh>

using core::pose::Pose;
using core::Size;

namespace devel {
namespace constel {

bool at_interface(Pose const& ps, utility::vector1<Size> const& cnl);

} // constel
} // devel

#endif
