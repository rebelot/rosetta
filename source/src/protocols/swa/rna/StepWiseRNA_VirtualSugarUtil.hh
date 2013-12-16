// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/swa/rna/StepWiseRNA_VirtualSugarUtil.hh
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_protocols_swa_rna_StepWiseRNA_VirtualSugarUtil_HH
#define INCLUDED_protocols_swa_rna_StepWiseRNA_VirtualSugarUtil_HH

#include <protocols/swa/rna/StepWiseRNA_VirtualSugarUtil.fwd.hh>
#include <protocols/swa/rna/StepWiseRNA_JobParameters.fwd.hh>
#include <protocols/swa/rna/SugarModeling.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>
#include <map>

using namespace core;
using namespace core::pose;

namespace protocols {
namespace swa {
namespace rna {

	void
	minimize_all_sampled_floating_bases( core::pose::Pose & viewer_pose,
																			 utility::vector1< SugarModeling > const & modeling_list,
																			 utility::vector1< PoseOP > & pose_data_list,
																			 core::scoring::ScoreFunctionOP const & sampling_scorefxn,
																			 StepWiseRNA_JobParametersCOP const & job_parameters,
																			 bool const virtual_sugar_is_from_prior_step = true );

	bool
	is_sugar_virtual( core::pose::Pose const & pose, core::Size const sugar_res, core::Size const bulge_res,
										utility::vector1< Size > & bulge_residues_to_virtualize );

	bool
	is_sugar_virtual( core::pose::Pose const & pose, core::Size const previous_moving_res, core::Size const previous_bulge_res );

	void
	copy_bulge_res_and_sugar_torsion( SugarModeling const & sugar_modeling, core::pose::Pose & pose, core::pose::Pose const & template_pose,
																		bool instantiate_sugar = false );

	void
	enumerate_starting_pose_data_list( utility::vector1< PoseOP > & starting_pose_data_list,
																		 utility::vector1< SugarModeling > const & SugarModeling_list,
																		 core::pose::Pose const & pose );


	std::map< Size, Size > const
	get_reference_res_for_each_virtual_sugar( pose::Pose const & pose,
																						bool const check_for_non_jump = false,
																						Size const moving_suite = 0 /*cannot place jump across partititions*/ );

	utility::vector1< Size >
	get_possible_reference_res_list( Size const virtual_sugar_res,
																	 pose::Pose const & pose,
																	 bool const check_for_non_jump,
																	 Size const moving_suite /*cannot place jump across partititions*/ );


	Size
	look_for_jumps_to_previous( Size const virtual_sugar_res,
															pose::Pose const & pose,
															bool const force_upstream );

	Size
	look_for_jumps_to_next( Size const virtual_sugar_res,
													pose::Pose const & pose,
													bool const force_upstream );

	Size
	look_for_non_jump_reference_to_previous( Size const virtual_sugar_res,
																					 pose::Pose const & pose,
																					 Size const moving_suite );

	Size
	look_for_non_jump_reference_to_next( Size const virtual_sugar_res,
																			 pose::Pose const & pose,
																			 Size const moving_suite );

} //rna
} //swa
} //protocols

#endif
