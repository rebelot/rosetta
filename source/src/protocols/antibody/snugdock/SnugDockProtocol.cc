// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/antibody/SnugDockProtocol.cc
/// @brief Dock and antigen to an antibody while optimizing the rigid body orientation of the VH and VL chains and
/// performing CDR loop minimization.
/// @details
///
///
/// @author Jianqing Xu ( xubest@gmail.com )
/// @author Brian D. Weitzner ( brian.weitzner@gmail.com )
/// @author Jeliazko Jeliazkov ( jeliazkov@jhu.edu )

// Unit headers
#include <protocols/antibody/snugdock/SnugDockProtocol.hh>

// Package headers
#include <protocols/antibody/AntibodyInfo.hh>
#include <protocols/antibody/RefineOneCDRLoop.hh>
#include <protocols/antibody/snugdock/SnugDock.hh>

// Project headers
#include <protocols/docking/DockingProtocol.hh>

#include <core/pose/Pose.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/antibody.OptionKeys.gen.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>

// constraints
#include <core/scoring/constraints/AngleConstraint.hh>
#include <core/scoring/constraints/DihedralConstraint.hh>
#include <core/scoring/func/FlatHarmonicFunc.hh>


// Basic headers
#include <basic/Tracer.hh>

// Utility headers
#include <utility/excn/Exceptions.hh>

static THREAD_LOCAL basic::Tracer TR( "protocols.antibody.SnugDockProtocol" );
using namespace core;

