// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.
//
/// @file 
/// @brief 
/// @author Jacob Bale ( balej@uw.edu )

#ifndef INCLUDED_devel_matdes_SymDofMoverSampler_HH
#define INCLUDED_devel_matdes_SymDofMoverSampler_HH

#include <core/types.hh>
#include <utility/vector1.hh>
#include <string>


namespace devel {
namespace matdes {

class SymDofMoverSampler 
{
	typedef core::Real Real;
	typedef core::Size Size;

public:
	static SymDofMoverSampler& get_instance();
	void set_angle_ranges(utility::vector1<Real> angles_range_min, utility::vector1<Real> angles_range_max, utility::vector1<Real> angle_steps);
	void set_radial_disp_ranges(utility::vector1<Real> radial_disps_range_min, utility::vector1<Real> radial_disps_range_max, utility::vector1<Real> radial_disp_steps );
	void set_sym_dof_names(utility::vector1<std::string> sym_dof_names );
	void set_angles(utility::vector1<Real> angles );
	void set_radial_disps(utility::vector1<Real> radial_disps );
	utility::vector1<Real> get_angles() { return angles_; }
	utility::vector1<Real> get_radial_disps() { return radial_disps_; }
	utility::vector1<Real> get_angle_diffs() { return current_angles_; }
	utility::vector1<Real> get_radial_disp_diffs() { return current_radial_disps_; }
	utility::vector1<std::string> get_sym_dof_names() { return sym_dof_names_; }
	void step();

private:
	// Don't implement the methods belowed, this class is a singleton.
	SymDofMoverSampler();
	SymDofMoverSampler(SymDofMoverSampler const&);
	void operator=(SymDofMoverSampler const&);


private:
	utility::vector1<std::string> sym_dof_names_;
	utility::vector1<Real> angles_;
	utility::vector1<Real> radial_disps_;
	utility::vector1<Real> angles_range_min_;
	utility::vector1<Real> angles_range_max_;
	utility::vector1<Real> angle_steps_;
	utility::vector1<Real> radial_disps_range_min_;
	utility::vector1<Real> radial_disps_range_max_;
	utility::vector1<Real> radial_disp_steps_;
	utility::vector1<Real> current_angles_;
	utility::vector1<Real> current_radial_disps_;
};

} //matdes
} // devel

#endif
