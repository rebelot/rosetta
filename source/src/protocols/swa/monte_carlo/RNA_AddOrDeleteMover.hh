// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file RNA_AddOrDeleteMover.hh
/// @brief
/// @detailed
///
/// @author Rhiju Das


#ifndef INCLUDED_protocols_swa_monte_carlo_RNA_AddOrDeleteMover_hh
#define INCLUDED_protocols_swa_monte_carlo_RNA_AddOrDeleteMover_hh

#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/swa/monte_carlo/SWA_Move.hh>
#include <protocols/swa/monte_carlo/SWA_MoveSelector.hh>
#include <protocols/swa/monte_carlo/RNA_AddMover.fwd.hh>
#include <protocols/swa/monte_carlo/RNA_DeleteMover.fwd.hh>
#include <protocols/swa/monte_carlo/RNA_AddOrDeleteMover.fwd.hh>


namespace protocols {
namespace swa {
namespace monte_carlo {

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
class RNA_AddOrDeleteMover: public protocols::moves::Mover {
public:


	RNA_AddOrDeleteMover( RNA_AddMoverOP rna_add_mover,
												RNA_DeleteMoverOP rna_delete_mover );

	//destructor -- necessary? -- YES destructors are necessary.
	~RNA_AddOrDeleteMover();
	using protocols::moves::Mover::apply;

	bool
  apply( core::pose::Pose & pose, std::string & move_type );

	/// @brief Apply the minimizer to one pose
	virtual void apply( core::pose::Pose & pose_to_visualize );
	virtual std::string get_name() const;

	void set_disallow_deletion_of_last_residue( bool const setting ){ disallow_deletion_of_last_residue_ = setting; }

	void set_skip_deletions( bool const setting ){ skip_deletions_ = setting; }

	void set_sample_res( utility::vector1< Size > const & setting ){ sample_res_ = setting; }

	void set_minimize_single_res( bool const setting );

	void set_disallow_skip_bulge( bool const & setting ){ disallow_skip_bulge_ = setting; }
	bool disallow_skip_bulge() const{ return disallow_skip_bulge_; }

private:

	RNA_AddMoverOP rna_add_mover_;
	RNA_DeleteMoverOP rna_delete_mover_;
	bool disallow_deletion_of_last_residue_;
	bool skip_deletions_;
	bool disallow_skip_bulge_;
	utility::vector1< Size > sample_res_;
	SWA_MoveSelectorOP swa_move_selector_;
};

} // monte_carlo
} // swa
} // protocols

#endif
