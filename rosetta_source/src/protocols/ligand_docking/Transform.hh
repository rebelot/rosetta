// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   /grid/src/protocols/ligand_docking/Transform.hh
/// @author Sam DeLuca

#ifndef INCLUDED_protocols_ligand_docking_Transform_hh
#define INCLUDED_protocols_ligand_docking_Transform_hh

#include <protocols/moves/Mover.hh>
#include <protocols/ligand_docking/Transform.fwd.hh>

#include <core/conformation/Residue.fwd.hh>
#include <core/kinematics/Jump.fwd.hh>

#include <utility/vector1.hh>


namespace protocols {
namespace ligand_docking {

enum MoveType
{
	conformerMove,
	transformMove
};

struct Transform_info{ // including default values

public:
	std::string chain;
	core::Size chain_id;
	core::Size jump_id;
	core::Real move_distance;
	core::Real box_size;
	core::Real angle;
	core::Size cycles;
	core::Real temperature;
	core::Size repeats;
	Transform_info(): chain(""), move_distance(0),box_size(0), angle(0), cycles(0),repeats(1){};
};

class Transform: public protocols::moves::Mover
{
public:
	Transform();
	Transform(
		std::string const & chain,
		core::Real const & box_size,
		core::Real const & move_distance,
		core::Real const & angle,
		core::Size const & cycles,
		core::Real const & temp
	);
	virtual ~Transform();
	virtual protocols::moves::MoverOP clone() const;
	virtual protocols::moves::MoverOP fresh_instance() const;
	virtual std::string get_name() const;

	virtual void parse_my_tag(
		utility::tag::TagPtr const tag,
		protocols::moves::DataMap & data_map,
		protocols::filters::Filters_map const &,
		protocols::moves::Movers_map const &,
		core::pose::Pose const &
	);

	virtual void apply(core::pose::Pose & pose);

private:
	utility::vector1<std::pair<core::SSize,core::kinematics::Jump> > transform_ligand(core::pose::Pose & pose);
	core::Size change_conformer(core::pose::Pose & pose, core::Size const & seqpos);

	void revert_conformer(
		core::pose::Pose & pose,
		core::Size const & conformer_index,
		core::Size const & seqpos);

	void revert_jumps(
		core::pose::Pose & pose,
		utility::vector1<std::pair<core::SSize,core::kinematics::Jump> > const & jumps);

	void revert_move(
		core::pose::Pose & pose,
		MoveType const & move_type,
		core::Size const & conformer_index,
		core::Size const & seqpos,
		utility::vector1<std::pair<core::SSize,core::kinematics::Jump> > const & jumps);

private:
	//qsar::scoring_grid::GridManagerOP grid_manager_;
	Transform_info transform_info_;
	utility::vector1< core::conformation::ResidueOP >  ligand_conformers_;
	bool optimize_until_score_is_negative_;

};

}
}

#endif /* TRANSFORM_HH_ */
