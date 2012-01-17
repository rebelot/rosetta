// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief Assigns a ConstraintSet to a pose. Reads and creats ConstraintSet from file via command line option -constraints::cst_file, unless a ConstraintSet is supplied via the constructor or the constraint_set() method.
/// @author ashworth

#include <protocols/simple_moves/ConstraintSetMover.hh>
#include <protocols/simple_moves/ConstraintSetMoverCreator.hh>

// AUTO-REMOVED #include <protocols/moves/DataMap.hh>

#include <core/pose/Pose.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <utility/tag/Tag.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>

//Auto Headers
#include <protocols/jobdist/Jobs.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>


static basic::Tracer TR( "protocols.simple_moves.ConstraintSetMover" );

namespace protocols {
namespace simple_moves {

using namespace core;
	using namespace basic::options;
	using namespace scoring;
		using namespace constraints;

using namespace utility::tag;

std::string
ConstraintSetMoverCreator::keyname() const
{
	return ConstraintSetMoverCreator::mover_name();
}

protocols::moves::MoverOP
ConstraintSetMoverCreator::create_mover() const {
	return new ConstraintSetMover;
}

std::string
ConstraintSetMoverCreator::mover_name()
{
	return "ConstraintSetMover";
}

ConstraintSetMover::ConstraintSetMover()
	: protocols::moves::Mover( ConstraintSetMoverCreator::mover_name() )
{
	read_options();
}

ConstraintSetMover::~ConstraintSetMover(){}

ConstraintSetMover::ConstraintSetMover( std::string const & type )
	: protocols::moves::Mover(type)
{
	read_options();
}

void
ConstraintSetMover::read_options()
{
	if ( option[ OptionKeys::constraints::cst_file ].user() )
		cst_file_ = option[ OptionKeys::constraints::cst_file ]().front();
	if ( option[ OptionKeys::constraints::cst_fa_file ].user() )
		cst_fa_file_ = option[ OptionKeys::constraints::cst_fa_file ]().front();
	else cst_fa_file_=cst_file_;
}

void
ConstraintSetMover::constraint_set( ConstraintSetCOP cst_set )
{
	constraint_set_low_res_ = new ConstraintSet( *cst_set );
	constraint_set_high_res_ = new ConstraintSet( *cst_set );
}

void
ConstraintSetMover::constraint_file( std::string const & cst_file )
{
	cst_file_ = cst_file;
	cst_fa_file_ = cst_file;
}


ConstraintSetOP ConstraintSetMover::constraint_set() { return constraint_set_low_res_; }
ConstraintSetCOP ConstraintSetMover::constraint_set() const { return constraint_set_low_res_; }

void
ConstraintSetMover::apply( Pose & pose )
{
	if ( !constraint_set_low_res_ && !pose.is_fullatom() ) {
		// uninitialized filename not tolerated, in order to avoid potential confusion
		if ( cst_file_.empty() ) utility_exit_with_message("Can\'t read constraints from empty file!");
		// special case: set cst_file_ to "none" to effectively remove constraints from Pose
		else if ( cst_file_ == "none" ) constraint_set_low_res_ = new ConstraintSet;
		else {
			constraint_set_low_res_ =
				ConstraintIO::get_instance()->read_constraints( cst_file_, new ConstraintSet, pose );
			//ConstraintIO::get_instance()->read_constraints_new( cst_file_, new ConstraintSet, pose );
		}
	}

	if ( !constraint_set_high_res_ && pose.is_fullatom() ) {
		// uninitialized filename not tolerated, in order to avoid potential confusion
		if ( cst_fa_file_.empty() ) utility_exit_with_message("Can\'t read constraints from empty file!");
		// special case: set cst_file_ to "none" to effectively remove constraints from Pose
		else if ( cst_fa_file_ == "none" ) constraint_set_high_res_ = new ConstraintSet;
		else {
			constraint_set_high_res_ =
				ConstraintIO::get_instance()->read_constraints( cst_fa_file_, new ConstraintSet, pose );
			//ConstraintIO::get_instance()->read_constraints_new( cst_fa_file_, new ConstraintSet, pose );
		}
	}

	if ( pose.is_fullatom() ) {
		pose.constraint_set( constraint_set_high_res_ );
	} else {
		pose.constraint_set( constraint_set_low_res_ );
	}
}

std::string
ConstraintSetMover::get_name() const {
	return ConstraintSetMoverCreator::mover_name();
}

protocols::moves::MoverOP ConstraintSetMover::clone() const { return new protocols::simple_moves::ConstraintSetMover( *this ); }
protocols::moves::MoverOP ConstraintSetMover::fresh_instance() const { return new ConstraintSetMover; }

void
ConstraintSetMover::parse_my_tag(
	TagPtr const tag,
	protocols::moves::DataMap &,
	Filters_map const &,
	protocols::moves::Movers_map const &,
	Pose const &
)
{
	if ( tag->hasOption("cst_file") ) cst_file_ = tag->getOption<std::string>("cst_file");
	if ( tag->hasOption("cst_fa_file") ) cst_fa_file_ = tag->getOption<std::string>("cst_fa_file");
	else cst_fa_file_=cst_file_;
	TR << "of type ConstraintSetMover with constraint file: "<< cst_file_ <<std::endl;
	if ( cst_fa_file_ != cst_file_ ) {
		TR << "of type ConstraintSetMover with fullatom constraint file: "<< cst_fa_file_ <<std::endl;
	}
}

} // moves
} // protocols