namespace protocols {
namespace antibody {

///////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// BOILER PLATE CODE //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief default constructor
SnugDockProtocol::SnugDockProtocol() : Mover() {
	init();
}

/// @brief copy constructor
SnugDockProtocol::SnugDockProtocol( SnugDockProtocol const & rhs ) : Mover(rhs) {
	init_for_equal_operator_and_copy_constructor( *this, rhs );
}

/// @brief assignment operator
SnugDockProtocol & SnugDockProtocol::operator=( SnugDockProtocol const & rhs ) {
	//abort self-assignment
	if ( this == &rhs ) return *this;
	Mover::operator=( rhs );
	init_for_equal_operator_and_copy_constructor( *this, rhs );
	return *this;
}

//destructor
SnugDockProtocol::~SnugDockProtocol() {}

/// @brief Each derived class must specify its name.
std::string SnugDockProtocol::get_name() const {
	return type();
}

//@brief clone operator, calls the copy constructor
protocols::moves::MoverOP
SnugDockProtocol::clone() const {
	return protocols::moves::MoverOP( new SnugDockProtocol( *this ) );
}

/// @brief fresh_instance returns a default-constructed object for JD2
protocols::moves::MoverOP
SnugDockProtocol::fresh_instance() const {
	return protocols::moves::MoverOP( new SnugDockProtocol() );
}

/// @brief This mover retains state such that a fresh version is needed if the input Pose is about to change
bool SnugDockProtocol::reinitialize_for_new_input() const {
	return true;
}

void SnugDockProtocol::register_options() {
	docking::DockingProtocol::register_options();
	SnugDock::register_options();
	RefineOneCDRLoop::register_options();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// END OF BOILER PLATE CODE //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void SnugDockProtocol::apply( Pose & pose ) {

	using namespace core::scoring::constraints;

	TR << "Beginning apply function of " + get_name() + "." << std::endl;

	if ( ! antibody_info_ ) setup_objects( pose );

	/// apply low-res auto-generated constraints here

	show( TR );
	TR << "Setting the input structure's FoldTree for Antibody-Antigen docking." << std::endl;
	pose.fold_tree( antibody_info_->get_FoldTree_LH_A( pose ) );

	// set constraints, stolen from AntibodyModelerProtocol (code duplication is bad, but I'm in a rush... sorry!)
	if ( has_auto_generate_kink_constraint() ) {
		// Create constraints on-the-fly here.

		// All of this stuff, and the work that is being done in finalize_setup() for that matter, only needs to happen
		// once per input. I don't trust the 'fresh_instance' and related settings in the other antibody movers well
		// enough to actually rely on that, so I'm going to do this every apply for now.

		// Get relevant residue numbers
		Size kink_begin = antibody_info_->kink_begin( pose );

		// Constraints operate on AtomIDs:
		Size CA( 2 ); // CA is atom 2; AtomIDs can only use numbers
		id::AtomID const atom_100x( CA, kink_begin );
		id::AtomID const atom_101( CA, kink_begin + 1 );
		id::AtomID const atom_102( CA, kink_begin + 2 );
		id::AtomID const atom_103( CA, kink_begin + 3 );

		// Yeah, yeah this is turrible. I'm trying to get stuff done over here, ok?
		// Eventually this will read from a database file
		// Generate functions
		Real alpha_x0 = 0.678; // radians
		Real alpha_sd = 0.41; // radians
		Real alpha_tol = 0.205; // radians
		scoring::func::FlatHarmonicFuncOP alpha_func( new scoring::func::FlatHarmonicFunc( alpha_x0, alpha_sd, alpha_tol ) );

		Real tau_x0 = 1.761; // radians
		Real tau_sd = 0.194; // radians
		Real tau_tol = 0.0972; // radians
		scoring::func::FlatHarmonicFuncOP tau_func( new scoring::func::FlatHarmonicFunc( tau_x0, tau_sd, tau_tol ) );

		// Instantiate constraints
		ConstraintOP tau_cst( new AngleConstraint( atom_100x, atom_101, atom_102, tau_func ) );
		ConstraintOP alpha_cst( new DihedralConstraint( atom_100x, atom_101, atom_102, atom_103, alpha_func ) );

		// Cache to pose
		pose.add_constraint( tau_cst );
		pose.add_constraint( alpha_cst );
	}

	TR << "Beginning application of " + docking()->get_name() + "." << std::endl;
	docking()->apply( pose );
}

void SnugDockProtocol::setup_objects( Pose const & pose ) {
	TR << "Setting up data for " + get_name() + "." << std::endl;

	/// AntibodyInfo is used to store information about the Ab-Ag complex and to generate useful helper objects based on
	/// that information (e.g. the various FoldTrees that are needed for SnugDock).
	antibody_info_ = AntibodyInfoOP( new AntibodyInfo( pose ) );

	setup_loop_refinement_movers();
	docking()->add_additional_low_resolution_step( low_res_refine_cdr_h2_ );
	docking()->add_additional_low_resolution_step( low_res_refine_cdr_h3_ );

	SnugDockOP high_resolution_phase( new SnugDock );
	high_resolution_phase->set_antibody_info( antibody_info_ );
	// pass on kink constraint to high-res docking mover
	if ( has_high_res_kink_constraint() ) { high_resolution_phase->set_high_res_kink_constraint( true ); }
	docking()->set_docking_highres_mover( high_resolution_phase );

}

void SnugDockProtocol::setup_loop_refinement_movers() {
	using core::scoring::ScoreFunctionFactory;
	using core::scoring::ScoreFunctionOP;

	if ( ! antibody_info_ ) {
		using utility::excn::EXCN_Msg_Exception;
		throw EXCN_Msg_Exception( "A valid AntibodyInfo instance is required to setup " + get_name() + "'s centroid loop "
			+ "refinement movers." );
	}

	/// FIXME: The chain break weight configuration and constraint weight should be handled by RefineOneCDRLoop.
	ScoreFunctionOP low_res_loop_refinement_scorefxn = ScoreFunctionFactory::create_score_function("cen_std", "score4L");
	low_res_loop_refinement_scorefxn->set_weight( scoring::chainbreak, 1.0 );
	low_res_loop_refinement_scorefxn->set_weight( scoring::overlap_chainbreak, 10./3. );
	low_res_loop_refinement_scorefxn->set_weight( scoring::atom_pair_constraint, 100 );

	// update low-res sfxn with kink constraint, unlike H3 modeling, constriants are not enable in low-res by default
	// also weights are hard coded, so this should be refactored later
	if ( has_auto_generate_kink_constraint() ) {
		low_res_loop_refinement_scorefxn->set_weight( scoring::dihedral_constraint, 1.0 );
		low_res_loop_refinement_scorefxn->set_weight( scoring::angle_constraint, 1.0 );
	}

	low_res_refine_cdr_h2_ = RefineOneCDRLoopOP( new RefineOneCDRLoop(
		antibody_info_,
		h2,
		loop_refinement_method_,
		low_res_loop_refinement_scorefxn
		) );
	low_res_refine_cdr_h2_->set_h3_filter( false );

	low_res_refine_cdr_h3_ = RefineOneCDRLoopOP( new RefineOneCDRLoop(
		antibody_info_,
		h3,
		loop_refinement_method_,
		low_res_loop_refinement_scorefxn
		) );
	low_res_refine_cdr_h3_->set_h3_filter( h3_filter_ );
	low_res_refine_cdr_h3_->set_num_filter_tries( h3_filter_tolerance_ );
}

void SnugDockProtocol::init() {
	type( "SnugDockProtocol" );

	/// TODO: Allow the refinement method to be set via a mutator and from the options system
	using basic::options::option;
	using namespace basic::options::OptionKeys;
	if ( option[ basic::options::OptionKeys::antibody::refine ].user() ) {
		loop_refinement_method_  = option[ basic::options::OptionKeys::antibody::centroid_refine ]() ;
	} else {
		loop_refinement_method_ = "refine_kic";
	}
	/// Allow h3_filter to be turned on at cost of extra loop modeling
	if ( option[ basic::options::OptionKeys::antibody::h3_filter ].user() ) {
		h3_filter_  = option[ basic::options::OptionKeys::antibody::h3_filter ]() ;
	} else {
		h3_filter_ = false;
	}
	if ( option[ basic::options::OptionKeys::antibody::h3_filter_tolerance ].user() ) {
		h3_filter_tolerance_  = option[ basic::options::OptionKeys::antibody::h3_filter_tolerance ]() ;
	} else {
		h3_filter_tolerance_ = 20;
	}

	if ( option[ basic::options::OptionKeys::antibody::auto_generate_kink_constraint ].user() ) {
		set_auto_constraint( option[ basic::options::OptionKeys::antibody::auto_generate_kink_constraint ]() );
	} else {
		set_auto_constraint( false ); // default
	}
	if ( option[ basic::options::OptionKeys::antibody::all_atom_mode_kink_constraint ].user() ) {
		set_high_res_kink_constraint( option[ basic::options::OptionKeys::antibody::all_atom_mode_kink_constraint ]() );
	} else {
		set_high_res_kink_constraint( false ); //default
	}
}

void SnugDockProtocol::init_for_equal_operator_and_copy_constructor(
	SnugDockProtocol & lhs,
	SnugDockProtocol const & rhs
) {
	// copy all data members from rhs to lhs
	lhs.antibody_info_ = rhs.antibody_info_;

	// Movers
	lhs.low_res_refine_cdr_h2_ = rhs.low_res_refine_cdr_h2_;
	lhs.low_res_refine_cdr_h3_ = rhs.low_res_refine_cdr_h3_;
	lhs.docking_ = rhs.docking_;

	lhs.loop_refinement_method_ = rhs.loop_refinement_method_;
}

docking::DockingProtocolOP SnugDockProtocol::docking() const {
	if ( ! docking_ ) {
		/// The full DockingProtocol is used with a custom high resolution phase and post-low-resolution phase
		/// All FoldTrees will be setup through AntibodyInfo so DockingProtocol's autofoldtree setup is disabled.
		docking_ = docking::DockingProtocolOP( new docking::DockingProtocol );
		docking_->set_autofoldtree( false );
	}
	return docking_;
}

void
SnugDockProtocol::show( std::ostream & out ) const {
	out << *this;
}

std::ostream & operator<<(std::ostream& out, SnugDockProtocol const & snugdockprotocol ) {
	if ( snugdockprotocol.antibody_info_ ) {
		out << snugdockprotocol.get_name() << " has been configured to operate on an Antibody-Antigen complex with the "
			<< "following information:" << std::endl;
		out << * snugdockprotocol.antibody_info_ << std::endl;
	} else {
		out << snugdockprotocol.get_name() << " has not been used yet.  " << snugdockprotocol.get_name()
			<<"'s data initialization occurs the first time its apply method is called." << std::endl;
	}
	return out;
}

} // namespace antibody
} // namespace protocols
