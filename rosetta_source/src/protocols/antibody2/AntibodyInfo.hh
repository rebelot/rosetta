// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief
/// @author Jianqing Xu (xubest@gmail.com)

#ifndef INCLUDED_protocols_antibody2_AntibodyInfo_hh
#define INCLUDED_protocols_antibody2_AntibodyInfo_hh

// Rosetta Headers
#include <core/kinematics/MoveMap.hh>
#include <core/pose/Pose.hh>
#include <core/types.hh>
#include <protocols/loops/Loops.hh>
#include <utility/vector1.hh>
// C++ Headers

// Utility Headers
// AUTO-REMOVED #include <utility/vector1.hh>

///////////////////////////////////////////////////////////////////////////////
namespace protocols {
namespace antibody2 {

/// antibody2 definition
class AntibodyInfo {

public:
	typedef std::map < std::string, loops::LoopOP > LoopMap;
	/// default constructor
	AntibodyInfo();

	/// constructor with arguments
	AntibodyInfo( core::pose::Pose & pose );
	AntibodyInfo( core::pose::Pose & pose, bool camelid );
	AntibodyInfo( core::pose::Pose & pose, std::string cdr_name );

	void setup_loops( core::pose::Pose & pose, bool camelid );

	void all_cdr_fold_tree( core::pose::Pose & pose );
//	void cdr_h3_fold_tree( core::pose::Pose & pose );

	/// @brief return the loop of a certain loop type
	loops::LoopOP get_loop( std::string loop );

	// return kinked/extended
	bool is_kinked() { return kinked_; }
	bool is_extended() { return extended_; }

	/// align current Fv to native.Fv
	void align_to_native( core::pose::Pose & pose, antibody2::AntibodyInfo & native, core::pose::Pose & native_pose );

	// Start coordinates of active loop
	core::Size current_start;
	// End coordinates of active loop
	core::Size current_end;
	utility::vector1< char > Fv_sequence_;

	loops::Loops all_cdr_loops_;

//private:
private:
	// cdr loops
	LoopMap loops_;
	loops::LoopOP L1_, L2_, L3_, H1_, H2_, H3_;
	core::Size hfr_[7][3]; // array of framework residues for alignment

	bool camelid_;

	// information about h3
	bool kinked_;
	bool extended_;


	void set_default( bool camelid );
	void detect_CDR_H3_stem_type( core::pose::Pose & pose );
	void detect_camelid_CDR_H3_stem_type();
	void detect_regular_CDR_H3_stem_type( core::pose::Pose & pose );
};


} //namespace antibody2
} //namespace protocols


#endif //INCLUDED_protocols_loops_AntibodyInfo_HH
