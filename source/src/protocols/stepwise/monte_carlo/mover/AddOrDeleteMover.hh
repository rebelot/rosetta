// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file AddOrDeleteMover.hh
/// @brief
/// @detailed
///
/// @author Rhiju Das


#ifndef INCLUDED_protocols_stepwise_monte_carlo_AddOrDeleteMover_hh
#define INCLUDED_protocols_stepwise_monte_carlo_AddOrDeleteMover_hh

#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/stepwise/monte_carlo/SWA_Move.hh>
#include <protocols/stepwise/monte_carlo/SWA_MoveSelector.hh>
#include <protocols/stepwise/monte_carlo/StepWiseMonteCarloOptions.fwd.hh>
#include <protocols/stepwise/monte_carlo/mover/AddMover.fwd.hh>
#include <protocols/stepwise/monte_carlo/mover/DeleteMover.fwd.hh>
#include <protocols/stepwise/monte_carlo/mover/AddOrDeleteMover.fwd.hh>
#include <protocols/stepwise/monte_carlo/mover/FromScratchMover.fwd.hh>


namespace protocols {
namespace stepwise {
namespace monte_carlo {
namespace mover {

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
class AddOrDeleteMover: public protocols::moves::Mover {
public:


	AddOrDeleteMover( AddMoverOP rna_add_mover,
												DeleteMoverOP rna_delete_mover,
												FromScratchMoverOP rna_from_scratch_mover );

	//destructor -- necessary? -- YES destructors are necessary.
	~AddOrDeleteMover();
	using protocols::moves::Mover::apply;

	void
	apply( core::pose::Pose & pose, SWA_Move const & swa_move );

	bool
  apply( core::pose::Pose & pose, std::string & move_type /* will be updated by mover */ );

	/// @brief Apply the minimizer to one pose
	virtual void apply( core::pose::Pose & pose_to_visualize );
	virtual std::string get_name() const;

	void set_disallow_deletion_of_last_residue( bool const setting ){ disallow_deletion_of_last_residue_ = setting; }

	void set_minimize_single_res( bool const setting );

	void
	set_options( StepWiseMonteCarloOptionsCOP options );

private:

	utility::vector1< Size >
	figure_out_actual_sample_res( pose::Pose const & pose ) const;

private:

	AddMoverOP rna_add_mover_;
	DeleteMoverOP rna_delete_mover_;
	FromScratchMoverOP rna_from_scratch_mover_;
	bool disallow_deletion_of_last_residue_;
	SWA_MoveSelectorOP swa_move_selector_;
	StepWiseMonteCarloOptionsCOP options_;

};

} //mover
} //monte_carlo
} //stepwise
} //protocols

#endif
