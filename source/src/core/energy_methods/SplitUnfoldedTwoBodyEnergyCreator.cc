// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file src/core/energy_methods/SplitUnfoldedTwoBodyEnergyCreator.cc
/// @briefEnergy creator for the split unfolded two body energy method
/// @author Riley Simmons-Edler (rse231@nyu.edu)


#include <core/energy_methods/SplitUnfoldedTwoBodyEnergyCreator.hh>
#include <core/energy_methods/SplitUnfoldedTwoBodyEnergy.hh>

#include <core/scoring/methods/EnergyMethodOptions.hh>
#include <core/scoring/EnergyMap.hh>


#include <utility/vector1.hh>


namespace core {
namespace energy_methods {



core::scoring::methods::EnergyMethodOP SplitUnfoldedTwoBodyEnergyCreator::create_energy_method(const core::scoring::methods::EnergyMethodOptions & options) const
{
	if ( options.has_method_weights( core::scoring::split_unfolded_two_body ) ) {
		utility::vector1<Real> const & v = options.method_weights( core::scoring::split_unfolded_two_body );
		debug_assert( v.size() == scoring::n_score_types );
		core::scoring::EnergyMap e;
		for ( Size ii = 1; ii < scoring::n_score_types; ++ii ) {
			e[( core::scoring::ScoreType)ii]=v[ii];
		}
		//using the same type option as unfolded state energy since those two need to match when using the split unfolded energy(since unfolded state holds the one body component).
		return utility::pointer::make_shared< SplitUnfoldedTwoBodyEnergy >( options.split_unfolded_label_type(), options.split_unfolded_value_type(), options.unfolded_energies_type(), e );
	}
	return utility::pointer::make_shared< SplitUnfoldedTwoBodyEnergy >( options.split_unfolded_label_type(), options.split_unfolded_value_type(), options.unfolded_energies_type() );
}

core::scoring::ScoreTypes SplitUnfoldedTwoBodyEnergyCreator::score_types_for_method() const
{
	using namespace core::scoring;
	ScoreTypes sts;
	sts.push_back( split_unfolded_two_body );
	sts.push_back( fa_atr_ref );
	sts.push_back( fa_rep_ref );
	sts.push_back( fa_sol_ref );
	sts.push_back( fa_elec_ref );
	sts.push_back( hbond_ref );
	sts.push_back( dslf_fa13_ref );
	return sts;
}


}
}
