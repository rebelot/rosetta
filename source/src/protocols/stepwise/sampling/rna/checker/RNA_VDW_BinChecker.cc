// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file RNA_VDW_BinChecker.cc
/// @brief Very fast version of VWD repulsion screening
/// @detailed
/// @author Parin Sripakdeevong


//////////////////////////////////
#include <protocols/stepwise/sampling/rna/StepWiseRNA_Classes.hh>
#include <protocols/stepwise/sampling/rna/util.hh>
#include <protocols/stepwise/sampling/util.hh> //Dec 23, 2011
#include <protocols/stepwise/sampling/working_parameters/StepWiseWorkingParameters.hh>
#include <protocols/stepwise/sampling/rna/checker/RNA_VDW_BinChecker.hh>
#include <protocols/stepwise/sampling/rna/rigid_body/util.hh>
#include <protocols/rotamer_sampler/rigid_body/util.hh>
#include <core/chemical/rna/util.hh>

#include <protocols/farna/util.hh>
//////////////////////////////////

#include <core/types.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/util.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/AtomType.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>
#include <core/scoring/rms_util.tmpl.hh>

#include <core/id/AtomID_Map.hh>

#include <core/kinematics/MoveMap.hh>
//#include <core/scoring/ScoreFunction.hh>

#include <numeric/conversions.hh>

#include <basic/Tracer.hh>

#include <iostream>
#include <fstream>
#include <sstream>
#include <ObjexxFCL/format.hh>
#include <set>
#include <time.h>


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stores a 3D grid, binned at high resolution (0.1 Angstroms) of where fixed atoms already are placed,
// permitting fast determination of whether atoms of moving residues clash with fixed atoms.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static basic::Tracer TR( "protocols.stepwise.sampling.rna.checker.RNA_VDW_BinChecker" );

using namespace core;

namespace protocols {
namespace stepwise {
namespace sampling {
namespace rna {
namespace checker {

