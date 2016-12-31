// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/recces/sampler/MC_Sampler.hh
/// @brief Abstract Base Class for Markov chain rotamer sampler.
/// @author Fang-Chieh Chou


#ifndef INCLUDED_protocols_sampler_MC_Sampler_HH
#define INCLUDED_protocols_sampler_MC_Sampler_HH

// Unit headers
#include <protocols/recces/sampler/MC_Sampler.fwd.hh>

// Package headers
#include <protocols/toolbox/SamplerPlusPlus.hh>

namespace protocols {
namespace recces {
namespace sampler {

class MC_Sampler: public protocols::toolbox::SamplerPlusPlus {
public:
	MC_Sampler():
		SamplerPlusPlus(),
		uniform_modeler_( false )
	{}

	virtual ~MC_Sampler(){}

	/// @brief Update the DOFs
	virtual void update() = 0;

	/// @brief Set uniform modeler (instead of Gaussian)
	virtual void set_uniform_modeler( bool const setting ) {
		uniform_modeler_ = setting;
	}

	/// @brief Get uniform modeler state
	virtual bool uniform_modeler() const {
		return uniform_modeler_;
	}

	/// @brief Name of the class
	virtual std::string get_name() const { return "MC_Sampler"; }

	/// @brief return OP to the subsampler that controls exactly this torsion_id (assume only one).
	virtual
	MC_SamplerOP
	find( core::id::TorsionID const & torsion_id ) = 0;

private:
	bool uniform_modeler_;
};

} //sampler
} //recces
} //protocols
#endif