  //constructor!
	RNA_VDW_BinChecker::RNA_VDW_BinChecker():
//		max_distance_(40.0),
		max_distance_( 55.0 ), //Nov 7, Nov_1_2r8s_TLR was barely out of range. atom_pos_bin.z= 1003
//		max_distance_(50.0), //Oct 27, Change from 60.0 to 50.0 after one slave node on Biox crashed, probably due to memory limit (but 60.0 should use only 200 MB!)
//		max_distance_(60.0), //Oct 26, Change from 40.0 to 60.0. In rare instances, minimization will move moving_res and reference res very far from initial pos of reference res.
		atom_bin_size_( 0.1 ),
		bin_min_( int(  - max_distance_/atom_bin_size_ ) ),
		bin_max_( int( max_distance_/atom_bin_size_ ) ),  //used to be (max_distance_/atom_bin_size_)-1, change on Nov 7, 2010
		bin_offset_( std::abs( bin_min_ ) + 1 ),
		num_clash_atom_cutoff_( 3 ), //num of clash required to be considered a clash
		write_to_file_( false ), //for debugging
		is_reference_xyz_setup_( false ),
		is_VDW_screen_bin_setup_( false ),
		user_inputted_VDW_screen_pose_( false ),
		VDW_rep_alignment_RMSD_CUTOFF_( 0.001 ),
		tolerate_off_range_atom_bin_( true ), //Use to be false before Dec 15, 2010
		num_atom_pos_bin_out_of_range_message_outputted_( 0 ),
		VDW_rep_screen_with_physical_pose_verbose_( true ),
		physical_pose_clash_dist_cutoff_( 1.2 ),
		use_VDW_rep_pose_for_screening_( false ), //Use actual VDW_checker_pose instead of bin for screening. This mode is slow but more robust. Feb 20, 2011
		output_pdb_( false )
	{

		//March 23,2011:
		//For VDW_rep_screen_with_physical_pose: use 1.2 Angstrom Cutoff (In constrast use 0.8 Angstrom in create_VDW_screen_bin). Reasoning is as follow:
		//Problem is that 0.8 Angstrom is not large enough to account for short hbond dist that it lesser than sum of VDW_radius
		//For Example the shortest H_bond can be 1.6 Angstrom. If donor is nitrogen then H_radius=1.20, N_radius=1.55, SUM=2.75. So cutoff should be 2.75-1.6=1.15~1.2
		//In create_VDW_screen_bin, this problem is ameliorated by using VDW_radius=1.0 for all moving_res atoms.

		//In the VDW_bin, the moving_res atom radius is assumed to be 1.0 Angstrom so this give at least another 0.2 Angstrom
		//Crap...THIS CODE ASSUMES that residue next to the sampling res (on both 3' and 5' end) need to either be virtualized or not be part of the VDW_rep_checker in the first place.
		//1.For working_pose VDW_rep ....ok since the bulge res is always virtualized. THIS MEANS THAT RECENT LONG_LOOP RUNS ARE BUGGY!
		//2. CRAP FOR PREPEND, the PHOSPHATE IS NOT VIRTUALIZED since ignore_res_list is EMPTY!!
		//3. This mean that main problem is with the APPEND with building nucleotide (1st step and at chain_closure)

	//Play it safe, use num_clash_atom_cutoff_(3):
	// ....with num_clash_atom cutoff equals 1...actually got full_atom_rep_count:43540/43622  bin_rep: 55456/682546
	// ....with num_clash_atom cutoff equals 3...actually got full_atom_rep_count:43622/43622  bin_rep: 73868/682546


	}

//////////////////////Note////////////////////////////
//for max_distance=50.0, atom_bin_size=0.1
//-->bin_min=-500, bin_max=500, bin_offset=501
//x_pos 														-49.95   -0.05   0.05   49.95
//x_pos/atom_bin_size	     						-499       0      0   	499
//if(x_pos<0) -1											-500				-1			 0			499
//+bin_offset												   1			 500    501   1000

//////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //destructor
  RNA_VDW_BinChecker::~RNA_VDW_BinChecker()
  {}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void
	RNA_VDW_BinChecker::setup_using_working_pose( core::pose::Pose const & const_working_pose, working_parameters::StepWiseWorkingParametersCOP const & working_parameters ){

		output_title_text( "Enter RNA_VDW_BinChecker::setup_using_working_pose", TR.Debug );

		Size const reference_res( working_parameters->working_reference_res() ); //the last static_residues that this attached to the moving residues

		numeric::xyzVector< core::Real > const reference_xyz = get_reference_xyz( const_working_pose, reference_res );

		pose::Pose working_pose = const_working_pose; //Feb 20, 2011..just to make sure that the o2prime hydrogens are virtualized. Since this hydrogen can vary during sampling.

		virtualize_side_chains( working_pose ); // includes virtualization of 2'-OH for RNA

		create_VDW_screen_bin( working_pose, working_parameters->working_moving_res_list(), working_parameters->is_prepend(), reference_xyz, false /*verbose*/ );

		output_title_text( "Exit RNA_VDW_BinChecker::setup_using_working_pose", TR.Debug );

	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//June 04, 2011. Instead of deleting the matching_res..just return it!
	utility::vector1< core::Size >
	RNA_VDW_BinChecker::get_matching_res_in_VDW_rep_screen_pose( core::pose::Pose const & VDW_rep_screen_pose,
																				 														 core::pose::Pose const & working_pose,
																				 														 utility::vector1< core::Size > const & VDW_rep_screen_align_res,
																				 														 utility::vector1< core::Size > const & working_align_res,
																																		 std::map< core::Size, core::Size > & full_to_sub ) const {

		output_title_text( "Enter RNA_VDW_BinChecker::get_matching_res_in_VDW_rep_screen_pose", TR.Debug );
		clock_t const time_start( clock() );

		using namespace ObjexxFCL;

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//First thing is to parse the VDW_rep_ignore_matching_res_ options. Here are the possibilities:
		//1. If VDW_rep_ignore_matching_res_ is a single element list with the element="false", then do not delete the matching_res.
		//2. If not 1), then will perform the matching deletion. To ensure that code is working probably, user need to pass in exact the list of
		// residues segments then matching DO NOT OCCUR. This is basically the residues being rebuilt.
		// Example Format: [5-12, 17-21] means that matching will not occur exactly between res 5 to 12 and res 17 to 21.
		// For SWA, the input residues should be in terms of full_length pose. Will use working_parameters->full_to_sub to convert to working_pose res.
		//3. If VDW_rep_delete_match_res_ is empty, then the check in 2. is not made.

		TR.Debug << "VDW_rep_ignore_matching_res_.size() = " << VDW_rep_ignore_matching_res_.size() << std::endl;
		for ( Size n = 1; n <= VDW_rep_ignore_matching_res_.size(); n++ ){
			TR.Debug << "VDW_rep_ignore_matching_res_[" << n << "] = " << VDW_rep_ignore_matching_res_[n] << std::endl;
		}

		if ( VDW_rep_ignore_matching_res_.size() == 1 ){
			if ( VDW_rep_ignore_matching_res_[1] == "false" ){
				TR.Debug << "VDW_rep_ignore_matching_res_[1] == \"false\", will NOT delete residues in VDW_rep_pose that exist in the working_pose" << std::endl;
				if ( VDW_rep_ignore_matching_res_.size() != 1 ) utility_exit_with_message( "VDW_rep_ignore_matching_res_.size() != 1" );
				utility::vector1< Size > empty_list;
				empty_list.clear();
				output_title_text( "Exit RNA_VDW_BinChecker::delete_matching_res_in_VDW_rep_screen_pose", TR );
				return empty_list;
			}
		}

		bool const check_VDW_rep_no_match_res = ( VDW_rep_ignore_matching_res_.size() == 0 ) ? false: true;
		output_boolean( "check_VDW_rep_no_match_res = ", check_VDW_rep_no_match_res, TR.Debug ); TR.Debug << std::endl;

		core::Real const dist_cutoff = VDW_rep_alignment_RMSD_CUTOFF_;
		TR.Debug << "atom_atom_dist_cutoff = " << dist_cutoff << std::endl;


		utility::vector1< Size > no_match_res_list;

		for ( Size n = 1; n <= VDW_rep_ignore_matching_res_.size(); n++ ){
			utility::vector1< std::string > no_match_res_segment_pair = tokenize( VDW_rep_ignore_matching_res_[n], "-" );
			if ( no_match_res_segment_pair.size() != 2 ) utility_exit_with_message( "no_match_res_segment_pair.size() != 2" );

			Size const start_res =  string_to_int( no_match_res_segment_pair[1] );
			Size const end_res  =  string_to_int( no_match_res_segment_pair[2] );

			if ( start_res >= end_res )	utility_exit_with_message( "start_res = " + string_of( start_res ) + " >= " + string_of( end_res ) + "end_res" );

			for ( Size full_seq_num = start_res; full_seq_num <= end_res; full_seq_num++ ){
				Size const working_seq_num = full_to_sub[full_seq_num];
				if ( working_seq_num == 0 ) continue;

				if ( no_match_res_list.has_value( working_seq_num ) ) utility_exit_with_message( "seq_num " + string_of( working_seq_num ) + " is already in the no_match_res_list!" );
				no_match_res_list.push_back( working_seq_num );
			}
		}

		output_seq_num_list( "working_no_match_res_list = ", no_match_res_list, TR.Debug );
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//OK, now we match the residues in working_pose and VDW_rep_checker_pose.
		utility::vector1< Size > working_matching_res_list, VDW_rep_matching_res_list;

		for ( Size working_seq = 1; working_seq <= working_pose.total_residue(); working_seq++ ){

			Size num_match_res = 0;

			for ( Size VDW_rep_seq = 1; VDW_rep_seq <= VDW_rep_screen_pose.total_residue(); VDW_rep_seq++ ){

				conformation::Residue const & working_rsd = working_pose.residue( working_seq ); //static_pose
				conformation::Residue const & VDW_rep_rsd = VDW_rep_screen_pose.residue( VDW_rep_seq ); //moving_pose

				if ( VDW_rep_rsd.aa() != working_rsd.aa() ) continue;

				Size const working_first_sc_at = working_rsd.first_sidechain_atom() + 1;
				Size const VDW_rep_first_sc_at = VDW_rep_rsd.first_sidechain_atom() + 1;

				std::string const working_first_sc_name = working_rsd.type().atom_name( working_first_sc_at );
				std::string const VDW_rep_first_sc_name = VDW_rep_rsd.type().atom_name( VDW_rep_first_sc_at );

				if ( working_first_sc_name != VDW_rep_first_sc_name ){
					utility_exit_with_message( "working_first_sc_name = " + working_first_sc_name + " != " + VDW_rep_first_sc_name + " = VDW_rep_first_sc_name" );
				}


				//fast screen. (O(n^2))
				if ( ( working_rsd.xyz( working_first_sc_at ) - VDW_rep_rsd.xyz( VDW_rep_first_sc_at ) ).length_squared() > dist_cutoff*dist_cutoff ) continue;

				//Consistency TEST: through screen ensure that every atoms is at the same position. (O(n)*O(m))///////////////////
				bool found_OP2 = false; bool found_OP1 = false; bool found_O5 = false; bool found_P = false;

				for ( Size working_at = 1; working_at <= working_rsd.natoms(); working_at++ ){ //I wonder if we should just consider heavy atom? (rsd_1.nheavyatoms())

		 			std::string const working_atom_name = working_rsd.type().atom_name( working_at );

					//if(working_rsd.atom_type(working_at).element()=="H") continue; #This doesn't work since if Hydrogen is virtual, the type is "X" not "H"

					//TR << "atom=" << working_at  << "|name=" << working_rsd.type().atom_name(working_at) << "|type=" << working_rsd.atom_type(working_at).name();
					//TR << "|element()=" << working_rsd.atom_type(working_at).element() << "|" << std::endl;

					if ( working_atom_name == " OP2" ){ found_OP2 = true; continue; }
					if ( working_atom_name == " OP1" ){ found_OP1 = true; continue; }
					if ( working_atom_name == " O5'" ){ found_O5 = true; continue; }
					if ( working_atom_name == " P  " ){ found_P = true; continue; }

					if ( working_atom_name == " H5'" ){ continue; }
					if ( working_atom_name == "H5''" ){ continue; }
					if ( working_atom_name == "HO2'" ){ continue; }

					if ( working_rsd.is_virtual( working_at ) ) continue; //mainly to deal with "OVU1", "OVL1", "OVL2"

					if ( VDW_rep_rsd.has( working_atom_name ) == false ) utility_exit_with_message( "VDW_rep_rsd does not have atom = " + working_atom_name + "!" );

					Size const VDW_rep_at = VDW_rep_rsd.atom_index( working_atom_name );

		 			std::string const VDW_rep_atom_name = VDW_rep_rsd.type().atom_name( VDW_rep_at );
		 			if ( working_atom_name != VDW_rep_atom_name ){ //Check
		 				utility_exit_with_message( "working_atom_name != VDW_rep_atom_name, working_atom_name = " + working_atom_name + " VDW_rep_atom_name = " + VDW_rep_atom_name );
		 			}

					if ( ( working_rsd.xyz( working_at ) - VDW_rep_rsd.xyz( VDW_rep_at ) ).length_squared() > ( dist_cutoff*dist_cutoff ) ){
						TR.Debug << "dist_cutoff = " << dist_cutoff << "| working_seq = " << working_seq << "| VDW_rep_seq = " << VDW_rep_seq << " |";
						TR.Debug << "( working_rsd.xyz( working_at ) - VDW_rep_rsd.xyz( VDW_rep_at ) ).length() = " << ( working_rsd.xyz( working_at ) - VDW_rep_rsd.xyz( VDW_rep_at ) ).length() << std::endl;
		 				utility_exit_with_message( working_atom_name + " of working_rsd and " + VDW_rep_atom_name + " of VDW_rep_rsd are not within dist_cutoff" );
					}
				}

				if ( found_OP2 == false ) utility_exit_with_message( "found_OP2 == false" );
				if ( found_OP1 == false ) utility_exit_with_message( "found_OP1 == false" );
				if ( found_O5 == false )  utility_exit_with_message( "found_O5 == false" );
				if ( found_P == false )   utility_exit_with_message( "found_P == false" );
				///////////////////////////////////////////////////////////////////////////////////////////////////
				if ( working_matching_res_list.has_value( working_seq ) ){
					utility_exit_with_message( "working_seq " + string_of( working_seq ) + " is already in the delete_res_list! working_seq = " + string_of( working_seq ) + " VDW_rep_seq = " + string_of( VDW_rep_seq ) + "." );
				}
				if ( VDW_rep_matching_res_list.has_value( VDW_rep_seq ) ){
					utility_exit_with_message( "VDW_rep_seq " + string_of( VDW_rep_seq ) + " is already in the delete_res_list! working_seq = " + string_of( working_seq ) + " VDW_rep_seq = " + string_of( VDW_rep_seq ) + "." );
				}


				working_matching_res_list.push_back( working_seq );
				VDW_rep_matching_res_list.push_back( VDW_rep_seq );

				num_match_res++;

				if ( no_match_res_list.has_value( working_seq ) ){
					utility_exit_with_message( " working_seq = " + string_of( working_seq ) + " is in both no_match_res_list and delete_res_list!" );
				}

				for ( Size ii = 1; ii <= working_align_res.size(); ii++ ){
					if ( working_seq == working_align_res[ii] ){
						if ( VDW_rep_seq != VDW_rep_screen_align_res[ii] ){
							utility_exit_with_message( "working_seq == working_align_res[ii] but VDW_rep_seq != VDW_rep_screen_align_res[ii]" );
						}
					}
				}

			} //VDW_rep_seq

			if ( num_match_res > 1 ) utility_exit_with_message( "num_match_res > 1 for working_seq = " + ObjexxFCL::string_of(working_seq) );

		} //working_seq

		output_seq_num_list( "working_matching_res_list = ", working_matching_res_list, TR.Debug );
		output_seq_num_list( "VDW_rep_matching_res_list = ", VDW_rep_matching_res_list, TR.Debug );

		////////////////////////////////////////////More consistency check!///////////////////////////////////////////////////////////////
		for ( Size working_seq = 1; working_seq <= working_pose.total_residue(); working_seq++ ){

			if (	working_matching_res_list.has_value( working_seq ) == false ){

				//check that all working_alignment_res are properly removed.
				if ( working_align_res.has_value( working_seq ) ) utility_exit_with_message( "working_seq = " + string_of( working_seq ) + " is a working_align_res!" );

				if ( check_VDW_rep_no_match_res ){
					if ( no_match_res_list.has_value( working_seq ) == false ) utility_exit_with_message( "working_seq = " + string_of( working_seq ) + " is NOT in both no_match_res_list and matching_res_list!" );
				}
			}
		}


		for ( Size VDW_rep_seq = 1; VDW_rep_seq <= VDW_rep_screen_pose.total_residue(); VDW_rep_seq++ ){

			if (	VDW_rep_matching_res_list.has_value( VDW_rep_seq ) == false ){
				//check that all working_alignment_res are properly removed.
				if ( VDW_rep_screen_align_res.has_value( VDW_rep_seq ) ){
					utility_exit_with_message( "VDW_rep_seq = " + string_of( VDW_rep_seq ) + " is a VDW_rep_screen_align_res but is not in VDW_rep_matching_res_list!" );
				}
			}
		}
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		////////////////////OK, now we delete the matching residues in working_pose//////////////////////////////////////////////////////
		/*
			for ( Size VDW_rep_seq = VDW_rep_screen_pose.total_residue(); VDW_rep_seq >= 1; VDW_rep_seq-- ){
				if (	VDW_rep_delete_res_list.has_value( VDW_rep_seq ) ){
					VDW_rep_screen_pose.conformation().delete_residue_slow( VDW_rep_seq );
				}
			}

			if ( output_pdb_ ) VDW_rep_screen_pose.dump_pdb( "VDW_rep_screen_bin_AFTER_DELETE_MATCHING_VDW_rep_screen_pose.pdb" );

		*/


		TR.Debug << "Total time get_matching_res_in_VDW_rep_screen_pose: " << static_cast< Real > ( clock() - time_start ) / CLOCKS_PER_SEC << " SECONDS" << std::endl;

		output_title_text( "Exit RNA_VDW_BinChecker::delete_matching_res_in_VDW_rep_screen_pose", TR.Debug );


		return VDW_rep_matching_res_list;
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	}




	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	void
	RNA_VDW_BinChecker::align_VDW_rep_screen_pose( core::pose::Pose & VDW_rep_screen_pose,
																				 							 core::pose::Pose const & working_pose,
																				 							 utility::vector1< core::Size > const & VDW_rep_screen_align_res,
																				 							 utility::vector1< core::Size > const & working_align_res,
																				 							 bool const verbose ) const {

		using namespace core::pose;
		using namespace ObjexxFCL;
		using namespace core::scoring;


		////////////////////////////////////Align VDW_rep_screen_pose to working_pose/////////////////////////////////////////////////////////////////////
		if ( verbose ) output_title_text( "Enter RNA_VDW_BinChecker::align_VDW_rep_screen_pose()", TR );

		if ( verbose ) TR << "VDW_rep_alignment_RMSD_CUTOFF_ = " << VDW_rep_alignment_RMSD_CUTOFF_ << std::endl;

		if ( VDW_rep_screen_align_res.size() != working_align_res.size() ){
			utility_exit_with_message( "Size of VDW_rep_screen_align_res ( " + string_of( VDW_rep_screen_align_res.size() ) + " ) != working_align_res ( " + string_of( working_align_res.size() ) + " )" );
		}

		if ( output_pdb_ ){
			working_pose.dump_pdb( "VDW_rep_screen_bin_BEFORE_ALIGN_working_pose.pdb" );
			VDW_rep_screen_pose.dump_pdb( "VDW_rep_screen_bin_BEFORE_ALIGN_VDW_rep_screen_pose.pdb" );
		}


		std::map< core::Size, core::Size > res_map;

		//debug
		if ( verbose ) TR << "VDW_rep_screen_align_res--->working_align_res" << std::endl;

		for ( Size ii = 1; ii <= working_align_res.size(); ii++ ){

			Size const res_num_1 = VDW_rep_screen_align_res[ii]; //moving_pose
			Size const res_num_2 = working_align_res[ii]; //static_pose

			if ( res_num_1 > VDW_rep_screen_pose.total_residue() ){
			 	utility_exit_with_message( "res_num_1 ( " + string_of( res_num_1 ) + " ) > VDW_rep_screen_pose.total_residue() ( " + string_of( VDW_rep_screen_pose.total_residue() ) + " )!" );
			}

			if ( res_num_2 > working_pose.total_residue() ){
			 	utility_exit_with_message( "res_num_2 ( " + string_of( res_num_2 ) + " ) > working_pose.total_residue() ( " + string_of( working_pose.total_residue() ) + " )!" );
			}

			if ( verbose ) TR << res_num_1 << " ---> " << res_num_2 << std::endl;

			res_map[ res_num_1 ] = res_num_2;

		}

		id::AtomID_Map < id::AtomID > alignment_atom_id_map = protocols::stepwise::sampling::create_alignment_id_map(	VDW_rep_screen_pose, working_pose, res_map );


		if ( verbose ) TR << "before superimpose_pose" << std::endl;

		/*Real const alignment_rmsd = // Unused variable causes warning.*/
		core::scoring::superimpose_pose( VDW_rep_screen_pose, working_pose, alignment_atom_id_map );

		if ( verbose ) TR << "after superimpose_pose" << std::endl;

		if ( output_pdb_ ){
			working_pose.dump_pdb( "VDW_rep_screen_bin_AFTER_ALIGN_working_pose.pdb" );
			VDW_rep_screen_pose.dump_pdb( "VDW_rep_screen_bin_AFTER_ALIGN_VDW_rep_screen_pose.pdb" );
		}

		//////////////////////////////////Check that VDW_rep_screen pose is perfectly align to working_pose...////////////////////////////////

		for ( Size n = 1; n <= working_align_res.size(); n++ ){

			Size const res_num_1 = VDW_rep_screen_align_res[n]; //moving_pose
			Size const res_num_2 = working_align_res[n]; //static_pose


			Size atom_count = 0;
			Real sum_sd = 0.0;

			base_atoms_square_deviation( VDW_rep_screen_pose, working_pose, res_num_1, res_num_2, atom_count, sum_sd, false /*verbose*/, false /*ignore_virtual_atom*/ );

			sum_sd = sum_sd/( atom_count );
			Real rmsd = sqrt( sum_sd );

			if ( atom_count == 0 ) rmsd = 99.99; //This is different from suite_rmsd function..!!

			if ( verbose ){
				TR << "rmsd = " << rmsd  << " Angstrom between res " << res_num_1 << " of VDW_rep_screen_align_res and res " << res_num_2 << " of working_pose" << std::endl;
			}


			if ( rmsd > VDW_rep_alignment_RMSD_CUTOFF_ ){ //change on Sept 26, 2010..problem arise when use this in non-long-loop mode...
				TR.Debug << "rmsd = " << rmsd  << " > ( " << VDW_rep_alignment_RMSD_CUTOFF_ << " ) Angstrom between res " << res_num_1 << " of VDW_rep_screen_align_res and res " << res_num_2 << " of working_pose" << std::endl;
				utility_exit_with_message( "rmsd > VDW_rep_alignment_RMSD_CUTOFF!" );
			}

		}
		if ( verbose ) output_title_text( "Exit RNA_VDW_BinChecker::align_VDW_rep_screen_pose()", TR );


	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*
	void
	RNA_VDW_BinChecker::align_to_first_working_pose( core::pose::Pose & pose, std::string const & tag ) const {

		using namespace core::pose;
		using namespace protocols::stepwise::sampling::rna;

		if ( first_working_align_res_.size() == 0 ) utility_exit_with_message( "first_working_align_res_.size() == 0" );

		align_poses( pose, tag, first_working_pose_, "first_working_pose ( for VDW_rep_checker setup )", first_working_align_res_ );

	}
	*/

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void
	RNA_VDW_BinChecker::create_VDW_rep_screen_pose( VDW_RepScreeninfo & VDW_rep_screen_info, //This function update this class!
																				 								core::pose::Pose const & working_pose,
																												std::map< core::Size, core::Size > & full_to_sub,
																				 								bool const verbose ) const {

		using namespace core::pose;
		using namespace ObjexxFCL;
	  using namespace core::chemical;


		ResidueTypeSetCAP rsd_set;
		rsd_set = core::chemical::ChemicalManager::get_instance()->residue_type_set( RNA );

		TR.Debug << "importing VDW_rep_screen_pose: " << VDW_rep_screen_info.pose_name << std::endl;
		if ( VDW_rep_screen_info.pose_name == "" ) utility_exit_with_message( VDW_rep_screen_info.pose_name == "" );

		////////import VDW_rep_screen_pose.////////////////////////////////////////
		pose::Pose VDW_rep_screen_pose;
		import_pose::pose_from_pdb( VDW_rep_screen_pose, *rsd_set, VDW_rep_screen_info.pose_name );
		protocols::farna::make_phosphate_nomenclature_matches_mini( VDW_rep_screen_pose );
		///////////////////////////////////////////////////////////////////////////

		//Virtualize O2prime...o2prime hydrogen position can change ..particularly important for long loop mode.
		//Important since by virtualizing...the o2prime hydrogen will be ignored when creating the VDW_screen_bin.
		add_virtual_O2Prime_hydrogen( VDW_rep_screen_pose );

		utility::vector1< core::Size > const & VDW_rep_screen_align_res = VDW_rep_screen_info.VDW_align_res;
		utility::vector1< core::Size > const & working_align_res = VDW_rep_screen_info.working_align_res;

		align_VDW_rep_screen_pose( VDW_rep_screen_pose, working_pose, VDW_rep_screen_align_res, working_align_res, verbose );

		VDW_rep_screen_info.VDW_ignore_res = get_matching_res_in_VDW_rep_screen_pose( VDW_rep_screen_pose,  working_pose, VDW_rep_screen_align_res,  working_align_res, full_to_sub );

		VDW_rep_screen_info.VDW_pose = VDW_rep_screen_pose;

	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	RNA_VDW_BinChecker::FARFAR_setup_using_user_input_VDW_pose( utility::vector1< std::string > const & All_VDW_rep_screen_pose_info,
                                                                				core::pose::Pose const & const_working_pose ){

		using namespace core::pose;
		using namespace ObjexxFCL;

		user_inputted_VDW_screen_pose_ = true;
		bool const verbose = true;

		output_title_text( "Enter RNA_VDW_BinChecker::FARFAR_setup_using_user_input_VDW_pose", TR.Debug );

		if ( ( All_VDW_rep_screen_pose_info.size() % 3 ) != 0 ){
			utility_exit_with_message( "( All_VDW_rep_screen_pose_info.size() % 3 ) != 0. Example: VWC_rep_screen_pose.pdb 6 - 44( align_res of VWD_rep_screen_pose ) 1 - 33( align_res of working_pose )" );
		}

		VDW_rep_screen_info_list_.clear();
		Size import_ID = 0;

		///////////////////////////////////////////////////////////////////////////
		for ( Size n = 1; n <= All_VDW_rep_screen_pose_info.size(); n += 3 ){
			import_ID++;

			VDW_RepScreeninfo VDW_rep_screen_info = VDW_RepScreeninfo();

			VDW_rep_screen_info.import_ID = import_ID;
			VDW_rep_screen_info.input_string = "";
			VDW_rep_screen_info.input_string += "pose_name = " + All_VDW_rep_screen_pose_info[n];
			VDW_rep_screen_info.input_string += " VDW_align_res_string = " + All_VDW_rep_screen_pose_info[n + 1];
			VDW_rep_screen_info.input_string += " full_align_res_string = " + All_VDW_rep_screen_pose_info[n + 2];

			TR.Debug << "Adding VDW_screen_rep_pose_info #" << VDW_rep_screen_info.import_ID << ": " << VDW_rep_screen_info.input_string << std::endl;

			VDW_rep_screen_info.pose_name = All_VDW_rep_screen_pose_info[n];
			utility::vector1< std::string > const VDW_rep_screen_align_res_string = tokenize( All_VDW_rep_screen_pose_info[n + 1], "-" );
			utility::vector1< std::string > const full_align_res_string = tokenize( All_VDW_rep_screen_pose_info[n + 2], "-" );

			for ( Size ii = 1; ii <= VDW_rep_screen_align_res_string.size(); ii++ ){
				VDW_rep_screen_info.VDW_align_res.push_back( string_to_int( VDW_rep_screen_align_res_string[ii] ) );
			}

			for ( Size ii = 1; ii <= full_align_res_string.size(); ii++ ){
				VDW_rep_screen_info.full_align_res.push_back( string_to_int( full_align_res_string[ii] ) );
			}

			if ( VDW_rep_screen_info.VDW_align_res.size() == 0 ) utility_exit_with_message( "VDW_align_ress.size() == 0" );
			if ( VDW_rep_screen_info.full_align_res.size() == 0 ) utility_exit_with_message( "full_align_res.size() == 0!" );

			if ( VDW_rep_screen_info.VDW_align_res.size() != VDW_rep_screen_info.full_align_res.size() ){
				utility_exit_with_message( "Size of VDW_align_res ( " + string_of( VDW_rep_screen_info.VDW_align_res.size() ) + " ) != working_align_res ( " + string_of( VDW_rep_screen_info.working_align_res.size() ) + " )" );
			}

			//Assume the FARFAR working_pose is the full_length pose.
			VDW_rep_screen_info.working_align_res = VDW_rep_screen_info.full_align_res;
			///////////////////////////////////////////////////////////////////////////
			pose::Pose working_pose = const_working_pose;
			add_virtual_O2Prime_hydrogen( working_pose );
			//Virtualize O2prime...o2prime hydrogen position can change ..particularly important for long loop mode.
			//Important since by virtualizing...the o2prime hydrogen will be ignored when creating the VDW_screen_bin.

			std::map< core::Size, core::Size > full_to_sub;
			full_to_sub.clear();

			for ( Size n = 1; n <= working_pose.total_residue(); n++ ){
				full_to_sub[n] = n;
			}

			use_VDW_rep_pose_for_screening_ = true;	 ///Allow use of full pose for VDW rep screen instead of the bin.
			create_VDW_rep_screen_pose( VDW_rep_screen_info, working_pose, full_to_sub, verbose );

			VDW_rep_screen_info_list_.push_back( VDW_rep_screen_info );

		}

		if ( verbose ) output_title_text( "Exit RNA_VDW_BinChecker::FARFAR_setup_using_user_input_VDW_pose", TR );


	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void
	RNA_VDW_BinChecker::setup_using_user_input_VDW_pose( utility::vector1< std::string > const & All_VDW_rep_screen_pose_info,
                                                                core::pose::Pose const & const_working_pose,
                                                                working_parameters::StepWiseWorkingParametersCOP const & working_parameters ){

		using namespace core::pose;
		using namespace ObjexxFCL;
	  using namespace core::chemical;
		using namespace core::scoring;


		user_inputted_VDW_screen_pose_ = true;
		bool const verbose = true;


		output_title_text( "Enter RNA_VDW_BinChecker::setup_using_user_input_VDW_pose", TR.Debug );

		if ( ( All_VDW_rep_screen_pose_info.size() % 3 ) != 0 ){
			utility_exit_with_message( "All_VDW_rep_screen_pose_info.size() % 3 ) != 0. Example: VWC_rep_screen_pose.pdb 6 - 44( align_res of VWD_rep_screen_pose ) 1 - 33( align_res of working_pose )" );
		}

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		VDW_rep_screen_info_list_.clear();
		Size import_ID = 0;

		for ( Size n = 1; n <= All_VDW_rep_screen_pose_info.size(); n += 3 ){
			import_ID++;

			VDW_RepScreeninfo VDW_rep_screen_info = VDW_RepScreeninfo();

			VDW_rep_screen_info.import_ID = import_ID;
			VDW_rep_screen_info.input_string = "";
			VDW_rep_screen_info.input_string += "pose_name = " + All_VDW_rep_screen_pose_info[n];
			VDW_rep_screen_info.input_string += " VDW_align_res_string = " + All_VDW_rep_screen_pose_info[n + 1];
			VDW_rep_screen_info.input_string += " full_align_res_string = " + All_VDW_rep_screen_pose_info[n + 2];

			TR.Debug << "Checking VDW_screen_rep_pose_info #" << VDW_rep_screen_info.import_ID << ": " << VDW_rep_screen_info.input_string << std::endl;

			VDW_rep_screen_info.pose_name = All_VDW_rep_screen_pose_info[n];
			utility::vector1< std::string > const VDW_rep_screen_align_res_string = tokenize( All_VDW_rep_screen_pose_info[n + 1], "-" );
			utility::vector1< std::string > const full_align_res_string = tokenize( All_VDW_rep_screen_pose_info[n + 2], "-" );


			for ( Size ii = 1; ii <= VDW_rep_screen_align_res_string.size(); ii++ ){
				VDW_rep_screen_info.VDW_align_res.push_back( string_to_int( VDW_rep_screen_align_res_string[ii] ) );
			}

			for ( Size ii = 1; ii <= full_align_res_string.size(); ii++ ){
				VDW_rep_screen_info.full_align_res.push_back( string_to_int( full_align_res_string[ii] ) );
			}

			if ( VDW_rep_screen_info.VDW_align_res.size() == 0 ) utility_exit_with_message( "Size of VDW_align_res.size() == 0" );
			if ( VDW_rep_screen_info.full_align_res.size() == 0 ) utility_exit_with_message( "full_align_res.size() == 0!" );

			if ( VDW_rep_screen_info.VDW_align_res.size() != VDW_rep_screen_info.full_align_res.size() ){
				utility_exit_with_message( "Size of VDW_align_res ( " + string_of( VDW_rep_screen_info.VDW_align_res.size() ) + " ) != working_align_res ( " + string_of( VDW_rep_screen_info.working_align_res.size() ) + " )" );
			}

			VDW_rep_screen_info.working_align_res = apply_full_to_sub_mapping( VDW_rep_screen_info.full_align_res, working_parameters );

			if ( VDW_rep_screen_info.full_align_res.size() != VDW_rep_screen_info.working_align_res.size() ){
				TR.Debug << "Ignoring VDW_screen_rep_pose_info #" << VDW_rep_screen_info.import_ID << " since full_align_res.size()!=working_align_res.size()!";
				TR.Debug << " full_align_res.size() = " 		 << VDW_rep_screen_info.full_align_res.size();
				TR.Debug << " working_align_res.size() = " << VDW_rep_screen_info.working_align_res.size();
				TR.Debug << std::endl;
				continue;
			}

			utility::vector1< core::Size > const working_fixed_res( working_parameters->working_fixed_res() );
			//Ok check that the working_align_res are working_fixed_res...
			//This is important for VDW_screening in the minimizer!
			for ( Size ii = 1; ii <= VDW_rep_screen_info.working_align_res.size(); ii++ ){
				if ( working_fixed_res.has_value( VDW_rep_screen_info.working_align_res[ii] ) == false ){
					output_seq_num_list( "working_align_res = ", VDW_rep_screen_info.working_align_res, TR.Debug );
					output_seq_num_list( "working_fixed_res = ", working_fixed_res, TR.Debug );
					utility_exit_with_message( "A residue in working_align_res is not a working_fixed_res!" );
				}
			}
			//////////////////////////////////////////////////////////////////
			pose::Pose working_pose = const_working_pose;
			add_virtual_O2Prime_hydrogen( working_pose );
			//Virtualize O2prime...o2prime hydrogen position can change ..particularly important for long loop mode.
			//Important since by virtualizing...the o2prime hydrogen will be ignored when creating the VDW_screen_bin.

			std::map< core::Size, core::Size > const const_full_to_sub = working_parameters->const_full_to_sub();
			std::map< core::Size, core::Size > full_to_sub = const_full_to_sub;

			use_VDW_rep_pose_for_screening_ = true;	 ///Allow use of full pose for VDW rep screen instead of the bin.
			create_VDW_rep_screen_pose( VDW_rep_screen_info, working_pose, full_to_sub, verbose );

			VDW_rep_screen_info_list_.push_back( VDW_rep_screen_info );

		}


		////////////////Find the VDW_rep_screen_pose_infos that are in the root partition!/////////////////////////////////////////////
		TR.Debug << "--------------------------------------------------------" << std::endl;

		ObjexxFCL::FArray1D < bool > const & partition_definition = working_parameters->partition_definition();
		bool const root_partition = partition_definition( const_working_pose.fold_tree().root() );
		Size num_rep_screen_pose_info_in_root_partition = 0;

		for ( Size n = 1; n <= VDW_rep_screen_info_list_.size(); n++ ){

			VDW_RepScreeninfo const & VDW_rep_screen_info = VDW_rep_screen_info_list_[n];

			TR.Debug << "Check if VDW screen_rep_pose_info #" << VDW_rep_screen_info.import_ID << ": " << VDW_rep_screen_info.input_string  << " is in root partition." << std::endl;

			if ( VDW_rep_screen_info.working_align_res.size() != VDW_rep_screen_info.full_align_res.size() ) utility_exit_with_message( "ERROR!" );

			bool contain_moving_partition_res = false;
			for ( Size ii = 1; ii <= VDW_rep_screen_info.working_align_res.size(); ii++ ){
				Size const seq_num = VDW_rep_screen_info.working_align_res[ii];
				if ( partition_definition( seq_num ) != root_partition ) {
					contain_moving_partition_res = true;
					break;
				}
			}

			if ( contain_moving_partition_res == false ){
				TR.Debug << "VDW_screen_rep_pose_info #" << VDW_rep_screen_info.import_ID << " is in root partition! " << std::endl;
				VDW_rep_screen_info_list_[n].in_root_partition = true;
				num_rep_screen_pose_info_in_root_partition++;
			} else{
				VDW_rep_screen_info_list_[n].in_root_partition = false;
			}

		}

		if ( num_rep_screen_pose_info_in_root_partition == 0 ){

			//consistency check!//////////////////////////////////////////////
			utility::vector1< core::Size > const working_fixed_res = working_parameters->working_fixed_res();
			utility::vector1< core::Size > working_fixed_res_in_root_partition;

			for ( Size n = 1; n <= working_fixed_res.size(); n++ ){
				Size seq_num = working_fixed_res[n];
				if ( partition_definition( seq_num ) == root_partition ) {
					working_fixed_res_in_root_partition.push_back( seq_num );
				}
			}
			//////////////////////////////////////////////////////////////////

			if ( working_fixed_res_in_root_partition.size() == 0 ) {
				TR.Debug << "No VDW_checker_pose since working_fixed_res_in_root_partition.size() == 0, probably build inward " << std::endl;
				user_inputted_VDW_screen_pose_ = false;
				return;
			} else{
				utility_exit_with_message( "num_rep_screen_pose_info_in_root_partition == 0!" );
			}
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		Size const reference_res = working_parameters->working_reference_res();
		TR.Debug << "--------------------------------------------------------" << std::endl;
		TR.Debug << "Get reference xyz: " << std::endl;
		TR.Debug << "working_reference_res = " << reference_res << std::endl;

		numeric::xyzVector< core::Real > const reference_xyz = get_reference_xyz( const_working_pose, reference_res, true );
		TR.Debug << "--------------------------------------------------------" << std::endl;

		create_VDW_screen_bin( VDW_rep_screen_info_list_, reference_xyz, false /*verbose*/ );

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		if ( verbose ) output_title_text( "Exit RNA_VDW_BinChecker::setup_using_user_input_VDW_pose", TR.Debug );

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	RNA_VDW_BinChecker::update_VDW_screen_bin( core::pose::Pose const & pose,
																						 utility::vector1< core::Size > const & ignore_res_list,
																						 bool const is_prepend,  //associated with ignore_res_list
																						 std::ofstream & outfile_act ){

		bool output_once = false;

		TR.Debug << "pose.total_residue() = " << pose.total_residue() << std::endl;

		//	if(output_pdb_) pose.dump_pdb( "create_VDW_screen_bin_pose.pdb" );

		utility::vector1< std::pair < Size, std::string > > virtual_atom_list;
		utility::vector1< std::pair < Size, std::string > > moving_phosphate_atom_list;


		for ( Size n = 1; n <= pose.total_residue(); n++ ){
			core::conformation::Residue const & rsd = pose.residue( n );

			if ( ignore_res_list.has_value( n ) ) {
				continue;
			}

			for ( Size at = 1; at <= rsd.natoms(); at++ ){ //include hydrogen atoms.

				if ( rsd.is_virtual( at )  ){
					virtual_atom_list.push_back( std::make_pair( n, rsd.type().atom_name( at ) ) );
					continue;
				}

				if ( is_prepend && ignore_res_list.has_value( n - 1 ) ) {
					bool is_moving_phosphate = false;

					if ( rsd.type().atom_name( at ) == " P  " ) is_moving_phosphate = true;
					if ( rsd.type().atom_name( at ) == " OP2" ) is_moving_phosphate = true;
					if ( rsd.type().atom_name( at ) == " OP1" ) is_moving_phosphate = true;
					if ( rsd.type().atom_name( at ) == " O5'" ) is_moving_phosphate = true;
					if ( rsd.type().atom_name( at ) == " H5'" ) is_moving_phosphate = true;
					if ( rsd.type().atom_name( at ) == "H5''" ) is_moving_phosphate = true;

					if ( is_moving_phosphate ){
						moving_phosphate_atom_list.push_back( std::make_pair( n, rsd.type().atom_name( at ) ) );
						continue;
					}
				}

				Atom_Bin const atom_pos_bin = get_atom_bin( rsd.xyz( at ) );

				if ( write_to_file_ ){
					outfile_act << rsd.xyz( at )[0] << " ";
					outfile_act << rsd.xyz( at )[1] << " ";
					outfile_act << rsd.xyz( at )[2] << " ";
					outfile_act << "\n";
				}

				//////////////Purpose of three_dim_VDW_bin is for van der Waals repulsion screening///////
				Real const VDW_radius = rsd.atom_type( at ).lj_radius();
				Real const moving_atom_radius = 1.0; //Play it safe...could optimize by having a VWD_bin for each moving_atom tyep but don't think will significantly speed out code Apr 17, 2010
				Real const clash_dist_cutoff = 0.8; //Fail van der Waals repulsion screen if two atoms radius within 0.5 Angstrom of each other
				Real const max_binning_error = 2*( atom_bin_size_/2 )*sqrt( 3.0 ); //Feb 09, 2012: FIXED. Used to be "3" instead of "3.0"

				//Basically distance from center of box to the edge of box...2x since this is Lennard Jones distance between two atoms.

				Real const sum_radius = moving_atom_radius + VDW_radius - clash_dist_cutoff - max_binning_error;

				int const max_bin_offset = int( ( sum_radius + atom_bin_size_ )/atom_bin_size_ );
				int const min_bin_offset = int(  - ( ( sum_radius + atom_bin_size_ )/atom_bin_size_ ) );

				if ( output_once == false ){
					output_once = true;
					TR.Debug << "seq_num = " << n << " atom_name = " << rsd.type().atom_name( at ) << " VDW_radius = " << VDW_radius;
					TR.Debug << " moving_atom_radius = " << moving_atom_radius << " clash_dist_cutoff = " << clash_dist_cutoff;
					TR.Debug << " max_binning_error = " << max_binning_error << " sum_radius " <<  sum_radius << std::endl;
					TR.Debug << "max_bin_offset = " << max_bin_offset << " min_bin_offset = " << min_bin_offset << std::endl;
					//Note that VDW_radius of virtual atom is 0.0...this doesn't really matter since there is a screen virtual atom before this point anyways.
				}

				for ( int x_bin_offset = min_bin_offset; x_bin_offset <= max_bin_offset;  x_bin_offset++ ){
					for ( int y_bin_offset = min_bin_offset; y_bin_offset <= max_bin_offset;  y_bin_offset++ ){
						for ( int z_bin_offset = min_bin_offset; z_bin_offset <= max_bin_offset;  z_bin_offset++ ){

							Atom_Bin xyz_bin = atom_pos_bin;
							xyz_bin.x += x_bin_offset;
							xyz_bin.y += y_bin_offset;
							xyz_bin.z += z_bin_offset;

							if ( !is_atom_bin_in_range( xyz_bin ) ) continue;

							numeric::xyzVector< core::Real > const xyz_pos = get_atom_pos( xyz_bin );

							if ( ( xyz_pos - rsd.xyz( at ) ).length_squared() < sum_radius*sum_radius ){
								VDW_screen_bin_[xyz_bin.x][xyz_bin.y][xyz_bin.z] = true;
							}

						}
					}
				}
				///////////////////////////////////////////////////////////////////////////
			}
		}

		output_seq_num_list( "ignore_res_list = ", ignore_res_list, TR.Debug );

		TR.Debug << "VIRTUAL_ATOM_LIST: " << std::endl;
		for ( Size ii = 1; ii <= virtual_atom_list.size(); ii++ ){
			TR.Debug << virtual_atom_list[ii].first << "-" << virtual_atom_list[ii].second << " ";
		}
		TR.Debug << std::endl;

		TR.Debug << "MOVING_PHOSPHATE_ATOM_LIST: " << std::endl;
		for ( Size ii = 1; ii <= moving_phosphate_atom_list.size(); ii++ ){
			TR.Debug << moving_phosphate_atom_list[ii].first << "-" << moving_phosphate_atom_list[ii].second << " ";
		}
		TR.Debug << std::endl;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	RNA_VDW_BinChecker::create_VDW_screen_bin( core::pose::Pose const & pose,
																											utility::vector1< core::Size > const & ignore_res_list,
																											bool const is_prepend,  //associated with ignore_res_list
																											numeric::xyzVector< core::Real > const & reference_xyz,
																											bool const verbose ){

		utility::vector1< core::pose::Pose >	pose_list;
		utility::vector1< utility::vector1< core::Size > >	list_of_ignore_res_list;
		utility::vector1< bool > list_of_is_prepend;

		pose_list.push_back( pose );
		list_of_ignore_res_list.push_back( ignore_res_list );
		list_of_is_prepend.push_back( is_prepend );

		create_VDW_screen_bin( pose_list, list_of_ignore_res_list, list_of_is_prepend, reference_xyz, verbose );

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	RNA_VDW_BinChecker::create_VDW_screen_bin( utility::vector1< VDW_RepScreeninfo > const & VDW_rep_screen_info_list,
																											numeric::xyzVector< core::Real > const & reference_xyz,
																											bool const verbose ){


		utility::vector1< core::pose::Pose >	pose_list;
		utility::vector1< utility::vector1< core::Size > >	list_of_ignore_res_list;
		utility::vector1< bool > list_of_is_prepend;

		for ( Size n = 1; n <= VDW_rep_screen_info_list.size(); n++ ){
			if ( VDW_rep_screen_info_list[n].in_root_partition == true ){
				pose_list.push_back( VDW_rep_screen_info_list[n].VDW_pose );
				list_of_ignore_res_list.push_back( VDW_rep_screen_info_list[n].VDW_ignore_res );
				list_of_is_prepend.push_back( false ); /*Backward consistency with prior code that delete the matching res in VDW_rep_pose */
			}
		}

		create_VDW_screen_bin( pose_list, list_of_ignore_res_list, list_of_is_prepend, reference_xyz, verbose );

	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Update on June 03, 2011 to allow multiple input. Users still need to make sure that all the poses are in the root partition!
	void
	RNA_VDW_BinChecker::create_VDW_screen_bin( utility::vector1< core::pose::Pose > const & pose_list,
																											utility::vector1< utility::vector1< core::Size > > const & list_of_ignore_res_list,
																											utility::vector1< bool > const list_of_is_prepend,
																											numeric::xyzVector< core::Real > const & reference_xyz,
																											bool const verbose ){

		TR.Debug << "------------------Enter create_pose_bin function------------------ " << std::endl;
		TR.Debug << "max_distance_ = " << max_distance_ << std::endl;
		TR.Debug << "atom_bin_size_ = " << atom_bin_size_ << std::endl;
		TR.Debug << "bin_min_ = " << bin_min_ << std::endl;
		TR.Debug << "bin_max_ = " << bin_max_ << std::endl;
		TR.Debug << "bin_offset_ = " << bin_offset_ << std::endl;
		TR.Debug << "num_clash_atom_cutoff_ = " << num_clash_atom_cutoff_ << std::endl;
		TR.Debug << "pose_list.size() = " << pose_list.size() << std::endl;
		output_boolean( "verbose = ", verbose, TR.Debug ); TR.Debug << std::endl;
		output_boolean( "write_to_file_ = ", write_to_file_, TR.Debug ); TR.Debug << std::endl;
		output_boolean( "user_inputted_VDW_screen_pose_ = ", user_inputted_VDW_screen_pose_, TR.Debug ); TR.Debug << std::endl;
		output_boolean( "tolerate_off_range_atom_bin_ = ", tolerate_off_range_atom_bin_, TR.Debug ); TR.Debug << std::endl;
		/////////////////////////////////////////////

		///Some consistency_checks////////
		if ( pose_list.size() != list_of_ignore_res_list.size() ){
			utility_exit_with_message( "pose_list.size(){ " + ObjexxFCL::string_of( pose_list.size() ) + "}  != { " + ObjexxFCL::string_of( list_of_ignore_res_list.size() ) + "} list_of_ignore_res_list.size()" );
		}

		if ( pose_list.size() != list_of_is_prepend.size() ){
			utility_exit_with_message( "pose_list.size(){ " + ObjexxFCL::string_of( pose_list.size() ) + "}  != { " + ObjexxFCL::string_of( list_of_is_prepend.size() ) + "} list_of_ignore_res_list.size()" );
		}
		//////////////////////////////////

		is_VDW_screen_bin_setup_ = true;
		set_reference_xyz( reference_xyz );

		//Use a vector as the storage object...memory expensive but fast lookup time..
		utility::vector1< bool > one_dim_bin( bin_max_*2, false );
		utility::vector1< utility::vector1< bool > > two_dim_bin( bin_max_*2, one_dim_bin );
		if ( VDW_screen_bin_.size() != 0 ) utility_exit_with_message( "VDW_screen_bin_.size() != 0 before setup!" );
		VDW_screen_bin_.assign( bin_max_*2, two_dim_bin );

		std::ofstream outfile_act;
		if ( write_to_file_ ) outfile_act.open( "atom_pos_bin_act.txt" );

		for ( Size n = 1; n <= pose_list.size(); n++ ){
			TR.Debug << "--------------Enter update_VDW_screen_bin() for pose #" << n << " (count only include VDW_poses in root partition!)--------------" << std::endl;
			update_VDW_screen_bin( pose_list[n], list_of_ignore_res_list[n], list_of_is_prepend[n], outfile_act );
			TR.Debug << "--------------Exit  update_VDW_screen_bin() for pose #" << n << " (count only include VDW_poses in root partition!)--------------" << std::endl;
		}

		if ( write_to_file_ ) outfile_act.close();
		if ( write_to_file_ ) output_atom_bin( "atom_VDW_bin.txt" );
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//Check the size
		Size occupied_bin_count = 0;
		Size total_bin_count = 0;
		for ( int x_bin = 1; x_bin <= bin_max_*2; x_bin++ ){
			for ( int y_bin = 1; y_bin <= bin_max_*2; y_bin++ ){
				for ( int z_bin = 1; z_bin <= bin_max_*2; z_bin++ ){
					if ( VDW_screen_bin_[x_bin][y_bin][z_bin] == true ) occupied_bin_count++;
					total_bin_count++;
				}
			}
		}

		if ( total_bin_count == 0 ) utility_exit_with_message( "total_bin_count of thre_dim_VDW_bin == 0!" );

		TR.Debug << "VDW_screen_bin_: occupied_bin_count = " << occupied_bin_count << " out of total_bin_count = " << total_bin_count << std::endl;
		TR.Debug << "------------------Exit create_pose_bin function------------------ " << std::endl;

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////June 04, 2011 Be careful...even nucleotides in the root partition CAN move during minimization, so VDW_screen_bin_ can be OFF!////////////////////////////////
	////////////////SO USE VDW_rep_screen() only during sampling. For minimization use VDW_rep_screen_with_act_pose()!
	//fast version
	bool
	RNA_VDW_BinChecker::VDW_rep_screen( core::pose::Pose const & screening_pose, //Warning..this pose coordinate is not update...use here for VIRTUAL atom screening.
																			Size const & moving_res,
																			core::conformation::Residue const & rsd_at_origin,
																			core::kinematics::Stub const & moving_res_base_stub ){

		check_VDW_screen_bin_is_setup();

		core::conformation::Residue const & moving_rsd = screening_pose.residue( moving_res );

		utility::vector1< std::pair < id::AtomID, numeric::xyzVector< core::Real > > > xyz_list;
		rotamer_sampler::rigid_body::get_atom_coordinates( xyz_list, moving_res, rsd_at_origin, moving_res_base_stub );

		Size num_clash_atom = 0;
		for ( Size n = 1; n <= xyz_list.size(); n++ ){

			//check
			if ( xyz_list[n].first.rsd() != moving_res ){
				TR.Debug << "xyz_list[n].first.rsd() = " << xyz_list[n].first.rsd() << " moving_res = " << moving_res << std::endl;
				utility_exit_with_message( "xyz_list[n].first.rsd() != moving_res" );
			}

			//Virtual atom screen
			Size const at = xyz_list[n].first.atomno();
			if ( moving_rsd.is_virtual( at ) ) continue; //Is this slow???

			Atom_Bin const atom_pos_bin = get_atom_bin( xyz_list[n].second );

			if ( check_atom_bin_in_range( atom_pos_bin ) == false ) continue;

			//VDW Replusion screening...
			if ( VDW_screen_bin_[atom_pos_bin.x][atom_pos_bin.y][atom_pos_bin.z] ){ //CLASH!
				num_clash_atom++;
			}

			if ( num_clash_atom >= num_clash_atom_cutoff_ ) return false; //before this used to be at the beginning of the for loop, move this down on Sept 29, 2010

		}

		return true;
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Slow version (in the sense that position of screening_pose had to be updated before this function is called)..
	bool
	RNA_VDW_BinChecker::VDW_rep_screen( core::pose::Pose const & screening_pose, core::Size const & moving_res ){

		check_VDW_screen_bin_is_setup();

		core::conformation::Residue const & moving_rsd = screening_pose.residue( moving_res );

		Size num_clash_atom = 0;
		for ( Size at = 1; at <= moving_rsd.natoms(); at++ ){

			//Virtual atom screen
			if ( moving_rsd.is_virtual( at )) continue;

			Atom_Bin const atom_pos_bin = get_atom_bin( moving_rsd.xyz( at ) );

			if ( check_atom_bin_in_range( atom_pos_bin ) == false ) continue;

			//VDW Replusion screening...
			if ( VDW_screen_bin_[atom_pos_bin.x][atom_pos_bin.y][atom_pos_bin.z] ){ //CLASH!
				num_clash_atom++;
			}

			if ( num_clash_atom >= num_clash_atom_cutoff_ ) return false; //before this use to be at the beginning of the for loop, move this down on Sept 29, 2010

		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///Warning, this version is very SLOW!
	bool
	RNA_VDW_BinChecker::VDW_rep_screen_with_act_pose( core::pose::Pose const & screening_pose,
																														 utility::vector1< core::Size > const & moving_res_list,
																														 bool const local_verbose ){

		if ( use_VDW_rep_pose_for_screening_ == false ) utility_exit_with_message( "use_VDW_rep_pose_for_screening_ == false" );

		if ( moving_res_list.size() == 0 ) utility_exit_with_message( "moving_res_list.size() == 0!" );

		if ( VDW_rep_screen_with_physical_pose_verbose_ ){
			TR.Debug << "------------VDW_rep_screen_with_physical_pose_INFO------------" << std::endl;
			VDW_rep_screen_with_physical_pose_verbose_ = false;
			TR.Debug << "physical_pose_clash_dist_cutoff_ = " << physical_pose_clash_dist_cutoff_ << std::endl;
			TR.Debug << "num_clash_atom_cutoff_ = " << num_clash_atom_cutoff_ << std::endl;
			TR.Debug << "VDW_rep_screen_info_list_.size() = " << VDW_rep_screen_info_list_.size() << std::endl;
			output_seq_num_list( "moving_res_list = ", moving_res_list, TR.Debug );
			TR.Debug << "--------------------------------------------------------------" << std::endl;
		}

		for ( Size n = 1; n <= VDW_rep_screen_info_list_.size(); n++ ){

			VDW_RepScreeninfo const & VDW_rep_screen_info = VDW_rep_screen_info_list_[n];

			core::pose::Pose VDW_rep_screen_pose = VDW_rep_screen_info.VDW_pose; //Hard copy!
			utility::vector1< core::Size > const & VDW_rep_screen_align_res = VDW_rep_screen_info.VDW_align_res;
			utility::vector1< core::Size > const & working_align_res = VDW_rep_screen_info.working_align_res;

			//Align VDW_rep_screen_pose to screening_pose
			align_VDW_rep_screen_pose( VDW_rep_screen_pose, screening_pose, VDW_rep_screen_align_res, working_align_res, false /*verbose*/ );

			for ( Size seq_num = 1; seq_num <= VDW_rep_screen_pose.total_residue(); seq_num++ ){

				if ( VDW_rep_screen_info.VDW_ignore_res.has_value( seq_num ) ) continue;

				for ( Size ii = 1; ii <= moving_res_list.size(); ii++ ){

					Size const moving_res = moving_res_list[ii];

					bool const residues_in_contact = is_residues_in_contact( moving_res, screening_pose, seq_num, VDW_rep_screen_pose, physical_pose_clash_dist_cutoff_, num_clash_atom_cutoff_, local_verbose /*verbose*/ );

					if ( residues_in_contact ) return false;
				}
			}
		}

		return true;
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool
	RNA_VDW_BinChecker::user_inputted_VDW_screen_pose() const {
		//output_boolean("user_inputted_VDW_screen_pose_= ", user_inputted_VDW_screen_pose_, TR );
		return user_inputted_VDW_screen_pose_;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////


	void
	RNA_VDW_BinChecker::reference_xyz_consistency_check( numeric::xyzVector< core::Real > const & inputted_reference_xyz ) const {

		if ( is_reference_xyz_setup_ == false ) return;

		for ( int n = 0; n <= 2; n++ ){
			if ( ( ( inputted_reference_xyz[n] - 0.00001 ) > reference_xyz_[n] ) || ( ( inputted_reference_xyz[n] + 0.00001 ) < reference_xyz_[n] ) ) {
				utility_exit_with_message( "( ( ( inputted_reference_xyz[n] - 0.00001 ) > reference_xyz_[n] ) || ( ( inputted_reference_xyz[n] + 0.00001 ) < reference_xyz_[n] ) )" );
				TR.Debug << "inputted_reference_xyz = " << inputted_reference_xyz[0] << " " << inputted_reference_xyz[1] << " " << inputted_reference_xyz[2] << std::endl;
				TR.Debug << "reference_xyz_ = " << reference_xyz_[0] << " " << reference_xyz_[1] << " " << reference_xyz_[2] << std::endl;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	RNA_VDW_BinChecker::check_VDW_screen_bin_is_setup() const {

		if ( is_VDW_screen_bin_setup_ == false ) utility_exit_with_message( "is_VDW_screen_bin_setup_ == false!" );

		if ( VDW_screen_bin_.size() == 0 ) utility_exit_with_message( "is_VDW_screen_bin_setup_ == true but VDW_screen_bin_ == 0!" );

	}



	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	bool
	RNA_VDW_BinChecker::is_atom_bin_in_range( Atom_Bin const & atom_pos_bin ) const {


		if ( atom_pos_bin.x < 1 || ( atom_pos_bin.x > ( bin_max_*2 ) ) ||
			 atom_pos_bin.y < 1 || ( atom_pos_bin.y > ( bin_max_*2 ) ) ||
       atom_pos_bin.z < 1 || ( atom_pos_bin.z > ( bin_max_*2 ) ) ){

			 return false;

		} else{
			 return true;
		}

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////


	bool
 	RNA_VDW_BinChecker::check_atom_bin_in_range( Atom_Bin const & atom_pos_bin ){

			if ( !is_atom_bin_in_range( atom_pos_bin ) ){

				if ( num_atom_pos_bin_out_of_range_message_outputted_ <= 10 ){
					TR.Debug << "bin_max_*2 = " << bin_max_*2 << std::endl;
					TR.Debug << " atom_pos_bin.x = " << atom_pos_bin.x << " atom_pos_bin.y = " << atom_pos_bin.y << " atom_pos_bin.z = " << atom_pos_bin.z << std::endl;
					TR.Debug << "atom_pos_bin out of range!" << std::endl;
					num_atom_pos_bin_out_of_range_message_outputted_++;
					TR.Debug << "num_atom_pos_bin_out_of_range_message_outputted_so_far = " << num_atom_pos_bin_out_of_range_message_outputted_ << std::endl;
				}

				if ( tolerate_off_range_atom_bin_ ) return false;

				utility_exit_with_message( "atom_pos_bin out of range!" );

			}

			return true;

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	core::Vector
	RNA_VDW_BinChecker::get_reference_xyz( core::pose::Pose const & pose, core::Size const reference_res, bool const verbose /* = false */ ){
		if ( pose.residue_type( reference_res ).is_RNA() ){
			return core::chemical::rna::get_rna_base_centroid( pose.residue( reference_res ), verbose );
		}
		return pose.residue( reference_res ).xyz( 1 );
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	RNA_VDW_BinChecker::set_reference_xyz( numeric::xyzVector< core::Real > const & reference_xyz ){

		if ( is_reference_xyz_setup_ == true ) utility_exit_with_message( "is_reference_xyz is already setup!" );

		reference_xyz_ = reference_xyz;

		is_reference_xyz_setup_ = true;

	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////


	Atom_Bin
	RNA_VDW_BinChecker::get_atom_bin( numeric::xyzVector< core::Real > const & atom_pos ) const{

		runtime_assert ( is_reference_xyz_setup_ );


		numeric::xyzVector< core::Real > const atom_pos_ref_frame = atom_pos - reference_xyz_;

		Atom_Bin atom_bin;
		atom_bin.x = int( atom_pos_ref_frame[0]/atom_bin_size_ );
		atom_bin.y = int( atom_pos_ref_frame[1]/atom_bin_size_ );
		atom_bin.z = int( atom_pos_ref_frame[2]/atom_bin_size_ );


		if ( atom_pos_ref_frame[0] < 0 ) atom_bin.x--;
		if ( atom_pos_ref_frame[1] < 0 ) atom_bin.y--;
		if ( atom_pos_ref_frame[2] < 0 ) atom_bin.z--;

		//////////////////////////////////////////////////////////
		atom_bin.x += bin_offset_; //Want min bin to be at one.
		atom_bin.y += bin_offset_; //Want min bin to be at one.
		atom_bin.z += bin_offset_; //Want min bin to be at one.


		//////////////////////////////////////////////////////////
		return atom_bin;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	numeric::xyzVector< core::Real >
	RNA_VDW_BinChecker::get_atom_pos( Atom_Bin const & atom_bin ) const {

		if ( is_reference_xyz_setup_ == false ) utility_exit_with_message( "is_reference_xyz is not setup yet!" );

		numeric::xyzVector< core::Real > atom_pos;

		atom_pos[0] = ( atom_bin.x + 0.5 - bin_offset_ )*atom_bin_size_;
		atom_pos[1] = ( atom_bin.y + 0.5 - bin_offset_ )*atom_bin_size_;
		atom_pos[2] = ( atom_bin.z + 0.5 - bin_offset_ )*atom_bin_size_;

		atom_pos = atom_pos + reference_xyz_;

		return atom_pos;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void
	RNA_VDW_BinChecker::output_atom_bin( std::string const filename ) const{

		if ( is_reference_xyz_setup_ == false ) utility_exit_with_message( "is_reference_xyz is not setup yet!" );
		check_VDW_screen_bin_is_setup();

		std::ofstream outfile;

		outfile.open( filename.c_str() );

		Atom_Bin atom_bin;

		for ( atom_bin.x = 1; atom_bin.x <= int( bin_max_*2 ); atom_bin.x++ ){
		for ( atom_bin.y = 1; atom_bin.y <= int( bin_max_*2 ); atom_bin.y++ ){
		for ( atom_bin.z = 1; atom_bin.z <= int( bin_max_*2 ); atom_bin.z++ ){

			if ( VDW_screen_bin_[atom_bin.x][atom_bin.y][atom_bin.z] == true ){
				numeric::xyzVector< core::Real > const atom_pos = get_atom_pos( atom_bin );
				outfile << atom_pos[0] << " ";
				outfile << atom_pos[1] << " ";
				outfile << atom_pos[2] << " ";
				outfile << "\n";
			}
		}
		}
		}

		outfile.close();
	}



} //checker
} //rna
} //sampling
} //stepwise
} //protocols
