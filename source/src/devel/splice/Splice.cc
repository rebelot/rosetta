// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file devel/splice/Splice.cc
/// @brief
/// @author Sarel Fleishman (sarel@weizmann.ac.il)

///Clarifications of variables used:
/*
 * Template_pose = The pose that is used as refrence for all the start and end positions of loop segments
 * Source_pose = The
 * startn = the position on the pose where the loop starts (N-ter)
 * startc = The position on the pose where the loop ends (C-ter)
 * from_res = The user supplies loop start residues accoding to Template PDB. Splice.cc updates this to be the correct residue accrding to the current pose (both
 * 			  Structures are lianged)
 * to_res = Same as from_res but apllies to the C-ter of the segment
 *

 */

// Unit headers
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/InnerJob.hh>
#include <protocols/jd2/Job.hh>
#include <devel/splice/Splice.hh>
#include <devel/splice/SpliceSegment.hh>
#include <devel/splice/TailSegmentMover.hh>
#include <core/pack/task/operation/NoRepackDisulfides.hh>
#include <devel/splice/SpliceCreator.hh>
#include <utility/string_util.hh>
#include <utility/exit.hh>
#include <core/kinematics/FoldTree.hh>
#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/chemical/AA.hh>
#include <protocols/protein_interface_design/filters/TorsionFilter.hh>
#include <protocols/protein_interface_design/util.hh>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>//for comparing string case insensitive
#include <protocols/toolbox/task_operations/RestrictChainToRepackingOperation.hh>
#include <protocols/rigid/RB_geometry.hh>
#include <protocols/simple_moves/MinMover.hh>
#include <core/pose/PDB_Info.hh>
#include <core/chemical/AtomType.fwd.hh>
#include <core/chemical/Atom.hh>
#include <core/chemical/AtomTypeSet.hh>
#include <core/chemical/ResidueTypeSet.hh>



// Package headers
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/conformation/util.hh>
#include <core/import_pose/import_pose.hh>
#include <core/conformation/Conformation.hh>
#include <core/pack/task/TaskFactory.hh>
#include <basic/Tracer.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <utility/tag/Tag.hh>
#include <utility/vector1.hh>
#include <basic/datacache/DataMap.hh>
#include <basic/datacache/DataMapObj.hh>
#include <protocols/simple_moves/RotamerTrialsMinMover.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <core/pose/selection.hh>
#include <protocols/protein_interface_design/movers/AddChainBreak.hh>
#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <boost/algorithm/string.hpp>
//Auto Headers
#include <core/conformation/Residue.hh>
#include <core/chemical/VariantType.hh>
#include <core/kinematics/MoveMap.hh>
#include <protocols/toolbox/task_operations/DesignAroundOperation.hh>
#include <protocols/toolbox/task_operations/ProteinInterfaceDesignOperation.hh>
#include <protocols/toolbox/task_operations/ThreadSequenceOperation.hh>
#include <protocols/toolbox/task_operations/SeqprofConsensusOperation.hh>
#include <protocols/loops/loop_mover/refine/LoopMover_CCD.hh>
#include <numeric/xyzVector.hh>
#include <protocols/loops/FoldTreeFromLoopsWrapper.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <protocols/protein_interface_design/movers/LoopLengthChange.hh>
#include <core/scoring/dssp/Dssp.hh>
#include <numeric/random/random.hh>
#include <numeric/random/random_permutation.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/SequenceProfileConstraint.hh>
#include <core/scoring/constraints/Constraints.hh>
#include <core/scoring/func/Func.hh>
#include <core/scoring/func/CircularHarmonicFunc.hh>
#include <numeric/constants.hh>
#include <core/scoring/constraints/DihedralConstraint.hh>
#include <core/scoring/constraints/CoordinateConstraint.hh>
#include <core/scoring/func/HarmonicFunc.hh>
#include <core/scoring/constraints/Constraint.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <core/scoring/constraints/util.hh>
#include <core/sequence/SequenceProfile.hh>
#include <core/scoring/Energies.hh>
#include <numeric/xyz.functions.hh>
#include <protocols/simple_moves/CutChainMover.hh>
//////////////////////////////////////////////////
#include <basic/options/option.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh> // for option[ out::file::silent  ] and etc.
#include <basic/options/keys/in.OptionKeys.gen.hh> // for option[ in::file::tags ] and etc.
#include <basic/options/keys/OptionKeys.hh>
#include <core/pose/util.hh>
///////////////////////////////////////////////////
#include <fstream>
#include <ctime>
#include <devel/splice/RBInMover.hh>
#include <devel/splice/RBOutMover.hh>

namespace devel {
	namespace splice {
	
	using namespace core::conformation;

		static basic::Tracer TR("devel.splice.Splice");
		static basic::Tracer TR_ccd("devel.splice.Splice_ccd");
		static basic::Tracer TR_constraints("devel.splice.Splice_constraints");
		static basic::Tracer TR_pssm("devel.splice.Splice_pssm");
		static basic::Tracer TR_min("devel.splice.Splice_min");
		static numeric::random::RandomGenerator RG(78289);
		std::string SpliceCreator::keyname() const {
			return SpliceCreator::mover_name();
		}

		protocols::moves::MoverOP SpliceCreator::create_mover() const {
			return new Splice;
		}

		std::string SpliceCreator::mover_name() {
			return "Splice";
		}

		Splice::Splice() :
				Mover(SpliceCreator::mover_name()),	from_res_(0), to_res_(0), saved_from_res_(0), saved_to_res_(0), source_pdb_(""), ccd_(true), scorefxn_(
				NULL), rms_cutoff_(999999), res_move_(4), randomize_cut_(false), cut_secondarystruc_(false), task_factory_(	NULL),
				design_task_factory_( NULL), torsion_database_fname_(""), database_entry_(0), database_pdb_entry_(""),
				template_file_(""), poly_ala_(true), equal_length_(false), template_pose_(NULL), start_pose_( NULL),
				saved_fold_tree_( NULL), design_(false), dbase_iterate_(false), allow_all_aa_(false), allow_threading_(true),
				rtmin_(true), first_pass_(true), locked_res_( NULL), locked_res_id_(' '), checkpointing_file_(""),
				loop_dbase_file_name_(""), loop_pdb_source_(""), mover_tag_(NULL), splice_filter_( NULL), Pdb4LetName_(""),
				use_sequence_profiles_(false), segment_type_("") {
			profile_weight_away_from_interface_ = 1.0;
			restrict_to_repacking_chain2_ = true;
			design_shell_ = 6.0;
			repack_shell_ = 8.0;
			add_sequence_constraints_only_ = false;
			torsion_database_.clear();
			delta_lengths_.clear();
			dbase_subset_.clear();
			splice_segments_.clear();
			pdb_segments_.clear();
			end_dbase_subset_ = new basic::datacache::DataMapObj<bool>;
			end_dbase_subset_->obj = false;
			basic::options::option[basic::options::OptionKeys::out::file::pdb_comments].value(true);
			rb_sensitive_ = false;
			protein_family_to_database_["antibodies"] = "protocol_data/splice/antibodies/";
			//Hard coding the correct order of segments to be spliced
			order_segments_["antibodies"].push_back("L1_L2");
			order_segments_["antibodies"].push_back("L3");
			order_segments_["antibodies"].push_back("H1_H2");
			order_segments_["antibodies"].push_back("H3");
			skip_alignment_ = false;
			set_fold_tree_only_=false;
			chain_num( 1 );
			}

		Splice::~Splice() {
		}

/// @brief copy a stretch of aligned phi-psi dofs from source to target. No repacking no nothing.
/// The core function, copy_segment, copies residues from the source to the target without aligning the residues, thereby delivering all of their dofs
		void Splice::copy_stretch(core::pose::Pose & target, core::pose::Pose const & source, core::Size const from_res, core::Size const to_res) {
			using namespace core::pose;
			using namespace protocols::rosetta_scripts;
			using namespace core::chemical;

			core::Size from_nearest_on_source(0), to_nearest_on_source(0);
//			if (skip_alignment()) { // use skip alignment if you know the segments are perfectly aligned by residue number and there are no loop length changes. Ask Sarel
//				from_nearest_on_source = from_res;
//				to_nearest_on_source = to_res;
//			}
//			else {
				core::Size const host_chain(chain_num() ); /// in certain cases, when the partner protein sterically overlaps with the designed protein, there are amibguities about which chain to search. The use of host_chain removes these ambiguities. Here, ugly hardwired
				from_nearest_on_source = find_nearest_res(source, target, from_res, host_chain);
				to_nearest_on_source = find_nearest_res(source, target, to_res, host_chain);
				TR << "target: " << from_res << " " << to_res << " source: " << from_nearest_on_source << " "
						<< to_nearest_on_source << std::endl;
				runtime_assert(from_nearest_on_source && to_nearest_on_source);
				// change loop length:
				TR << "to_nearest_on_source:" << to_nearest_on_source << ",from_nearest_on_source " << from_nearest_on_source<< ";to_res:" << to_res << ",from_res " << from_res << std::endl;
				int const residue_diff(to_nearest_on_source - from_nearest_on_source - (to_res - from_res)); // SF&CN changed from core::Size to int, as residue_diff can be negative.
				//	if( residue_diff == 0 ){
				//		TR<<"skipping copy_stretch since loop lengths are identical"<<std::endl;
				//		return;
				//	}
				core::kinematics::FoldTree const saved_ft(target.fold_tree());
				TR << "copy_stretch foldtree: " << saved_ft << std::endl;
				TR << "from res: " << from_res << " to res: " << to_res << " residue_diff: " << residue_diff << std::endl;
				protocols::protein_interface_design::movers::LoopLengthChange llc;
				llc.loop_start(from_res);
				llc.loop_end(to_res);
				llc.delta(residue_diff);
				//	target.dump_pdb( "before_copy_stretch_llc_test.pdb" );
				llc.apply(target);
					//target.dump_pdb( "after_copy_stretch_llc_test.pdb" );
//			}

			target.copy_segment(to_nearest_on_source - from_nearest_on_source + 1, source, from_res, from_nearest_on_source);
			if (debug_)
				target.dump_pdb(mover_name_+"after_copy_stretch_test.pdb" );
		}

/// The checkpointing file has the following structure: the first line contains an ordered list of the dbase_subset_ for splice to iterate over the loop database. The second line contains the last element tested (the loop-entry number in the database; not the iterator to it!) and the third line contains the best element tested (again, the loop number from the database, not the iterator!).
/// To recover from a checkpoint the following reads the dbase_subset_ then, if this is a first_pass_ the best entry becomes current, and if it is not a first_pass then the current entry is current.
		void Splice::load_from_checkpoint() {
			using namespace std;

			if (checkpointing_file_ == "")
				return;
			utility::io::izstream data(checkpointing_file_);
			if (!data)
				return;
			TR << "Loading from checkpoint" << std::endl;
			/// first read the dbase_subset from the checkpointing file
			{
				string line;
				getline(data, line);
				if (line.length() == 0) {
					TR << "Checkpointing file empty or corrupted. Not loading." << std::endl;
					return;
				}
				istringstream line_stream(line);
				dbase_subset_.clear();
				while (!line_stream.eof()) {
					core::Size entry;
					line_stream >> entry;
					dbase_subset_.push_back(entry);
				}
			}
			TR << "dbase subset order loaded from checkpoint is: ";
			BOOST_FOREACH( core::Size const i, dbase_subset_ ) {
				TR<<i<<' ';
			}


			{
				std::string line;
				getline(data, line);
				istringstream line_stream(line);
				core::Size entry;
				line_stream >> entry;
				current_dbase_entry_ = std::find(dbase_subset_.begin(), dbase_subset_.end(), entry);
			}
			TR << "current dbase entry loaded from checkpoint is: " << *current_dbase_entry_ << std::endl;
		}

		void Splice::save_to_checkpoint() const {
			if (checkpointing_file_ == "")
				return;
			TR << "Splice checkpointing to file: " << checkpointing_file_ << std::endl;
			std::ofstream data;
			data.open(checkpointing_file_.c_str(), std::ios::out);
			if (!data.good())
				utility_exit_with_message("Unable to open splice checkpointing file for writing: " + checkpointing_file_ + "\n");
			BOOST_FOREACH( core::Size const dbase_entry, dbase_subset_ ) {
				TR<<' '<<dbase_entry;
				data << ' ' << dbase_entry;
			}
			if (current_dbase_entry_ == dbase_subset_.end())
				data << '\n' << 99999 << std::endl;
			else
				data << '\n' << *current_dbase_entry_ << std::endl;
			data.close();
		}

///@brief controls which dbase entry will be used. Three options: 1. specific one according to user instruction; 2. randomized out of a subset of the dbase with fitting sequence lengths (if user specified 0); 3. iterating over dbase subset
		core::Size Splice::find_dbase_entry(core::pose::Pose const & pose) {
			core::Size dbase_entry(database_entry());
			if (first_pass_) {		/// setup the dbase subset where loop lengths fit the selection criteria
				for (core::Size i = 1; i <= torsion_database_.size(); ++i) {		// find entries that fit the length criteria
					using namespace protocols::rosetta_scripts;
					if( skip_alignment() ){/*skip_alignment=true means that we trust the torsiondb entirely*/
						dbase_subset_.push_back( i );
						continue;
					}

					ResidueBBDofs const & dofs(torsion_database_[i]);
					//TR<<"Dofs start loop: "<<dofs.start_loop()<<std::endl;
					core::Size const nearest_to_entry_start_on_pose(find_nearest_res(pose, *template_pose_, dofs.start_loop(), 1/*chain*/));
					core::Size const nearest_to_entry_stop_on_pose(
							find_nearest_res(pose, *template_pose_, dofs.stop_loop(), 1/*chain*/));
					core::Size const pose_residues = nearest_to_entry_stop_on_pose - nearest_to_entry_start_on_pose + 1;
					int const delta(dofs.size() - pose_residues);
					if (locked_res() >= nearest_to_entry_start_on_pose && locked_res() <= nearest_to_entry_stop_on_pose) {
						/// if locked_res is within the loop, don't select different loop lengths
						if (delta != 0)
							continue;
					}
					if (equal_length()) { // if equal_length, don't select different loop lengths
						if (delta != 0)
							continue;
						else
							TR << "DEBUG:: I'm appending " << torsion_database_[i].source_pdb() << " with delta of " << delta << std::endl;
					}
					bool const fit = std::find(delta_lengths_.begin(), delta_lengths_.end(), delta) != delta_lengths_.end();

					if (fit || database_pdb_entry_ != "" || dbase_entry != 0 ) {
						dbase_subset_.push_back(i);
					}
				}
				if (dbase_subset_.empty()) {
					TR << "Loop of appropriate length not found in database. Returning" << std::endl;
					retrieve_values();
					return 0;
				}
				TR << "Found " << dbase_subset_.size() << " entries in the torsion dbase that match the length criteria"
						<< std::endl;
				numeric::random::random_permutation(dbase_subset_.begin(), dbase_subset_.end(), RG);
				current_dbase_entry_ = dbase_subset_.begin();
				load_from_checkpoint();
				first_pass_ = false;
			} // fi first_pass
			if (dbase_iterate()) {
				load_from_checkpoint();
				if (current_dbase_entry_ == dbase_end()) {
					TR << "Request to read past end of dbase. Splice returns without doing anything." << std::endl;
					return 0;
				}
				dbase_entry = *current_dbase_entry_;
				if (!first_pass_)
					current_dbase_entry_++;
				if (current_dbase_entry_ == dbase_end()) {
					TR << "Reached last dbase entry" << std::endl;
					end_dbase_subset_->obj = true;
				}
			} // fi dbase_iterate
			else if (dbase_entry == 0) {
				if (database_pdb_entry_ == "") //randomize dbase entry
					{
							TR << "The dbase_subset size is " << dbase_subset_.size() << std::endl;
							core::Size entry_no_to_choose = (core::Size) (RG.uniform() * dbase_subset_.size() + 1);
							TR << "trying to pick entry " << entry_no_to_choose << std::endl;
							dbase_entry = dbase_subset_[ entry_no_to_choose ];
					}
				//dbase_entry = ( core::Size )( RG.uniform() * dbase_subset_.size() + 1 );
				else { // look for the pdb_entry name
					for (core::Size count = 1; count <= dbase_subset_.size(); ++count) {
						if (torsion_database_[dbase_subset_[count]].source_pdb() == database_pdb_entry_) {
							TR << "Found entry for " << database_pdb_entry_ << " at number " << dbase_subset_[count] << std::endl;
							dbase_entry = dbase_subset_[count];
							break;
						}
					}
					runtime_assert(dbase_entry <= dbase_subset_.size());
				}
			} //fi dbase_entry==0

			return dbase_entry;
		}

		void Splice::apply(core::pose::Pose & pose) {
			using namespace protocols::rosetta_scripts;
			using core::chemical::DISULFIDE;

			if (add_sequence_constraints_only()) {
				TR << "Only adding sequence constraints!!! Not doing any splice!!!" << std::endl;
				//Because of Dror's problem with reverse chain order I am now passing by chain
				ccd_ = false; //WE ARE NOT DOING CCD!
				add_sequence_constraints(pose);

				return;
			}
			protocols::simple_moves::CutChainMover ccm;
			vl_vh_cut = ccm.chain_cut(pose); //find_vl_vh cut point. important for when doing floppy tail mover;

			set_last_move_status(protocols::moves::MS_SUCCESS);
			TR << "Starting splice apply" << std::endl;
			save_values();
			if (locked_res()) {
				locked_res_id(pose.residue(locked_res()).name1());
				TR << "locked residue/locked_residue_id set to: " << locked_res() << ',' << locked_res_id() << std::endl;
			}

			/// from_res() and to_res() can be determined directly on the tag, through a taskfactory, or through a template file. If through a template file,
			/// we start by translating from_res/to_res from the template file to the in coming pose as in the following paragraph
			if (template_file_ != "") { /// using a template file to determine from_res() to_res()
				rb_adjust_template(pose);
				core::Size template_from_res(0), template_to_res(0);

				if (from_res() && to_res()) {
					template_from_res = find_nearest_res(pose, *template_pose_, from_res(), 1/*chain*/);
					template_to_res = find_nearest_res(pose, *template_pose_, to_res(), 1/*chain*/);
					runtime_assert(template_from_res);
					runtime_assert(template_to_res);
				}

				from_res(template_from_res);
				to_res(template_to_res);
			} // fi template_file != ""
			if (!from_res() && !to_res() &&protein_family_=="antibodies" &&ccd()/*this is for splice out*/) {//if user has not defined from res and to res then we use antibody disulfides as start and end points
				utility::vector1<core::Size> cys_pos; //store all cysteine positions in the AB chain, I assume that the order is VL and then VH, gideon
				for (core::Size i = 1; i <= pose.total_residue(); ++i) {
					if (pose.residue(i).has_variant_type(core::chemical::DISULFIDE)) {
						cys_pos.push_back(i);
					}
				}
				if( segment_type_ =="L1_L2") {
						from_res(cys_pos[1]+1);
						to_res(cys_pos[2]-1);
				}
				else if  (segment_type_=="H1_H2"){
						from_res(cys_pos[3]+1);
						to_res(cys_pos[4]-1);
				}
				else if (segment_type_=="H3") {
						from_res(cys_pos[4]+1);
						core::conformation::Conformation const & conf(pose.conformation());
						to_res(conf.num_chains()==1?pose.total_residue():conf.chain_end(1));
				}
				else if (segment_type_=="L3"){
						from_res(cys_pos[2]+1);
						to_res(vl_vh_cut);
				}
				else
						utility_exit_with_message("Does not recognize segment: "+segment_type_);
			}
			if (set_fold_tree_only_) {
				TR << "Only setting fold tree!!! Not doing any splice!!!" << std::endl;
				load_pdb_segments_from_pose_comments(pose); //load comments form pdb to pose
				//Because of Dror's problem with reverse chain order I am now passing by chain
				ccd_ = false; //WE ARE NOT DOING CCD!
				set_fold_tree(pose, vl_vh_cut);

				return;
			}


			core::pose::Pose const in_pose_copy(pose);
			pose.conformation().detect_disulfides(); // just in case; but I think it's unnecessary

			/// from_res/to_res can also be determined through task factory, by identifying the first and last residues that are allowed to design in this tf
			if (torsion_database_fname_ == "" && from_res() == 0 && to_res() == 0) { /// set the splice site dynamically according to the task factory
				utility::vector1<core::Size> designable(
						protocols::rosetta_scripts::residue_packer_states(pose, task_factory(), true/*designable*/,
								false/*packable*/));
				std::sort(designable.begin(), designable.end());
				from_res(designable[1]);
				to_res(designable[designable.size()]);
			}
			core::pose::Pose source_pose;
			core::Size nearest_to_from(0), nearest_to_to(0);
			int residue_diff(0); // residues on source_pose that are nearest to from_res and to_res; what is the difference in residue numbers between incoming pose and source pose. 21/11/13: CN&SF This was changed from Core:Size to int as residue diff very well could be shorter than 0! //
			ResidueBBDofs dofs; /// used to store the torsion/resid dofs from any of the input files
			dofs.clear();
			ResidueBBDofs tail_dofs;
			tail_dofs.clear();
			core::Size cut_site(0);
			if (torsion_database_fname_ == "") { // read dofs from source pose rather than database
				core::import_pose::pose_from_pdb(source_pose, source_pdb_);
				//Check if there are chain_breaks in the source PDB, if so exit with error msg. gideon 24jun14
				protocols::simple_moves::CutChainMover ccm;
				core::Size source_pdb_cut(ccm.chain_cut(source_pose));
				if (source_pdb_cut!=0)//found cut site in source pose
						utility_exit_with_message("found chain break in source PDB "+source_pdb_+",exiting\n");
				if (segment_type_=="H3"&& (tail_segment_=="c")){
					nearest_to_from = find_nearest_res(source_pose, pose,from_res()/*should be the 2 vh cys on template*/, 1);//start res is nearest disulfide on source_pdb
					nearest_to_to = source_pose.total_residue();
				}
				else if(segment_type_=="L3"&& (tail_segment_=="c")){
					nearest_to_from = find_nearest_res(source_pose, pose,from_res()/*should be the 2 vl cys on template*/, 1);//start res is nearest disulfide on source_pdb
					nearest_to_to = source_pose.total_residue();
					TR<<"from_res()"<<from_res()<<"nearest_to_from: "<<nearest_to_from<<std::endl;
				}
				else{
					nearest_to_from = find_nearest_res(source_pose, pose, from_res(), 1/*chain*/);
					nearest_to_to = find_nearest_res(source_pose, pose, to_res(), 1/*chain*/);

				}

				if (((nearest_to_from == 0) || (nearest_to_to == 0))) {
					std::ostringstream os;	os << "nearest_to_from: " << nearest_to_from << " nearest_to_to: " << nearest_to_to << ". Failing"	<< std::endl;
					utility_exit_with_message(os.str());
					/*	TR<<"nearest_to_from: "<<nearest_to_from<<" nearest_to_to: "<<nearest_to_to<<". Failing"<<std::endl;
					 set_last_move_status( protocols::moves::FAIL_DO_NOT_RETRY );
					 retrieve_values();
					 return;
					 */

				}
				TR << "nearest_to_from: " << nearest_to_from << " nearest_to_to: " << nearest_to_to << std::endl;
				TR << "from_res():"<<from_res()<<"to_res():"<<to_res()<<std::endl;
				residue_diff = nearest_to_to - nearest_to_from - (to_res() - from_res());
				TR<<"Residue diff is: "<<residue_diff<<std::endl;
				for (core::Size i = nearest_to_from; i <= nearest_to_to; ++i) {
					if (source_pose.residue(i).has_variant_type(DISULFIDE)) { /// in future, using disulfides would be a great boon as it rigidifies loops.
						std::ostringstream os;
						//temp removed following error to test large chunk splice
						//os<<"Residue "<<i<<" is a disulfide. Failing"<<std::endl;
						//utility_exit_with_message(os.str());
						/*TR<<"Residue "<<i<<" is a disulfide. Failing"<<std::endl;
						 set_last_move_status( protocols::moves::FAIL_DO_NOT_RETRY );
						 retrieve_values();
						 return;
						 */

					}
					/// Feed the source_pose dofs into the BBDofs array
					BBDofs residue_dofs;
					residue_dofs.resid(i); /// resid is probably never used
					residue_dofs.phi(source_pose.phi(i));
					residue_dofs.psi(source_pose.psi(i));
					residue_dofs.omega(source_pose.omega(i));

					//core::Size const nearest_on_target( find_nearest_res( pose, source_pose, i ) );

					/// convert 3let residue code to 1let code
					std::stringstream ss;
					std::string s;
					ss << source_pose.residue(i).name1();
					ss >> s;
					residue_dofs.resn(s);

					dofs.push_back(residue_dofs);
				} // for i nearest_to_from..nearest_to_to
				cut_site = dofs.cut_site() ? dofs.cut_site() + from_res() - 1 : to_res(); // isn't this always going to be to_res()? I think so...
			} // fi torsion_database_fname==NULL
			else { /// read from dbase
				core::Size const dbase_entry(find_dbase_entry(pose));
				if (dbase_entry == 0) // failed to read entry
						{
					TR << "Should we fail loudly if this happens??" << std::endl;
					return;
				}
				runtime_assert(dbase_entry <= torsion_database_.size());
				dofs = torsion_database_[dbase_entry];
				if (tail_segment_!=""){
					tail_dofs=tail_torsion_database_[dbase_entry];
					TR<<"tail dofs size is: "<<tail_dofs.size()<<std::endl;
				}
				std::string const source_pdb_name(dofs.source_pdb());
				Pdb4LetName_ = dofs_pdb_name = source_pdb_name;
				//		if( use_sequence_profiles_ ){
				load_pdb_segments_from_pose_comments(pose);
				modify_pdb_segments_with_current_segment(source_pdb_name);
				//		}
				TR << "extracting dofs from source pdb " << source_pdb_name << std::endl;
				//adding current segment to pose comments
				TR << "The currnet segment is: " << segment_type_ << " and the source pdb is " << Pdb4LetName_ << std::endl;
				core::pose::add_comment(pose, "segment_" + segment_type_, Pdb4LetName_);//change correct association between current loop and pdb file
				if (mover_tag_() != NULL)
					mover_tag_->obj = "segment_" + source_pdb_name;
				BOOST_FOREACH(BBDofs & resdofs, dofs)
				{ /// transform 3-letter code to 1-letter code
					using namespace core::chemical;
					if (resdofs.resn() == "CYD") { // at one point it would be a good idea to use disfulfides rather than bail out on them...; I think disulfided cysteins wouldn't be written as CYD. This requires something more clever...
						TR << "Residue " << resdofs.resid() << " is a disulfide. Failing" << std::endl;
						set_last_move_status(protocols::moves::FAIL_DO_NOT_RETRY);
						retrieve_values();
						return;
					}
					std::stringstream ss;
					std::string s;
					ss << oneletter_code_from_aa(aa_from_name(resdofs.resn()));
					ss >> s;
					resdofs.resn(s);
				} /// foreach resdof
				nearest_to_to = dofs.size(); /// nearest_to_to and nearest_to_from are used below to compute the difference in residue numbers...
				nearest_to_from = 1;
				/// set from_res/to_res/cut_site on the incoming pose
				if (template_file_ != "" && !skip_alignment() ) { /// according to the template pose
					//			to_res( from_res() + dofs.size() -1);
					runtime_assert(from_res());
					runtime_assert(to_res());
					cut_site = dofs.cut_site() - dofs.start_loop() + from_res();
					TR << "cut_size before randomize cut:" << cut_site << std::endl;
				} // fi template_file != ""
				else { /// according to the dofs array (taken from the dbase)
					from_res(dofs.start_loop());
					to_res(dofs.stop_loop());
					cut_site = dofs.cut_site();
					runtime_assert(from_res() && to_res() && cut_site);
				}
				// TR<<"dofs.size "<<dofs.size()<<" dofs.stop_loop() "<<dofs.stop_loop()<<" dofs.start_loop() "<<dofs.start_loop()<<std::endl;
				residue_diff = dofs.size() - (dofs.stop_loop() - dofs.start_loop() + 1);
			} // read from dbase
			runtime_assert(to_res() > from_res());
			//	if( saved_fold_tree_ )/// is saved_fold_tree_ being used?
			//		pose.fold_tree( *saved_fold_tree_ );

			/// The database is computed with respect to the template pose, so before applying dofs from the dbase it's important to make that stretch identical to
			/// the template. from_res() and to_res() were previously computed to be with respect to the incoming pose, so within this subroutine the refer to pose rather
			/// than template_pose (this is a bit confusing, but it works!)
			copy_stretch(pose, *template_pose_, from_res(), to_res());
			//	( *scorefxn() ) ( pose );

			using namespace utility;
			/// randomize_cut() should not be invoked with a database entry, b/c the dbase already specified the cut sites.
			/// this is important b/c nearest_to_from/nearest_to_to are degenerate if the dbase is used.
			if (randomize_cut()) {
				/// choose cutsite randomly within loop residues on the loop (no 2ary structure)
				core::scoring::dssp::Dssp dssp(source_pose);
				dssp.dssp_reduced(); // switch to simplified H E L notation
				std::vector<core::Size> loop_positions_in_source;
				loop_positions_in_source.clear();
				TR << "DSSP of source segment: ";
				for (core::Size i = nearest_to_from; i <= std::min(nearest_to_to, to_res() - from_res() + nearest_to_from);
						++i) {
					if (dssp.get_dssp_secstruct(i) == 'L' || cut_secondarystruc()) // allow site for cutting if it's either in a loop or if cutting secondary structure is allowed
						loop_positions_in_source.push_back(i);
					TR << dssp.get_dssp_secstruct(i);
				}
				TR << std::endl;
				//New test to see what is the sequence of the new loop
				TR.Debug << "The sequence of the source loop is: ";
				for (core::Size i = nearest_to_from; i <= std::min(nearest_to_to, to_res() - from_res() + nearest_to_from);
						++i) {
					TR.Debug << source_pose.residue(i).name1() << " ";
				}
				TR << std::endl;
				cut_site = loop_positions_in_source[(core::Size) (RG.uniform() * loop_positions_in_source.size())]
						- nearest_to_from + from_res();
				TR << "Cut placed at: " << cut_site << std::endl;
			} // fi randomize_cut
			else if (ccd()){
				cut_site = find_non_active_site_cut_site(source_pose);//find the first non functional residue on the source segment
				//b/c the source segment is not the same length as the pose we have to find the difference between the pose and source pose. we do this by finding the first conserved residue
				//for antibodies its the cysteines. For other protein (TIM-barrels and such) it will be something else.gideon 16jun14
				core::Size source_pose_nearest_disulfide(find_nearest_res(source_pose,pose,find_nearest_disulfide(pose,from_res()),1));
				TR<<"from res disulfide on source is:"<<source_pose_nearest_disulfide<<std::endl;
				int start_res_diff = find_nearest_disulfide(pose,from_res())-source_pose_nearest_disulfide;
				TR<<"Res diff is:"<<start_res_diff<<std::endl;
				cut_site+=start_res_diff- (nearest_to_to - nearest_to_from - (to_res() - from_res()));
				TR<<"Non functional  cut site on pose is: "<<cut_site<<std::endl;

			}

			//	pose.dump_pdb( "before_ft_test.pdb" ); //this is the strucutre before changing the loop
			if (cut_site==to_res())
					cut_site=cut_site-1;
			fold_tree(pose, from_res(), to_res(), cut_site); /// the fold_tree routine will actually set the fold tree to surround the loop
			if (segment_type_=="H1_H2"||segment_type_=="L1_L2"){
				fold_tree(pose, find_nearest_disulfide(pose,from_res()),find_nearest_disulfide(pose,to_res()), cut_site); /// the fold_tree routine will actually set the fold tree to surround the loop
			}
			//	pose.dump_pdb( "after_ft_test.pdb" );
			/// change the loop length
			protocols::protein_interface_design::movers::LoopLengthChange llc;
			//if we are adding a c-terminal tail then we change the length at the end of the segment
			if (boost::iequals(tail_segment_, "c")) {
				llc.loop_start(find_nearest_disulfide(pose,from_res())+1);
				if (segment_type_=="L3")
					llc.loop_end(vl_vh_cut);
				else if (segment_type_=="H3"){
					core::conformation::Conformation const & conf(pose.conformation());
						llc.loop_end(conf.chain_end(1));//Asuming that the ligand is chain 2;
				}
				else
					utility_exit_with_message("Attempting to copy c-ter tail stretch from source PDB but segment type is not H3 or L3. Failing\n");
				llc.delta(residue_diff);
				tail_fold_tree(pose,vl_vh_cut,0/*chain_break*/);
			}
			else{
			llc.loop_start(from_res());
			llc.loop_end(cut_site + residue_diff < from_res() ? to_res() : cut_site);
			llc.delta(residue_diff);
			}
			core::Size cut_vl_vh_after_llc(from_res() < vl_vh_cut ? vl_vh_cut + residue_diff : vl_vh_cut); //update cut site between vl and vh after loop insertion, only if the loop was inserted to the vl.
			TR << "Foldtree before loop length change: " << pose.fold_tree() << std::endl;
			llc.apply(pose);
			TR << "Foldtree after loop length change: " << pose.fold_tree() << std::endl;
			if (debug_)
				pose.dump_pdb(mover_name_+"_after_2ndllc_test.pdb");
			/// set torsions
			core::Size const total_residue_new(dofs.size()); //how long is the introduced segment
			TR << "Changing dofs"<<std::endl;
			for (core::Size i = 0; i < total_residue_new; ++i) {
				core::Size const pose_resi(from_res() + i);
						TR<<"Previous phi/psi/omega at resi: "<<pose_resi<<" "<<pose.phi( pose_resi )<<'/'<<pose.psi( pose_resi )<<'/'<<pose.omega( pose_resi )<<'\n';
				pose.set_phi(pose_resi, dofs[i + 1].phi());
				pose.set_psi(pose_resi, dofs[i + 1].psi());
				pose.set_omega(pose_resi, dofs[i + 1].omega());
				//		pose.dump_pdb( "dump"+ utility::to_string( i ) + ".pdb" );
				//TR<<"resi, phi/psi/omega: "<< pose_resi<<' '<<pose.phi( pose_resi )<<'/'<<pose.psi( pose_resi )<<'/'<<pose.omega( pose_resi )<<std::endl;
				TR<<"requested phi/psi/omega: "<<dofs[ i + 1 ].phi()<<'/'<<dofs[i+1].psi()<<'/'<<dofs[i+1].omega()<<std::endl;
			}
			if (debug_)
				pose.dump_pdb(mover_name_+"_after_segment_dofs_before_tail.pdb");
			if (!ccd() && tail_segment_!=""){//if not doing ccd then we are splicing in using torsion db dofs
				tail_fold_tree(pose, cut_vl_vh_after_llc,0/*chainbreak*/); // setting a new fold tree
				core::Size start_res=0;
				if (boost::iequals(tail_segment_, "c")){
					start_res=find_nearest_disulfide(pose,tail_dofs.disulfide());
				}
				else if (boost::iequals(tail_segment_, "n")){
					start_res=find_nearest_disulfide(pose,tail_dofs.disulfide())-tail_dofs.size()-1;
					TR<<"Size of tail dofs: "<<tail_dofs.size()<<std::endl;
					TR<<"start res of tail dofs is:"<<start_res<<std::endl;
				}
				for (core::Size i = 1; i <= tail_dofs.size(); ++i) {
					core::Size const pose_resi(start_res + i);
					TR<<"Previous phi/psi/omega at resi: "<<pose_resi<<" "<<pose.phi( pose_resi )<<'/'<<pose.psi( pose_resi )<<'/'<<pose.omega( pose_resi )<<'\n';
					pose.set_phi(pose_resi, tail_dofs[i ].phi());
					pose.set_psi(pose_resi, tail_dofs[i ].psi());
					pose.set_omega(pose_resi, tail_dofs[i ].omega());
					//		pose.dump_pdb( "dump"+ utility::to_string( i ) + ".pdb" );
					TR<<"requested phi/psi/omega: "<<tail_dofs[ i].phi()<<'/'<<tail_dofs[i].psi()<<'/'<<tail_dofs[i].omega()<<std::endl;
				}

			}
			if (debug_)
				pose.dump_pdb(mover_name_+"_after_changedofs_test.pdb");
			TR << std::endl;
			std::string threaded_seq("");	/// will be all ALA except for Pro/Gly on source pose and matching identities on source pose
			/// Now decide on residue identities: Alanine throughout except when the template pose has Gly, Pro or a residue that is the same as that in the original pose
			utility::vector1<core::Size> pro_gly_res; //keeping track of where pro/gly residues are placed
			pro_gly_res.clear();
			//pose.dump_pdb("before_threading.pdb");
			for (core::Size i = 0; i < total_residue_new; ++i) {
				core::Size const pose_resi(from_res() + i);
				std::string const dofs_resn(dofs[i + 1].resn());
				runtime_assert(dofs_resn.length() == 1);
				if (pose_resi == locked_res()) {
					threaded_seq += locked_res_id();
					continue;
				}
				if (design()) { // all non pro/gly residues in template are allowed to design
					if (dofs_resn == "G" || dofs_resn == "P") {
						pro_gly_res.push_back(pose_resi);
						TR << "Pro/Gly will be allowed at: " << pose_resi << std::endl;
					}
						threaded_seq += dofs_resn;
					//pose.replace_residue(pose_resi,source_pose.residue(nearest_to_from+i),0);
					continue;
				}
				core::Size const host_chain(1);
				core::Size const nearest_in_copy(find_nearest_res(in_pose_copy, pose, pose_resi, host_chain));
				if ((nearest_in_copy > 0 && dofs_resn[0] == in_pose_copy.residue(nearest_in_copy).name1()) || dofs_resn == "G"
						|| dofs_resn == "P")
					threaded_seq += dofs_resn;
				else {
					if (poly_ala())
						threaded_seq += "A";
					else {
						char orig_residue(0);
						if (nearest_in_copy)
							orig_residue = in_pose_copy.residue(nearest_in_copy).name1();
						if (orig_residue == 0 || orig_residue == 'G' || orig_residue == 'P')
							threaded_seq += 'x'; // residues that were originally Gly/Pro can be designed now
						else
							threaded_seq += ' '; // only repack
					}
				}
			}
			//pose.dump_pdb( "after_sequence_thread.pdb" );
			using namespace protocols::toolbox::task_operations;
			using namespace core::pack::task;
			ThreadSequenceOperationOP tso = new ThreadSequenceOperation;
				tso->target_sequence(threaded_seq);
				tso->start_res(from_res());
				tso->allow_design_around(true); // 21Sep12: from now on the design shell is determined downstream //false );
				TR << "Threading sequence: " << threaded_seq << " starting from " << from_res() << std::endl;
			TaskFactoryOP tf;
			if (design_task_factory()() == NULL)
				tf = new TaskFactory;
			else
				tf = new TaskFactory(*design_task_factory());

			if (restrict_to_repacking_chain2()) {
				for (core::Size i = 2; i <= pose.conformation().num_chains(); ++i) {
					TR << "Restricting chain " << i << " to repacking only" << std::endl;
					tf->push_back(new protocols::toolbox::task_operations::RestrictChainToRepackingOperation(i));
				}
			}

			tf->push_back(new operation::InitializeFromCommandline);
			tf->push_back(new operation::NoRepackDisulfides);
			if (allow_threading()) {
				TR << "THREADING ALLOWED" << std::endl;
				tf->push_back(tso);
			}
			else
				TR << "THREADING NOT ALLOWED" << std::endl;
			DesignAroundOperationOP dao = new DesignAroundOperation;
			dao->design_shell((design_task_factory()() == NULL ? 0.0 : design_shell())); // threaded sequence operation needs to design, and will restrict design to the loop, unless design_task_factory is defined, in which case a larger shell can be defined
			dao->repack_shell(repack_shell());

			TR << "Dao design shell: " << dao->design_shell() << ", Dao repack shell: " << repack_shell()
					<< std::endl;
			TR << std::endl;
			for (core::Size i = from_res(); i <= from_res() + total_residue_new - 1; ++i) {
				if (!pose.residue(i).has_variant_type(DISULFIDE)) {
					dao->include_residue(i);
					//TR << i << "+";
				}
			} //for
			TR << std::endl;
			tf->push_back(dao);
			TR<< "allowing pro/gly only at positions (29Mar13, given sequence profiles, now allowing pro/gly/his at all designed positions. The following is kept for benchmarking): ";
			for (core::Size res_num = 1; res_num <= pose.total_residue(); res_num++) {
				if (std::find(pro_gly_res.begin(), pro_gly_res.end(), res_num) == pro_gly_res.end()) {
					operation::RestrictAbsentCanonicalAASOP racaas = new operation::RestrictAbsentCanonicalAAS;
					if (allow_all_aa())
						racaas->keep_aas("ACDEFGHIKLMNPQRSTVWY"); //allow all amino acids - for the humanization project - Assaf Alon
					else
						racaas->keep_aas("ADEFGHIKLMNPQRSTVWY"); /// disallow pro/gly/cys/his /// 29Mar13 now allowing all residues other than Cys. Expecting sequence profiles to take care of gly/pro/his
					racaas->include_residue(res_num);
					tf->push_back(racaas);
				}
				else
					TR << res_num << ", ";
			}
			TR << std::endl;
			//	if( locked_res() ){
			//		operation::PreventRepackingOP pr = new operation::PreventRepacking;
			//		pr->include_residue( locked_res() );
			//		tf->push_back( pr );
			//		TR<<"preventing locked residue "<<locked_res()<<" from repacking"<<std::endl;
			//	}
			if (!(boost::iequals(tail_segment_, "c"))) { //if splicing out a c-ter tail then we are not doing ccd but rather tail segment mover, therefore we don't want to add a chainbreak. GDL
			protocols::protein_interface_design::movers::AddChainBreak acb;
			acb.resnum(utility::to_string(cut_site + residue_diff));
			acb.find_automatically(false);
			acb.change_foldtree(false);

//pose.dump_pdb("SJF_debugging.pdb");
				acb.apply(pose);
				TR << "Adding ccd chainbreak at: " << cut_site + residue_diff << std::endl;
			}
			//SJF debug	pose.conformation().detect_disulfides();
			//	( *scorefxn() ) ( pose );
			//	pose.update_residue_neighbors();
			if (use_sequence_profiles_){
				TR << "NOW ADDING SEQUENCE CONSTRAINTS" << std::endl;
				add_sequence_constraints(pose);
			}
			if (ccd()) {
				TR<<"Before doing CCD threading sequence from source pose onto pose"<<std::endl;
			//pose.dump_pdb("before_threading.pdb");
				TaskFactoryOP tf_thread = new TaskFactory();
				DesignAroundOperationOP dao_for_threading =  new DesignAroundOperation;
				for (core::Size i = from_res(); i <= from_res() + total_residue_new - 1; ++i) {
					if (!pose.residue(i).has_variant_type(DISULFIDE)) {
						dao_for_threading->include_residue(i);
						//TR << i << "+";
					}
				} //for
				dao_for_threading->design_shell();
				dao_for_threading->repack_shell(0);
				tf_thread->push_back(dao_for_threading);
				tf_thread->push_back(tso);
				PackerTaskOP ptask = tf_thread()->create_task_and_apply_taskoperations(pose);
				protocols::simple_moves::PackRotamersMover prm(scorefxn(), ptask);
				prm.apply(pose);
				if (debug_)
					pose.dump_pdb(mover_name_+"_after_threading.pdb");

				using namespace protocols::loops;
				using namespace protocols::rosetta_scripts;
				Pdb4LetName_ = parse_pdb_code(source_pdb_);	//
				core::Size const startn(from_res());
				core::Size const startc(from_res() + total_residue_new - 1);

				///		Loop loop( std::max( (core::Size) 2, from_res() - 6 )/*start*/, std::min( pose.total_residue()-1, to_res() + 6 )/*stop*/, cut_site/*cut*/ );
				TR<<"Loop definition before ccd:startn, startc, cut_site"<<startn<<","<< startc<<","<< cut_site<<std::endl;
				Loop loop(startn, startc, cut_site); /// Gideon & Sarel (8Jul13): we're now respecting the user's choice of from_res to_res and not melting the framework
				LoopsOP loops = new Loops();
				loops->push_back(loop);

				/// Gideon & Sarel (8Jul13): the comment below is no longer true, see comments above.
				/// Set ccd to minimize 4 residues at each loop terminus including the first residue of the loop. This way,
				/// the torsion in the loop are maintained. Allow repacking around the loop.
				/// If disulfide occurs in the range that is allowed to minimize, adjust that region to not include disulf
				core::scoring::ScoreFunctionOP scorefxn_local(scorefxn()->clone());	/// in case you want to modify the scorefxn. Currently not used
				protocols::loops::loop_mover::refine::LoopMover_Refine_CCD ccd_mover(loops, scorefxn_local);
				TailSegmentMover tsm;	//Object used for designing the tail segments of a protein
				ccd_mover.temp_initial(1.5);
				ccd_mover.temp_final(0.5);
				core::kinematics::MoveMapOP mm;
				mm = new core::kinematics::MoveMap;
				mm->set_chi(false);
				mm->set_bb(false);
				mm->set_jump(false);
				core::conformation::Conformation const & conf(pose.conformation());
				core::kinematics::FoldTree ft;
				ft=pose.fold_tree();
				if (conf.num_chains()>1){//if ligand is present we need to add edge between receptor and ligand

					core::Size const jump_num= ft.num_jump();
						TR<<"Number of jumps in the fold tree is:"<<jump_num<<std::endl;
						mm->set_jump(jump_num, true); /// 1Feb13 for cases in which the we're splicing in the presence of a ligand

				}

				/// First look for disulfides. Those should never be moved.
				///8Jul13: Gideon & Sarel: we previously melted the framework down from the from_res to_res points. Now, we respect the
				/// user's choice of from_res to_res, but the user has to make sure that the span doesn't contain a disulfide.
				/// a disulfide within the span will result in an assertion
				/*		core::Size disulfn( 0 ), disulfc( 0 );
				 for( core::Size i = from_res() - 3; i <= from_res(); ++i ){
				 if( pose.residue( i ).has_variant_type( DISULFIDE ) ){
				 disulfn = i;
				 }
				 }
				 for( core::Size i = from_res() + total_residue_new - 1; i <= from_res() + total_residue_new + 2; ++i ){
				 if( pose.residue( i ).has_variant_type( DISULFIDE ) ){
				 disulfc = i;
				 break;
				 }
				 }*/
				//TR<<"startn="<<startn<<std::endl;
				//TR<<"Residues allowed to minimize"<<std::endl;
				for (core::Size i = startn; i <= startc; ++i) {
					//TR<<i<<"+";
					mm->set_chi(i, true);
					mm->set_bb(i, true);

				}
				//TR<<""<<std::endl;;
				//	TR<<"Residues allowed to minimize"<<std::endl;

				mm->show();
				tf()->create_task_and_apply_taskoperations(pose);
				ccd_mover.set_task_factory(tf);
				ccd_mover.move_map(mm);

			//	pose.dump_pdb("before_ccd.pdb");
				//pose.dump_pdb("before_coordinate_constraints");
				TR << "Stretch I am applying the coordinate constraints on: " << from_res() << "-" << to_res() + residue_diff<< std::endl;

				core::Size anchor_res( 1 );
				if (protein_family_=="antibodies")/*if using "segment" and "protein_family" tags then we are using Rosetta db PSSMs*/
				  anchor_res = find_nearest_disulfide(pose, from_res());
				add_coordinate_constraints(pose, source_pose, from_res(), to_res() + residue_diff, anchor_res);	//add coordiante constraints to loop
				//TR_constraints<<"score function after CA constraints"<<std::endl;
				//scorefxn()->show(pose);
				add_coordinate_constraints(pose, source_pose, from_res(), to_res() + residue_diff, anchor_res,"CB");	//add coordiante constraints to loop
				//TR_constraints<<"score function after CB constraints"<<std::endl;
					//			scorefxn()->show(pose);
				if (CG_const_){
				PackerTaskOP ptask_for_coor_const(tf()->create_task_and_apply_taskoperations(pose));
				add_coordinate_constraints(pose, source_pose, from_res(), to_res() + residue_diff, anchor_res,"CG",ptask_for_coor_const);	//add coordiante constraints to loop
				TR_constraints<<"score function after CG constraints"<<std::endl;
												scorefxn()->show(pose);
				}
				if (!(boost::iequals(tail_segment_, "c")))//if we splcing out tail segment then we don't want to do dihedral constraint here. We will do it in the tail segemtn
					add_dihedral_constraints(pose, source_pose, from_res(), to_res() + residue_diff);	//add dihedral constraints loop
				//as control I want to see which residues are allowed to be design according to design shell.Gideonla may13
				//uncommnet this for debugging

				utility::vector1<core::Size> designable_residues = residue_packer_states(pose, tf, true, false);
				TR << "Residues Allowed to Design: "<< std::endl;
				for (utility::vector1<core::Size>::const_iterator i(designable_residues.begin());
						i != designable_residues.end(); ++i) {
					TR << *i << "+";
				}
				TR << std::endl;

				/*utility::vector1< core::Size > packable_residues = residue_packer_states (pose,tf, false, true);
				 TR<<"Residues Allowed to Repack:"<<std::endl;
				 for (utility::vector1<core::Size>::const_iterator i (packable_residues.begin()); i != packable_residues.end(); ++i) {
				 TR<<pose.residue(*i).name1()<<*i<<",";
				 }
				 TR<<std::endl;*/
				TR << "Weighted score function before ccd:" << std::endl;
				scorefxn()->show(pose);	//before ccd starts make sure we have all constratins in place, Gideon Lapidoth Jul13

				TR<<"fold tree before ccd"<<pose.fold_tree()<<std::endl;
				if (!(boost::iequals(tail_segment_, "c")))//if adding c-ter tail then we don't need ccd
					ccd_mover.apply(pose);//if tail segment is on we don't need ccd

				TR << "Weighted score function after ccd:" << std::endl;
				scorefxn()->show(pose);
				if (debug_)
					pose.dump_pdb(mover_name_+"_after_ccd.pdb");
				/*b/c of weird chain_break residues I add a second "copy dof from source" to correct this. Only for chain_break_res
				 gideon,160614*/
		/*		TR << "Changing chain break dofs"<<std::endl;
				core::kinematics::MoveMapOP mm4break = new core::kinematics::MoveMap;
				mm4break->set_chi(false);
				mm4break->set_bb(false);
				mm4break->set_jump(false);
				for (core::Size i = 0; i < total_residue_new; ++i) {
					core::Size const pose_resi(from_res() + i);
					if ((pose_resi<=(cut_site+residue_diff)+2)&&(pose_resi>=(cut_site+residue_diff)-1)){
						TR<<"Previous phi/psi/omega at resi: "<<pose_resi<<" "<<pose.phi( pose_resi )<<'/'<<pose.psi( pose_resi )<<'/'<<pose.omega( pose_resi )<<std::endl;
						pose.set_phi(pose_resi, dofs[i + 1].phi());
						pose.set_psi(pose_resi, dofs[i + 1].psi());
						pose.set_omega(pose_resi, dofs[i + 1].omega());
						mm4break->set_chi(pose_resi,true);
						mm4break->set_bb(pose_resi,true);
					//	pose.dump_pdb( "check_dih_cut_Site"+ utility::to_string( pose_resi ) + ".pdb" );
						TR<<"requested phi/psi/omega: "<<dofs[ i + 1 ].phi()<<'/'<<dofs[i+1].psi()<<'/'<<dofs[i+1].omega()<<std::endl;
						TR<<"resi after copy, phi/psi/omega: "<< pose_resi<<' '<<pose.phi( pose_resi )<<'/'<<pose.psi( pose_resi )<<'/'<<pose.omega( pose_resi )<<std::endl;
					}
				}*/
			//	pose.dump_pdb("chain_break_copy_dofs.pdb");
		//		TR<<"move map before chain break minimization:"<<std::endl;
			//	mm4break->show();
				//Perfrom minimization on new DIH angles
	//			protocols::simple_moves::MinMoverOP min_mover= new protocols::simple_moves::MinMover(mm4break,scorefxn(),"dfpmin_armijo_nonmonotone",0.01,true /*use_nblist*/ );
	//								min_mover->apply(pose);


				//After segment insertion do we want to add the tail segment
				/* The antibody is comprised of two chains, Vl and Vh, there fore we have 4 tail segments to consider:
				 * each case is handled differently

				 *Antibody representation:
				 * n-ter tail       Vl                   cut                 Vh
				 'n-ter'--------C--------C---------'c-ter'//'n-ter'--------C--------C---------'c-ter'

				 */
				//core::Real tail_rms=0;// will store the rms diff between the tail segment and the source pdb
				core::Size tail_size = 0; //how many residues are in the tail segment;
				std::string tail= ""; //will mark if the tail is n-terminal or c-terminal
				core::Size tail_end = 0; //save the position of the last tail residue
				core::Size tail_start = 0;
				core::Size disulfide_res = 0; //will store disulfide residue number
				if (tail_segment_ != "") {
					tail='*';//used to mark in the tail db that we have tail segment;
					TR << "Adding tail segment, removing previous constraints:" << std::endl;
					//remove previous constraints so they don't skew scorefunction
					pose.remove_constraints();
					//Don't need chainbreak score anymore so remove it
					add_sequence_constraints(pose);

					//To make sure constraint were removed lets look at the score function:
					scorefxn()->show(pose);
					dofs.clear(); //initializing the dofs vector. will now store the dofs of the tail segment

					//find relevant disulfide
					disulfide_res = find_nearest_disulfide(pose, from_res()); //returns the closest disulfide residue to the input res.
					if (boost::iequals(tail_segment_, "n")) {
						disulfide_res < vl_vh_cut ?
								tail_start = 1: tail_start = vl_vh_cut + 1;
						tail_size = disulfide_res - tail_start;
						tail_end = disulfide_res - 1;
					}
					else if (boost::iequals(tail_segment_, "c")) {
						TR << "to res on pose after llc: " << to_res() + residue_diff << std::endl;
						if (disulfide_res < cut_vl_vh_after_llc)
							tail_end = cut_vl_vh_after_llc;
						else
							tail_end = pose.split_by_chain()[1]->total_residue();//in case we have a ligand I assume the designed protein will be chain 1.

						tail_size = tail_end - disulfide_res;
						tail_start = disulfide_res + 1;
					}
					//pose.dump_pdb("before_tail.pdb");
					//TR << "Setting new fold tree and move map for tail segment"<< std::endl;
					core::Size nearest_disulfide_on_source = find_nearest_res(source_pose, pose, disulfide_res, 1);
					TR<<"disulfide res on pose is:"<<disulfide_res<<std::endl;
					TR << "Nearest disulfide on source is: " << nearest_disulfide_on_source << std::endl;
					int res_diff = nearest_disulfide_on_source - disulfide_res;
					TR << "Tail_start:" << tail_start << ", Tail_end: " << tail_end << " disulfide res: " << disulfide_res<< "residue diff: " << res_diff << std::endl;
					//Because we are copying dihedral angles from the source pdb we have to make sure the tail start/end of the source pdb
					//is at leaset the same lenght as the pose. This conditional should only hold for n-ter tails because the the seqeunce length from start to Cys is constant. For C-ter tail we are copying the stretch from the source PDB so there is no problem.
					TR<<"tail_start + res_diff="<<tail_start + res_diff<<std::endl;
					if (((boost::iequals(tail_segment_, "n")) && (tail_start + res_diff < 1))) {
						utility_exit_with_message("Source pdb tail must be at least the same length as the template PDB\n");
					}

					for (core::Size i = tail_start; i <= tail_end; ++i) {
						/// Feed the source_pose dofs into the BBDofs array
						BBDofs residue_dofs;
						//TR<<"Copying the following dihedral angles from source pdb:"<< std::endl;
						TR << source_pose.residue(i + res_diff).name() << i + res_diff << source_pose.phi(i + res_diff) << ","
								<< source_pose.psi(i + res_diff) << "," << source_pose.omega(i + res_diff) << "," << std::endl;
						residue_dofs.phi(source_pose.phi(i + res_diff));
						residue_dofs.psi(source_pose.psi(i + res_diff));
						residue_dofs.omega(source_pose.omega(i + res_diff));

						/// convert 3let residue code to 1let code
						std::stringstream ss;
						std::string s;
						ss << source_pose.residue(i + res_diff).name1();
						ss >> s;
						residue_dofs.resn(s);

						dofs.push_back(residue_dofs);
					}			// for i nearest_to_from..nearest_to_to

					//copy dofs from source pdb to pose
					//before copying dofs apply coordinate constrains
					TR << "Applying dihedral and coordinate constraints" << std::endl;
					add_coordinate_constraints(pose, source_pose, tail_start, tail_end, disulfide_res);	//add coordinate constraints to loop
					add_coordinate_constraints(pose, source_pose, tail_start, tail_end, disulfide_res,"CB");	//add coordiante constraints to loop
					//TR_constraints<<"score function after CB constraints"<<std::endl;
					//			scorefxn()->show(pose);
					if (CG_const_){
						PackerTaskOP ptask_for_coor_const(tf()->create_task_and_apply_taskoperations(pose));
						add_coordinate_constraints(pose, source_pose, tail_start, tail_end, disulfide_res,"CG",ptask_for_coor_const);	//add coordiante constraints to loop
						//TR_constraints<<"score function after CG constraints"<<std::endl;
						//scorefxn()->show(pose);
					}
					add_dihedral_constraints(pose, source_pose,boost::iequals(tail_segment_, "n")?tail_start+1: tail_start, boost::iequals(tail_segment_, "c")?tail_end-1: tail_end);
					tail_size = dofs.size();
					tail_fold_tree(pose, cut_vl_vh_after_llc,0/*chain_break*/); // setting a new fold tree
					TR << "Changing dofs, the size of the tail segment is: " << tail_size << std::endl;
					//pose.dump_pdb("before_changedofs_tail_test.pdb");
					for (core::Size i = 0; i < tail_size; ++i) {
						pose.set_phi(tail_start + i, dofs[i + 1].phi());
						pose.set_psi(tail_start + i, dofs[i + 1].psi());
						pose.set_omega(tail_start + i, dofs[i + 1].omega());
						//pose.dump_pdb( "dump"+ utility::to_string( i ) + ".pdb" );
						TR << "pose_res, phi/psi/omega: " << tail_start + i << ' ' << pose.phi(tail_start + i) << '/'
								<< pose.psi(tail_start + i) << '/' << pose.omega(tail_start + i) << std::endl;
						TR << "requested phi/psi/omega: " << dofs[i + 1].phi() << '/' << dofs[i + 1].psi() << '/'
								<< dofs[i + 1].omega() << std::endl;
					}	//for
					//add dihedral constraints loop
					//pose.dump_pdb("after_changedofs_tail_test.pdb");
					mm = new core::kinematics::MoveMap;
					mm->set_chi(false);
					mm->set_bb(false);
					mm->set_jump(false);

					for (core::Size i = tail_start; i <= tail_end; ++i) {			//make sure disulfide cannot move
						mm->set_chi(i, true); //allowing chi angle movement
						mm->set_bb(i, true); //allowing bb movement
					}
					mm->show();
					//Add new dihedral and coordinate constraint to the tail segment
					tsm.set_fa_scorefxn(scorefxn());
					tsm.set_movemap(mm);
					tf = new TaskFactory;
					tf = new TaskFactory(*design_task_factory());
					if (restrict_to_repacking_chain2()) {
						for (core::Size i = 2; i <= pose.conformation().num_chains(); ++i) {
							TR << "Restricting chain " << i << " to repacking only" << std::endl;
							tf->push_back(new protocols::toolbox::task_operations::RestrictChainToRepackingOperation(i));
						}
					}
					tf->push_back(new operation::InitializeFromCommandline);
					tf->push_back(new operation::NoRepackDisulfides);
					if (allow_threading()) {
						TR << "THREADING ALLOWED" << std::endl;
						tf->push_back(tso);
					}
					else
						TR << "THREADING NOT ALLOWED" << std::endl;

					DesignAroundOperationOP dao = new DesignAroundOperation;
					dao->design_shell((design_task_factory()() == NULL ? 0.0 : design_shell())); // threaded sequence operation needs to design, and will restrict design to the loop, unless design_task_factory is defined, in which case a larger shell can be defined
					dao->repack_shell(repack_shell());
					for (core::Size i = tail_start; i <= tail_end; ++i) {
						if (!pose.residue(i).has_variant_type(DISULFIDE)) {
							dao->include_residue(i);
						}
					} //for
					TR << std::endl;
					tf->push_back(dao);
					for (core::Size res_num = 1; res_num <= pose.total_residue(); res_num++) {
						if (std::find(pro_gly_res.begin(), pro_gly_res.end(), res_num) == pro_gly_res.end()) {
							operation::RestrictAbsentCanonicalAASOP racaas = new operation::RestrictAbsentCanonicalAAS;
							if (allow_all_aa())
								racaas->keep_aas("ACDEFGHIKLMNPQRSTVWY"); //allow all amino acids - for the humanization project - Assaf Alon
							else
								racaas->keep_aas("ADEFGHIKLMNPQRSTVWY"); /// disallow pro/gly/cys/his /// 29Mar13 now allowing all residues other than Cys. Expecting sequence profiles to take care of gly/pro/his
							racaas->include_residue(res_num);
							tf->push_back(racaas);
						}
						else
							TR << res_num << ", ";
					}
					tsm.set_task_factory(tf);
					tsm.apply(pose);

				} //fi "tail_segment"
				//pose.dump_pdb("after_tail_segemtn_mover.pdb");
				/// following ccd, compute rmsd to source loop to ensure that you haven't moved too much. This is a pretty decent filter
				if (torsion_database_fname_ == "") { // no use computing rms if coming from a database (no coordinates)
					core::Real rms(0);
					for (core::Size i = 0; i <= total_residue_new - 1; ++i) {
						core::Real const dist(
								pose.residue(from_res() + i).xyz("CA").distance(source_pose.residue(nearest_to_from + i).xyz("CA")));
						rms += dist;
					}
					core::Real const average_rms(rms / total_residue_new);
					TR << "Average distance of spliced segment to original: " << average_rms << std::endl;
					TR<< "rms calculated for span:"<< from_res()<<"-"<<from_res() +total_residue_new<<std::endl;
					if (average_rms >= rms_cutoff()) {
						TR << "Failing because rmsd = " << average_rms << std::endl;
						set_last_move_status(protocols::moves::FAIL_RETRY);
						retrieve_values();
					//	pose.dump_pdb("failed_rmsd_pose.pdb");
						return;
					}

					//print rms value to output pdb structure
					std::string Result;
					std::string Result_filter;
					std::ostringstream convert;
					std::ostringstream convert_filter;
					convert << average_rms;
					Result = convert.str();

					core::pose::add_comment(pose, "RMSD to source loop", Result);

					//print ChainBreak value to output pdb structure
					//add chain breack again

					if (!(boost::iequals(tail_segment_, "c"))){//When splicing in C-ter tails we don't insert chain-break therefore no need to check chain break filter, gideon
						if( (protein_family_=="antibodies" ) )
							tail_fold_tree(pose,cut_vl_vh_after_llc,cut_site+residue_diff); // why is this necessary? SJF 22Jul14
						TR<<"fold tree before chain break val score: "<<pose.fold_tree()<<std::endl;
						convert_filter << splice_filter()->score(pose);
						Result_filter = convert_filter.str();
						core::pose::add_comment(pose, "Chainbreak Val:", Result_filter);
						if (!splice_filter()->apply(pose)) {
					//		pose.dump_pdb("failed_filter_pose.pdb");
							TR << "Failing because filter fails" << std::endl;
							set_last_move_status(protocols::moves::FAIL_RETRY);
							retrieve_values();
							return;
						}
					}
				}
				/// tell us what the torsions of the new (closed) loop are. This is used for dbase construction. At one point, might be a good idea to make the mover
				/// output the dofs directly to a dbase file rather than to a log file.
				TaskFactoryOP tf_dofs = new TaskFactory;
				DesignAroundOperationOP dao_dofs = new DesignAroundOperation;
				for (core::Size i = startn; i <= std::min(startc + res_move() - 1, startc); ++i)
					dao_dofs->include_residue(i);
				dao_dofs->design_shell(0);		/// only include the loop residues
				tf_dofs->push_back(dao_dofs);
				protocols::protein_interface_design::filters::Torsion torsion;
				torsion.task_factory(tf_dofs);
				torsion.task_factory_set(true);
				torsion.apply(pose);
				core::Size const stop_on_template(to_res());
				TR_ccd << "start, stop, cut: " << startn << " " << stop_on_template << " " << cut_site << std::endl; /// used for the dbase

				/// Now write to dbase disk file
				if (loop_dbase_file_name_ != "") {
					std::ofstream dbase_file;
					dbase_file.open(loop_dbase_file_name_.c_str(), std::ios::app);
					for (core::Size i = startn; i <= std::min(startc + res_move() - 1, startc); ++i)
						dbase_file << pose.phi(i) << ' ' << pose.psi(i) << ' ' << pose.omega(i) << ' ' << pose.residue(i).name3()
								<< ' ';
					dbase_file << startn << ' ' << stop_on_template << ' ' << cut_site << ' ';
					if (loop_pdb_source_ != "")
						dbase_file << Pdb4LetName_;
					else
						dbase_file << "cut" ; // the word cut is used as a placeholder. It is advised to use instead the source pdb file in this field so as to keep track of the origin of dbase loops
					if (tail_segment_ != "") { //add tail dihedral angles to db file
						dbase_file << tail<<' ';
						for (core::Size i = tail_start; i <= tail_end; ++i) {
							dbase_file << pose.phi(i) << ' ' << pose.psi(i) << ' ' << pose.omega(i) << ' ' << pose.residue(i).name3()<< ' ';
						}
						dbase_file << find_nearest_disulfide(*template_pose_,disulfide_res) << ' ' << tail_segment_ << ' ' << Pdb4LetName_<<' '<< cut_site<< std::endl;
					}
					else{//add newline charecter to db file
						dbase_file <<std::endl;
					}
					dbase_file.close();
				}
			} // fi ccd
			else { // if no ccd, still need to thread sequence
				//If a filter (such as the bb clash dectection filter is defined) it should be tested here.
				//TR<<"Skipped ccd"<<std::endl;
				if (splice_filter() && !splice_filter()->apply(pose)) {
					//std::ostringstream convert_filter;
					//std::string Result_filter;
					//convert_filter << splice_filter()->score( pose );
					//Result_filter = convert_filter.str();

					TR << "Failing before design because filter reports failure (splice filter)!" << std::endl;
					//TR<<"Filter value is: "<<Result_filter<<std::endl;
					set_last_move_status(protocols::moves::FAIL_RETRY);
					retrieve_values();
					return;
				}

				//Debugging, remove after, gideonla aug13
				//TR<<"NOT DOING CCD, DOING REPACKING INSTEAD"<<std::endl;
				TaskFactoryOP tf_in = new TaskFactory(*design_task_factory());
				tf_in->push_back(new operation::InitializeFromCommandline);
				tf_in->push_back(new operation::NoRepackDisulfides);
				tf_in->push_back(dao);

				PackerTaskOP ptask = tf_in()->create_task_and_apply_taskoperations(pose);
				protocols::simple_moves::PackRotamersMover prm(scorefxn(), ptask);
				//		pose.conformation().detect_disulfides();
				//		pose.update_residue_neighbors();
				//		(*scorefxn())(pose);
				if (debug_){
					utility::vector1<core::Size> designable_residues = residue_packer_states(pose, tf_in, true/*designable*/, false/*packable*/);
					TR << "Residues Allowed to Design: "<< std::endl;
					for (utility::vector1<core::Size>::const_iterator i(designable_residues.begin());i != designable_residues.end(); ++i) {
						TR<<"Allowed aa's for residue "<<*i<<" are: ";
						std::list< core::chemical::ResidueTypeCOP > allowed_aas =ptask->residue_task(*i).allowed_residue_types();
						for (std::list< core::chemical::ResidueTypeCOP >::const_iterator restype = allowed_aas.begin();restype != allowed_aas.end(); ++restype) {
							TR<<(*restype )->name1()<<",";
						}
						TR<<std::endl;;
					}

					pose.dump_pdb(mover_name_+"_before_ppk.pdb");
				}
				prm.apply(pose);
				if (debug_)
					pose.dump_pdb(mover_name_+"_before_rtmin.pdb");
				//After Re-packing we add RotamerTrialMover to resolve any left over clashes, gideonla Aug13
				if (rtmin()) {		//To prevent rtmin when not needed - Assaf Alon
					TaskFactoryOP tf_rtmin = new TaskFactory(*tf);//this taskfactory (tf_rttmin) is only used here. I don't want to affect other places in splice, gideonla aug13
					tf_rtmin->push_back(new operation::RestrictToRepacking()); //W don't rtmin to do design
					ptask = tf_rtmin()->create_task_and_apply_taskoperations(pose);
					protocols::simple_moves::RotamerTrialsMinMover rtmin(scorefxn(), *ptask);
					rtmin.apply(pose);
					if (debug_)
						pose.dump_pdb(mover_name_+"after_rtmin.pdb");
				}
				if (min_seg_){
					core::kinematics::MoveMapOP mm;
					mm = new core::kinematics::MoveMap;
					mm->set_chi(false);
					mm->set_bb(false);
					mm->set_jump(false);
					if (debug_==1)
						pose.dump_pdb(mover_name_+source_pdb()+"_Before_min.pdb");
					//TR<<"from_res()@minimization: "<<from_res()<<std::endl;
					//set movemap for segment
					for (core::Size i = 0; i < dofs.size(); ++i) {
						core::Size const pose_resi(from_res() + i);
						mm->set_chi(pose_resi, true);
						mm->set_bb(pose_resi, true);
					}
					TR_min<<"from_res: "<<from_res()<<",to_res: "<<to_res()<<",dofs size: "<<dofs.size()<<std::endl;
					add_coordinate_constraints(pose, pose, from_res(),from_res()+dofs.size()-1, find_nearest_disulfide(pose,from_res()));
					core::Size cut_site=0;
					if (boost::iequals(tail_segment_, "n")){//This only matters for n-ter tail because C-ter tail residue are included in the previous mm
						//add edge to fold tree
						add_coordinate_constraints(pose, pose, find_nearest_disulfide(pose,from_res())-tail_dofs.size(), find_nearest_disulfide(pose,from_res())-1, find_nearest_disulfide(pose,from_res()));//add coordinate constraint to tail
						for (core::Size i =find_nearest_disulfide(pose,from_res())-tail_dofs.size(); i<find_nearest_disulfide(pose,from_res()) ; ++i) {
							mm->set_chi(i, true);
							mm->set_bb(i, true);
						}
						mm->show();

						TR<<"pose disulfides: "<<find_nearest_disulfide(pose,to_res())<<","<<find_nearest_disulfide(pose,from_res())<<",template disulfides: "<<find_nearest_disulfide(*template_pose_,dofs.stop_loop())<<","<<find_nearest_disulfide(*template_pose_,dofs.start_loop())<<std::endl;
						int pose_seg_length=(find_nearest_disulfide(pose,to_res())-find_nearest_disulfide(pose,from_res()));
						TR<<"pose seg len:"<<pose_seg_length<<std::endl;
						int template_seg_length=find_nearest_disulfide(*template_pose_,dofs.stop_loop())-find_nearest_disulfide(*template_pose_,dofs.start_loop());
						TR<<"template seg len:"<<template_seg_length<<std::endl;
						int residue_diff=pose_seg_length-template_seg_length;
						TR<<"Residue_diff is: " <<residue_diff<<std::endl;
						cut_site=dofs.cut_site()+residue_diff;

						TR << "Adding chainbreak at: " << cut_site << std::endl;
						protocols::protein_interface_design::movers::AddChainBreak acb;
						acb.resnum(utility::to_string(cut_site));
						acb.find_automatically(false);
						acb.change_foldtree(0);
						acb.apply(pose);
						mm->show();
					}

					tail_fold_tree(pose, cut_vl_vh_after_llc, cut_site);
					TR<<"Fold tree before minimization: "<<pose.fold_tree()<<std::endl;
					protocols::simple_moves::MinMoverOP min_mover= new protocols::simple_moves::MinMover(
								mm,
								scorefxn(),
								"dfpmin_armijo_nonmonotone",
								0.01,
								true /*use_nblist*/ );
					min_mover->apply(pose);
					//remove coordinate constraints post minimization
					pose.remove_constraints();
					add_sequence_constraints(pose);
				}
			}
			saved_fold_tree_ = new core::kinematics::FoldTree(pose.fold_tree());
			retrieve_values();
		}

/// splice apply might change the from_res/to_res internals since they sometimes refer to the template file. If that happens, we want the values to
/// revert to their original values before the end of the apply function (so retrieve_values) below must be called before return.
		void Splice::save_values() {
			saved_from_res_ = from_res();
			saved_to_res_ = to_res();
		}

		void Splice::retrieve_values() {
			from_res(saved_from_res_);
			to_res(saved_to_res_);
			first_pass_ = false;
			save_to_checkpoint();
		}

		std::string Splice::get_name() const {
			return SpliceCreator::mover_name();
		}

		void Splice::parse_my_tag(TagCOP const tag, basic::datacache::DataMap &data, protocols::filters::Filters_map const & filters, protocols::moves::Movers_map const &, core::pose::Pose const & pose) {
			utility::vector1<TagCOP> const sub_tags(tag->getTags());
			mover_name_=tag->getOption<std::string>("name");//for debugging purposes
			debug_=tag->getOption<bool>("debug", false);
			min_seg_=tag->getOption<bool>("min_seg", false);
			CG_const_=tag->getOption<bool>("CG_const", false);
			rb_sensitive(tag->getOption<bool>("rb_sensitive", false));
			chain_num_ = tag->getOption<core::Size>("chain_num", 1);
			tail_segment_ = tag->getOption<std::string>("tail_segment", "");
			if ((tag->hasOption("tail_segment"))
					&& !(((boost::iequals/*BOOST function for comparing strings */(tail_segment_, "c")))
							|| (boost::iequals(tail_segment_, "n")))) {
				utility_exit_with_message("\"tail_segment\" tag accepts either \"c\" or \"n\"\n");
			}

			bool check_segment = false;
			if ((tag->hasOption("segment")) && (tag->hasOption("protein_family"))) {
				TR << " !! ATTENTION !! YOU ARE USING DATABASE PSSMs" << std::endl;

				segment_type_ = tag->getOption<std::string>("segment");
				protein_family(tag->getOption<std::string>("protein_family")); ///Declare which protein family to use in-order to invoke the correct PSSM files
			BOOST_FOREACH( std::string const segment_type, order_segments_[protein_family_] ) {

				SpliceSegmentOP splice_segment( new SpliceSegment );
				splice_segment->all_pdb_profile( protein_family_to_database_.find(protein_family_)->second, segment_type );
				splice_segment->read_many (protein_family_to_database_.find(protein_family_)->second,segment_type);
				splice_segments_.insert( std::pair< std::string, SpliceSegmentOP >( segment_type, splice_segment ) );
				use_sequence_profiles_ = true;
				segment_names_ordered_.push_back(segment_type);
				profile_weight_away_from_interface( tag->getOption< core::Real >( "profile_weight_away_from_interface", 1.0 ) );
				check_segment=true;
			}
			//Un-comment this for de-bugging purposes. gdl 161213
			/*	typedef std::map< std::string, SpliceSegmentOP >::iterator it_type;
			 for(it_type iterator = splice_segments_.begin(); iterator != splice_segments_.end(); iterator++) {
			 TR<<"##############"<<iterator->first<<std::endl;
			 TR<<" The cluster of 1AHW is: "<<iterator->second->get_cluster_name_by_PDB("1AHW")<<std::endl;;//second is the SpliceSegment_OP
			 }//End of debuggin function*/

			if (protein_family_=="antibodies"){/*if using "segment" and "protein_family" tags then we are using Rosetta db PSSMs*/
				pdb_to_H3_seq_map_=read_H3_seq(protein_family_to_database_.find(protein_family_)->second);
			}
		}
//complex if staement to ensure that user provied both segment flag and protein family flag
		if ((!(tag->hasOption("segment")) && (tag->hasOption("protein_family")))
				|| ((tag->hasOption("segment")) && (!tag->hasOption("protein_family")))) {
			utility_exit_with_message(
					"You are using Splice flags \"segment=\" or \"protein_family\" without the other, they both must be used!\n");
		}
		skip_alignment( tag->getOption< bool >( "skip_alignment", false ) );
		typedef utility::vector1<std::string> StringVec;
//that the sequence profile is built according to the user (eg. vl, L3, vh, H3)
		if (!sub_tags.empty()) { //subtags are used to assign CDR PSSMs and to
			BOOST_FOREACH( TagCOP const sub_tag, sub_tags ) {

				if( sub_tag->getName() == "Segments" ) {
					//if this flag is present then we don't use the subtags, get all pssm data from the database
					if (tag->hasOption("segment")) { //if both tags are turned on exit with error msg.
						utility_exit_with_message(
								"it appears you are trying to run both \"segment\" and sub tags \"segments\" simoutansiously, this is not a valid option. Please only choose one\n");
					}
					segment_names_ordered_.clear();	//This string vector holds all the segment names inserted by the user, the pssm will be constructed in this order so user must make sure the order is correct
					check_segment = false;//This is set to false unless current segment appears in the segment list
					use_sequence_profiles_ = true;
					profile_weight_away_from_interface( tag->getOption< core::Real >( "profile_weight_away_from_interface", 1.0 ) );
					segment_type_ = sub_tag->getOption< std::string >( "current_segment" );

					//TR<<"reading segments in splice "<<tag->getName()<<std::endl;

					utility::vector1< TagCOP > const segment_tags( sub_tag->getTags() );
					BOOST_FOREACH( TagCOP const segment_tag, segment_tags ) {
						SpliceSegmentOP splice_segment( new SpliceSegment );
						std::string const segment_name( segment_tag->getName() ); //get name of segment from xml
						std::string const pdb_profile_match( segment_tag->getOption< std::string >( "pdb_profile_match" ) );// get name of pdb profile match, this file contains all the matching between pdb name and sub segment name, i.e L1.1,L1.2 etc
						std::string const profiles_str( segment_tag->getOption< std::string >( "profiles" ) );
						StringVec const profile_name_pairs( utility::string_split( profiles_str, ',' ) );

						//	TR<<"Now working on segment:"<<segment_name<<std::endl;
						BOOST_FOREACH( std::string const s, profile_name_pairs ) {
							StringVec const profile_name_file_name( utility::string_split( s, ':' ) );
								//TR<<"pssm file:"<<profile_name_file_name[ 2 ]<<",segment name:"<<profile_name_file_name[ 1 ]<<std::endl;

							splice_segment->read_profile( profile_name_file_name[ 2 ], profile_name_file_name[ 1 ] );
						}

						splice_segment->read_pdb_profile( pdb_profile_match );
						//TR<<"the segment name is: "<<segment_name<<std::endl;
						if (segment_name.compare(segment_type_) == 0) {
							check_segment=true;
						}
						splice_segments_.insert( std::pair< std::string, SpliceSegmentOP >( segment_name, splice_segment ) );
						segment_names_ordered_.push_back(segment_name);
					} //foreach segment_tag
				} // fi Segments
				if( sub_tag->getName() == "DB" ) {
					check_segment=1; //so not to fail following sanity check
					utility::vector1< TagCOP > const segment_tags( sub_tag->getTags() );
					BOOST_FOREACH( TagCOP const segment_tag, segment_tags ) {
						std::string const segment_name( segment_tag->getName() );
						std::string const db_file( segment_tag->getOption< std::string >( "torsion_db_file" ) );
						database_segment_map_[segment_name]=db_file;
						if (segment_names_ordered_.size()<=segment_tags.size()) {
							segment_names_ordered_.push_back(segment_name); //this vector is used to save the order in which the segemnt are entered according to the user
						} //fi
					} //for
				} //fi
			} //foreach sub_tag
			if (tag->hasOption("use_seqeunce_profile")) {
				add_sequence_constraints_only(tag->getOption<bool>("use_seqeunce_profile"));
			}
			if (!check_segment && !segment_names_ordered_.empty()) { //sanity check to make sure the current segment (what is being spliced) is also in the list of segemtns
				utility_exit_with_message(
						"Segment " + segment_type_ + " was not found in the list of segemnts. Check XML file\n");
			}
			if (segment_names_ordered_.empty()) { //If splicing in segment but not using sequence profile then turn off "use_seqeunce_profiles"
				use_sequence_profiles_ = false;
			}

		} //fi (sub_tags!=NULL)


//Function for debuging should be commented out, check that all segments are in the right place
		/*
		 typedef std::map< std::string, SpliceSegmentOP >::iterator it_type;
		 for(it_type iterator = splice_segments_.begin(); iterator != splice_segments_.end(); iterator++) {
		 TR<<"##############"<<iterator->first<<std::endl;
		 TR<<" The cluster of 1AHW is: "<<iterator->second->get_cluster_name_by_PDB("1AHW")<<std::endl;;//second is the SpliceSegment_OP
		 */

//}//End of debuggin function
		scorefxn(protocols::rosetta_scripts::parse_score_function(tag, data));
		add_sequence_constraints_only(tag->getOption<bool>("add_sequence_constraints_only", false));
		if (add_sequence_constraints_only()) {
			TR << "add_sequence_constraints only set to true. Therefore not parsing any of the other Splice flags."
					<< std::endl;

			return;
		}
		template_file(tag->getOption<std::string>("template_file", ""));
		if (template_file_ != "") { /// using a template file to determine from_res() to_res()
			if (data.has("poses", template_file_)) {
				template_pose_ = data.get<core::pose::Pose *>("poses", template_file_);
				TR << "using template pdb from datamap" << std::endl;
			}
			else if (tag->hasOption("template_file")) {
				template_pose_ = new core::pose::Pose;
				core::import_pose::pose_from_pdb(*template_pose_, template_file_);
				data.add("poses", template_file_, template_pose_);
				TR << "loading template_pose from " << template_file_ << std::endl;
			}
		}
		else
			template_pose_ = new core::pose::Pose(pose);
		set_fold_tree_only_ = tag->getOption<bool>("set_fold_tree_only", false);
		if (set_fold_tree_only_)
			return; //other options are not relevant
		start_pose_ = new core::pose::Pose(pose);
		runtime_assert(tag->hasOption("torsion_database") != tag->hasOption("source_pdb"));
		task_factory(protocols::rosetta_scripts::parse_task_operations(tag, data));
		if (!tag->hasOption("task_operations")) {
			from_res(core::pose::parse_resnum(tag->getOption<std::string>("from_res", "0"), pose));
			to_res(core::pose::parse_resnum(tag->getOption<std::string>("to_res", "0"), pose));
		}
		if (tag->hasOption("design_task_operations")) {
			TR << "Defined design_task_factory, which will be used during splice design" << std::endl;
			design_task_factory(
					protocols::rosetta_scripts::parse_task_operations(tag->getOption<std::string>("design_task_operations"),
							data));
		}
		if (tag->hasOption("residue_numbers_setter")) {
			runtime_assert(!tag->hasOption("locked_res"));
			locked_res_ =
					basic::datacache::get_set_from_datamap<basic::datacache::DataMapObj<utility::vector1<core::Size> > >(
							"residue_numbers", tag->getOption<std::string>("residue_numbers_setter"), data);
		}
		if (tag->hasOption("torsion_database")) {
			torsion_database_fname(tag->getOption<std::string>("torsion_database"));
			database_entry(tag->getOption<core::Size>("database_entry", 0));
			database_pdb_entry(tag->getOption<std::string>("database_pdb_entry", ""));
			runtime_assert(!(tag->hasOption("database_entry") && tag->hasOption("database_pdb_entry")));
			read_torsion_database();
			TR << "torsion_database: " << torsion_database_fname() << " ";
			if (database_entry() == 0) {
				if (database_pdb_entry_ == "")
					TR << " database entry will be randomly picked at run time. ";
				else
					TR << " picking database entry " << database_pdb_entry() << std::endl;
			}
			else {
				TR << " database_entry: " << database_entry() << " ";
				runtime_assert(database_entry() <= torsion_database_.size());
			}
		}
		else
			source_pdb(tag->getOption<std::string>("source_pdb"));

		ccd(tag->getOption<bool>("ccd", 1));
//dihedral_const(tag->getOption< core::Real >( "dihedral_const", 0 ) );//Added by gideonla Apr13, set here any real posiive value to impose dihedral constraints on loop
//coor_const(tag->getOption< core::Real >( "coor_const", 0 ) );//Added by gideonla May13, set here any real to impose coordinate constraint on loop
		design_shell(tag->getOption<core::Real>("design_shell", 6.0)); //Added by gideonla May13,
		repack_shell(tag->getOption<core::Real>("repack_shell", 8.0)); //Added by gideonla May13,
		rms_cutoff(tag->getOption<core::Real>("rms_cutoff", 999999));
		runtime_assert(!(tag->hasOption("torsion_database") && tag->hasOption("rms_cutoff"))); // torsion database doesn't specify coordinates so no point in computing rms
		res_move(tag->getOption<core::Size>("res_move", 1000)); // All resdiues in the backbone can move
		randomize_cut(tag->getOption<bool>("randomize_cut", false));
		runtime_assert((tag->hasOption("randomize_cut") && tag->hasOption("source_pose")) || !tag->hasOption("source_pose"));
		cut_secondarystruc(tag->getOption<bool>("cut_secondarystruc", false));
//	runtime_assert( (tag->hasOption( "cut_secondarystruc ") && tag->hasOption( "randomize_cut" )) || !tag->hasOption( "cut_secondarystruc" ) );
		equal_length(tag->getOption<bool>("equal_length", false));
		poly_ala(tag->getOption<bool>("thread_ala", true));

		std::string delta;
		if (tag->hasOption("delta_lengths")) {
			delta = tag->getOption<std::string>("delta_lengths");
			StringVec const lengths_keys(utility::string_split(delta, ','));
		BOOST_FOREACH( std::string const delta, lengths_keys ) {
			if( delta == "" ) continue;
			int const delta_i( 1 * atoi( delta.c_str() ) );
			delta_lengths_.push_back( delta_i );
		}
	}
	else
		delta_lengths_.push_back(0);
	std::sort(delta_lengths_.begin(), delta_lengths_.end());
	std::unique(delta_lengths_.begin(), delta_lengths_.end());
	//TR<<"Deltas from xml: ";
	//BOOST_FOREACH( int const d, delta_lengths_ )
	//    TR<<d<<',';
	//TR<<std::endl;

	design(tag->getOption<bool>("design", false));
	allow_threading(tag->getOption<bool>("allow_threading", true)); // to stop Splice from threading the Gly and Pro residues from the source - Assaf Alon
	rtmin(tag->getOption<bool>("rtmin", true)); // to prevent splice from doing rtmin when not needed - Assaf Alon
	allow_all_aa(tag->getOption<bool>("allow_all_aa", false)); // to allow all amino acids in design - Assaf Alon
	dbase_iterate(tag->getOption<bool>("dbase_iterate", false));
	if (dbase_iterate()) { /// put the end_dbase_subset_ variable on the datamap for LoopOver & MC to be sensitive to it
		std::string const curr_mover_name(tag->getOption<std::string>("name"));
		data.add("stopping_condition", curr_mover_name, end_dbase_subset_);
		TR << "Placed stopping_condition " << curr_mover_name << " on the DataMap" << std::endl;
	}
	if (tag->hasOption("locked_residue")) {
		locked_res(core::pose::parse_resnum(tag->getOption<std::string>("locked_residue"), pose));
		locked_res_id(pose.residue(locked_res()).name1());
		TR << "locking residue " << locked_res() << " of identity " << locked_res_id() << std::endl;
	}
	checkpointing_file(tag->getOption<std::string>("checkpointing_file", ""));
	loop_dbase_file_name(tag->getOption<std::string>("loop_dbase_file_name", ""));
	if (tag->hasOption("splice_filter"))
		splice_filter(protocols::rosetta_scripts::parse_filter(tag->getOption<std::string>("splice_filter"), filters));
	if (tag->hasOption("mover_tag"))
		mover_tag_ = basic::datacache::get_set_from_datamap<basic::datacache::DataMapObj<std::string> >("tags",
				tag->getOption<std::string>("mover_tag"), data);
	loop_pdb_source(tag->getOption<std::string>("loop_pdb_source", ""));

	restrict_to_repacking_chain2(tag->getOption<bool>("restrict_to_repacking_chain2", true));

	TR << "from_res: " << from_res() << " to_res: " << to_res() << " dbase_iterate: " << dbase_iterate()
			<< " randomize_cut: " << randomize_cut() << " cut_secondarystruc: " << cut_secondarystruc() << " source_pdb: "
			<< source_pdb() << " ccd: " << ccd() << " rms_cutoff: " << rms_cutoff() << " res_move: " << res_move()
			<< " template_file: " << template_file() << " checkpointing_file: " << checkpointing_file_
			<< " loop_dbase_file_name: " << loop_dbase_file_name_ << " loop_pdb_source: " << loop_pdb_source()
			<< " mover_tag: " << mover_tag_ << " torsion_database: " << torsion_database_fname_
			<< " restrict_to_repacking_chain2: " << restrict_to_repacking_chain2() << " rb_sensitive: " << rb_sensitive()
			<< std::endl;
	use_sequence_profiles_=tag->getOption<bool>("use_sequence_profiles", true);
}

protocols::moves::MoverOP Splice::clone() const {
	return (protocols::moves::MoverOP(new Splice(*this)));
}

void Splice::scorefxn(core::scoring::ScoreFunctionOP sf) {
	scorefxn_ = sf;
}

core::scoring::ScoreFunctionOP Splice::scorefxn() const {
	return scorefxn_;
}

core::pack::task::TaskFactoryOP Splice::task_factory() const {
	return task_factory_;
}

void Splice::task_factory(core::pack::task::TaskFactoryOP tf) {
	task_factory_ = tf;
}

core::pack::task::TaskFactoryOP Splice::design_task_factory() const {
	return design_task_factory_;
}

void Splice::design_task_factory(core::pack::task::TaskFactoryOP tf) {
	design_task_factory_ = tf;
}

/// the torsion dbase should have the following structure:
/// each line represents a single loop. Each four values represent <phi> <psi> <omega> <3-let resid>; the last entry in a line represents <loop start> <loop stop> <cut site> cut; where cut signifies that this is the loop designator
void Splice::read_torsion_database() {
	using namespace std;
	TR << "Reading torsion database" << std::endl;
	utility::io::izstream data(torsion_database_fname_);
	if (!data) {
		utility_exit_with_message("cannot open torsion database " + torsion_database_fname_ + "\n");
	}
	std::string line;//go through torsion DB line be line
	std::string line_tail;
	while (getline(data, line)) {
		utility::vector1<std::string> elements_in_line(utility::string_split(line, ' '));
		if (elements_in_line.size() % 4 != 0)
			utility_exit_with_message("While reading torsion database " + torsion_database_fname_+ " found a line where the number of elements is not divisible by 4. This likely stems from an error in the database:\n"+ line);
		//Lines with tail dofs are marked with a '*' so we look for that in the torsion db to know that we dealing with tail segemtn line
		//if we find tail segment dofs we break the line into 2.
		core::Size tail_element=0;
		line_tail="";
		for (core::Size i=1; i<=elements_in_line.size(); i++){
			  if (elements_in_line[i].find('*')!=std::string::npos){
				  tail_element=i;
				  line="";
				//	const size_t last = elements_in_line[i].find('*');
					/*remove * from element*/
					// TODO: Do we just erase the last charachter, or should we instead actually look for the '*' charachter?
					//const size_t last = elements_in_line[i].find('*');
					elements_in_line[i].erase(elements_in_line[i].size()-1);
				  break;
			  }
		}
		for (core::Size i=1; i<=tail_element; i++){
			line=line+elements_in_line[i]+" ";
		}
		for (core::Size i=tail_element+1; i<=elements_in_line.size(); i++){
			line_tail=line_tail+elements_in_line[i]+" ";
		}
		using namespace boost::algorithm;
		trim(line_tail);
		std::istringstream tail_line_stream(line_tail);
		ResidueBBDofs bbdof_entry;
		bbdof_entry.clear();
		if (debug_){
			TR<<"tail bbdof_entry size is:"<<bbdof_entry.size()<<std::endl;
			TR<<"Tail dofs:"<<std::endl;
			TR<<tail_line_stream.str()<<std::endl;
		}
		while (!tail_line_stream.eof()) {
			std::string phi, psi, omega,resn;
			tail_line_stream >> phi >> psi >> omega >> resn;
			if (tail_line_stream.eof()) { // the end of the line signifies that we're reading the start, stop, cut, source_pdb fields
				bbdof_entry.disulfide(atoi(phi.c_str()));
				bbdof_entry.tail_segment(psi);
				bbdof_entry.cut_site(atoi(resn.c_str()));
				bbdof_entry.source_pdb(omega);

			}
			else
				bbdof_entry.push_back(BBDofs(0/*resstd::map<std::string, core::Size> cys_pos;//store all cysteine positions in the AB chainid*/,std::strtod(phi.c_str(),0), std::strtod(psi.c_str(),0), std::strtod(omega.c_str(),0), resn)); /// resid may one day be used. Currently it isn't
		}

		tail_torsion_database_.push_back(bbdof_entry);
		//TR<<"tail bbdof_entry size is:"<<bbdof_entry.size()<<std::endl;
		//TR<<"tail bbdof_entry name is:"<<bbdof_entry.source_pdb()<<std::endl;
		using namespace boost::algorithm;
		trim(line);
		std::istringstream line_stream(line);
		bbdof_entry.clear();
		//TR<<"Segment dofs:"<<std::endl;
		//TR<<line<<std::endl;
		while (!line_stream.eof()) {
			core::Real phi, psi, omega;
			std::string resn;
			line_stream >> phi >> psi >> omega >> resn;
			if (line_stream.eof()) { // the end of the line signifies that we're reading the start, stop, cut, source_pdb fields
				bbdof_entry.start_loop((core::Size) phi);
				bbdof_entry.stop_loop((core::Size) psi);
				bbdof_entry.cut_site((core::Size) omega);
				bbdof_entry.source_pdb(resn);
			}
			else
				bbdof_entry.push_back(BBDofs(0/*resstd::map<std::string, core::Size> cys_pos;//store all cysteine positions in the AB chainid*/,phi, psi, omega, resn)); /// resid may one day be used. Currently it isn't
		}
		torsion_database_.push_back(bbdof_entry);
		//TR<<"segment bbdof_entry size is:"<<bbdof_entry.size()<<std::endl;
	}
	TR << "Finished reading torsion database with " << torsion_database_.size() << " entries" << std::endl;
}

///@Set a general fold tree to use for all antibodies, gideon, Apr2014
void Splice::set_fold_tree(core::pose::Pose & pose, core::Size const vl_vh_cut) {
	using namespace protocols::rosetta_scripts;
	core::kinematics::FoldTree ft;
	ft.clear();
	protocols::simple_moves::CutChainMover ccm;
	//core::Size template_pdb_cut_vl_vh=ccm.chain_cut(*template_pose_);
	utility::vector1<core::Size> cys_pos; //store all cysteine positions in the AB chain
	std::map<std::string, core::Size> pose_cut_pts; //store all cut points of pose
	std::map<std::string, core::Size> pose_start_pts;
	std::map<std::string, core::Size> pose_end_pts;
	//find all cysteines in the pose
	for (core::Size i = 1; i <= pose.total_residue(); ++i) {
		if (pose.residue(i).has_variant_type(core::chemical::DISULFIDE)) {
			cys_pos.push_back(i);
		}
	}
	//"comments" gives us the association between the segment name and the source pdb
	std::map<std::string, std::string> comments = core::pose::get_all_comments(pose);

	BOOST_FOREACH( std::string const segment_type, segment_names_ordered_ ) {
		protocols::protein_interface_design::movers::AddChainBreak acb;
		TR<<"segment:" <<segment_type<<std::endl;
		torsion_database_fname_=database_segment_map_[segment_type];
		TR<<"db_fname:" <<torsion_database_fname_<<std::endl;
		std::string pdb_source_name=comments["segment_"+segment_type]; //segments in the pose comments have a prefix
		TR<<"pdb_source:" <<pdb_source_name<<std::endl;
		Splice::read_torsion_database();
//bool found_source_name_in_db=false;// in the unlikely event that the source pdb is not db file, exit with error message
		core::Size dbase_entry=0;
//core::Size residue_diff=0;
		core::Size template_cut_site=0;
		core::Size template_start=0;
		core::Size template_end=0;
		core::Size template_nearest_disulfide=0;
		core::Size template_disulfide_cut_length=0;
//find the pdb_source_in_the_db_files
		for (core::Size i = 1; i <= torsion_database_.size(); ++i) {
			if (torsion_database_[i].source_pdb()== pdb_source_name) {
				TR << "Found entry for " << pdb_source_name << " at number " << i<< std::endl;
				template_cut_site=torsion_database_[i].cut_site();
				TR<<"template cut site is:"<<template_cut_site<<std::endl;
				template_start=torsion_database_[i].start_loop();
				TR<<"template start is: "<<template_start<<std::endl;
				template_end=torsion_database_[i].stop_loop();
				TR<<"template end is: "<<template_end<<std::endl;
				template_nearest_disulfide=find_nearest_disulfide(*template_pose_,template_start);
				TR<<"template nearest_disulfide is: "<<template_nearest_disulfide<<std::endl;
				template_disulfide_cut_length=template_cut_site-template_nearest_disulfide;
				TR<<"template disulfide_cut_length is: "<<template_disulfide_cut_length<<std::endl;
				dbase_entry = i;
				break;
			}
		}
		if (!dbase_entry) {
			utility_exit_with_message("can't find torsion data for "+pdb_source_name+"in file: "+torsion_database_fname_+"\n");
		}
		TR<<"current segemtn: "<<segment_type<<std::endl;
		pose_start_pts[segment_type]=find_nearest_res(pose,*template_pose_, template_start, 1);
		TR<<"pose start_point is"<<pose_start_pts[segment_type]<<std::endl;
		pose_end_pts[segment_type]=find_nearest_res(pose,*template_pose_, template_end, 1);
		TR<<"pose end_point is"<<pose_end_pts[segment_type]<<std::endl;
		int res_diff=(pose_end_pts[segment_type]-pose_start_pts[segment_type])-(template_end-template_start);
		TR<<"res_diff is"<<res_diff<<std::endl;
		core::Size nearest_disulfide_pose=find_nearest_res(pose,*template_pose_, template_nearest_disulfide, 1/*chain*/);
		TR<<"nearest_disulfide_pose:"<<nearest_disulfide_pose<<std::endl;
		pose_cut_pts[segment_type]=nearest_disulfide_pose+template_disulfide_cut_length+res_diff;
		TR<<"pose_cut_pts:"<<pose_cut_pts[segment_type]<<std::endl;
		acb.resnum(utility::to_string(pose_cut_pts[segment_type]));
		acb.find_automatically(false);
		acb.change_foldtree(false);
		acb.apply(pose);

		torsion_database_.clear(); //if I don't clear this every time then the database will be pushed one on top of the other
	}

	//set fold tree for vl section
	ft.add_edge(cys_pos[1], 1, -1);
	ft.add_edge(cys_pos[1], pose_cut_pts["L1_L2"], -1);
	ft.add_edge(cys_pos[2], pose_cut_pts["L1_L2"] + 1, -1);
	ft.add_edge(cys_pos[2], cys_pos[1], 2);
	ft.add_edge(cys_pos[2], pose_cut_pts["L3"], -1);
	ft.add_edge(pose_end_pts["L3"], pose_cut_pts["L3"] + 1, -1);
	ft.add_edge(cys_pos[2], pose_end_pts["L3"], 3);
	ft.add_edge(pose_end_pts["L3"], vl_vh_cut, -1);
	//set fold tree for vh section
	ft.add_edge(cys_pos[3], vl_vh_cut + 1, -1);
	ft.add_edge(cys_pos[3], pose_cut_pts["H1_H2"], -1);
	ft.add_edge(cys_pos[4], pose_cut_pts["H1_H2"] + 1, -1);
	ft.add_edge(cys_pos[4], cys_pos[3], 4);
	ft.add_edge(cys_pos[4], pose_cut_pts["H3"], -1);
	ft.add_edge(pose_end_pts["H3"], pose_cut_pts["H3"] + 1, -1);
	ft.add_edge(cys_pos[4], pose_end_pts["H3"], 5);
	ft.add_edge(pose_end_pts["H3"], pose.total_residue(), -1);
	ft.add_edge(cys_pos[2], cys_pos[4], 1); //vl/vh jump

	ft.delete_self_edges();
	ft.check_fold_tree();

	TR << "single chain ft : " << ft << std::endl;
	ft.reorder(cys_pos[2]);
	ft.check_fold_tree();

	pose.fold_tree(ft);
	TR << "single chain ft : " << ft << std::endl;
	return;
}

///@brief Setup fold tree for segments at the termini of the protein, gdl, Apr2014
void Splice::tail_fold_tree(core::pose::Pose & pose, core::Size const vl_vh_cut, core::Size chain_break) const {
	using namespace protocols::loops;
//	TR << "cut point after llc " << vl_vh_cut << std::endl;
	core::conformation::Conformation const & conf(pose.conformation());
	core::kinematics::FoldTree ft;
	utility::vector1<core::Size> cys_pos; //store all cysteine positions in the AB chain

	for (core::Size i = 1; i <= conf.chain_end(1); ++i) {
		if (pose.residue(i).has_variant_type(core::chemical::DISULFIDE)) {
			cys_pos.push_back(i);
		}
	}
	ft.clear();
	ft.add_edge(1, cys_pos[1], -1);
	ft.add_edge(cys_pos[1], cys_pos[2], -1);
	ft.add_edge(cys_pos[2], vl_vh_cut, -1);
	ft.add_edge(cys_pos[2], cys_pos[4], 1);
	ft.add_edge(cys_pos[3], vl_vh_cut + 1, -1);
	ft.add_edge(cys_pos[4], cys_pos[3], -1);
	ft.add_edge(cys_pos[4], conf.chain_end(1), -1);
	//ft.delete_self_edges();


	if (chain_break!=0){//This fold tree should include the chainbreak edges
		if (segment_type_=="L1_L2"){
			ft.delete_unordered_edge(cys_pos[1], cys_pos[2], -1);
			ft.add_edge(cys_pos[1],chain_break,-1);
			ft.add_edge(cys_pos[2],chain_break+1,-1);
			ft.add_edge(cys_pos[1],cys_pos[2],2);
		}
		else{
			ft.delete_unordered_edge(cys_pos[4], cys_pos[3], -1);
			ft.add_edge(cys_pos[3],chain_break,-1);
			ft.add_edge(cys_pos[4],chain_break+1,-1);
			ft.add_edge(cys_pos[4],cys_pos[3],2);
		}
	}
	if (conf.num_chains()>1){//if ligand is present we need to add edge between receptor and ligand
			core::Size jump_num= ft.num_jump();
			TR<<"Number of jumps in the fold tree is:"<<jump_num<<std::endl;
			ft.add_edge(conf.chain_begin(2), conf.chain_end(2), -1);
			ft.add_edge(cys_pos[4], conf.chain_begin(2), jump_num+1);

	}
	ft.reorder(cys_pos[4]);
	//ft.reorder(1);
	ft.check_fold_tree();
	//TR<<"Is the tree foldable: "<<tree_foldable<<std::endl;
	TR << "ft for tail segment: " << ft << std::endl;
	pose.fold_tree(ft);
	return;
}

///@brief set the fold tree around start/stop/cut sites.
/// presently makes a simple fold tree, but at one point may be a more complicated function to include two poses
void Splice::fold_tree(core::pose::Pose & pose, core::Size const start, core::Size const stop, core::Size const cut) const {
	using namespace protocols::loops;
	core::conformation::Conformation const & conf(pose.conformation());
	core::Size const s1 = start;
	core::Size const s2 = stop;
	core::kinematics::FoldTree ft;
	ft.clear();
	if (conf.num_chains() == 1) {		/// build simple ft for the cut
		ft.add_edge(1, s1-1, -1);
		ft.add_edge(s1-1, s2+1, 1);
		ft.add_edge(s1-1, cut, -1);
		ft.add_edge(s2+1, cut + 1, -1);
		ft.add_edge(s2+1, pose.total_residue(), -1);
		ft.delete_self_edges();
		//ft.reorder(s2);
		TR << "single chain ft: " << ft << std::endl;
		pose.fold_tree(ft);
		return;
	}
	if( chain_num() > 1 ){ // build simple ft surrounding the loop
		ft.add_edge( 1, start-1, -1 );
		ft.add_edge( start-1, stop+1, 1 );
		ft.add_edge( start-1, cut, -1 );
		ft.add_edge( stop+1, cut + 1, -1 );
		ft.add_edge( stop+1, pose.total_residue(), -1 );
		pose.fold_tree( ft );
		return;
	}
	//core::Size from_res( 0 );
	for (core::Size resi = conf.chain_begin(1); resi <= conf.chain_end(1); ++resi) {
		if (pose.residue(resi).has_variant_type(core::chemical::DISULFIDE)) {
			//from_res = resi;  // set but never used ~Labonte
			break;
		}
	}
	ft.add_edge(1, s1-1, -1);
	ft.add_edge(s1-1, s2+1, 1);
	ft.add_edge(s2+1, conf.chain_end(1), -1);
	if (locked_res() > 0 && (locked_res() <= s2 && locked_res() >= s1)) {
		TR << "s1,s2,locked_res: " << s1 << ',' << s2 << ',' << locked_res() << std::endl;
		if (locked_res() < cut) {
			ft.add_edge(s1, locked_res(), -1);
			ft.add_edge(locked_res(), cut, -1);
			ft.add_edge(s2, cut + 1, -1);
		}
		if (locked_res() > cut) {
			ft.add_edge(s1, cut, -1);
			ft.add_edge(s2, locked_res(), -1);
			ft.add_edge(locked_res(), cut + 1, -1);
		}
		if (locked_res() == cut) {
			ft.add_edge(s1, cut, -1);
			ft.add_edge(s2, cut + 1, -1);
		}
		using namespace protocols::protein_interface_design;
		std::string const from_atom(optimal_connection_point(pose.residue(locked_res()).name3()));
		core::Real min_dist(100000);
		core::Size nearest_res(0);
		core::Size nearest_atom(0);
		for (core::Size resi = conf.chain_begin(2); resi <= conf.chain_end(2); ++resi) {
			core::conformation::Residue const residue(conf.residue(resi));
			if (residue.is_ligand())
				continue;
			for (core::Size atomi = 1; atomi <= residue.natoms(); ++atomi) {
				core::Real const dist(conf.residue(locked_res()).xyz(from_atom).distance(residue.xyz(atomi)));
				if (dist <= min_dist) {
					nearest_res = resi;
					nearest_atom = atomi;
					min_dist = dist;
				}
			}
		}
		runtime_assert(nearest_res);
		ft.add_edge(locked_res(), nearest_res, 2);
		ft.add_edge(nearest_res, conf.chain_begin(2), -1);
		ft.add_edge(nearest_res, conf.chain_end(2), -1);
		ft.set_jump_atoms(2, from_atom, conf.residue(nearest_res).atom_name(nearest_atom));
	}
	else {
		if (locked_res() > 0 && !(locked_res() > s1 && locked_res() < s2)) {
			TR << "locked_res " << locked_res() << " is outside loop scope so ignoring" << std::endl;
		}

		ft.add_edge(s1-1, cut, -1);
		ft.add_edge(s2+1, cut + 1, -1);
//		ft.add_edge(s2, conf.chain_end(1), -1); // SJF 22Jul14 this line already appears above, leading to bad fold tree here...
		ft.add_edge(1, conf.chain_begin(2), 2);
		ft.delete_self_edges();
		TR<<"fold tree:"<<ft<<std::endl;
	}
	if ((!locked_res() || (locked_res() <= s1 || locked_res() >= s2)) && !pose.residue(conf.chain_begin(2)).is_ligand())
		ft.add_edge(conf.chain_begin(2), conf.chain_end(2), -1);
	ft.reorder(1);
	ft.check_fold_tree();
	TR << "Previous ft: " << pose.fold_tree() << std::endl;
	//	pose.dump_pdb( "before_ft.pdb" );
	pose.fold_tree(ft);
	//	pose.dump_pdb( "after_ft.pdb" );
	TR << "Current ft: " << pose.fold_tree() << std::endl;
}

BBDofs::~BBDofs() {
}

ResidueBBDofs::~ResidueBBDofs() {
}

utility::vector1<core::Size>::const_iterator Splice::dbase_begin() const {
	return dbase_subset_.begin();
}

utility::vector1<core::Size>::const_iterator Splice::dbase_end() const {
	return dbase_subset_.end();
}

core::Size Splice::locked_res() const {
	if (locked_res_)
		return locked_res_->obj[1];
	else
		return 0;
}

void Splice::locked_res(core::Size const r) {
	locked_res_->obj[1] = r;
}

void Splice::locked_res_id(char const c) {
	locked_res_id_ = c;
}

char Splice::locked_res_id() const {
	return locked_res_id_;
}

std::string Splice::checkpointing_file() const {
	return checkpointing_file_;
}

void Splice::checkpointing_file(std::string const cf) {
	checkpointing_file_ = cf;
}

void Splice::loop_dbase_file_name(std::string const s) {
	loop_dbase_file_name_ = s;
}

std::string Splice::loop_dbase_file_name() const {
	return loop_dbase_file_name_;
}

void Splice::loop_pdb_source(std::string const s) {
	loop_pdb_source_ = s;
}

std::string Splice::loop_pdb_source() const {
	return loop_pdb_source_;
}

protocols::filters::FilterOP Splice::splice_filter() const {
	return splice_filter_;
}

void Splice::splice_filter(protocols::filters::FilterOP f) {
	splice_filter_ = f;
}

void		//is anyone using this function ? gideonla. NOv13
Splice::read_splice_segments(std::string const segment_type, std::string const segment_name, std::string const file_name) {
	if (use_sequence_profiles_) {
		splice_segments_[segment_type]->read_profile(file_name, segment_name);
		TR << "In segment_type " << segment_type_ << ": reading profile for segment " << segment_name << " from file "
				<< file_name << std::endl;
	}
}

core::sequence::SequenceProfileOP Splice::generate_sequence_profile(core::pose::Pose & pose) {
	if (!use_sequence_profiles_)
		return NULL;
		using namespace core::sequence;
		using namespace std;

		using namespace basic::options;
		using namespace basic::options::OptionKeys;
		using protocols::jd2::JobDistributor;

		std::string pdb_tag = JobDistributor::get_instance()->current_job()->inner_job()->input_tag();

//	std::string temp_pdb_name = pdb_tag.erase(pdb_tag.size() - 5); //JD adds "_0001" to input name, we need to erase it
//pdb_tag = temp_pdb_name +".pdb";
		TR << " The scaffold file name is :" << pdb_tag << std::endl;		//file name of -s pdb file
//		core::pose::read_comment_pdb(pdb_tag,pose); //read comments from pdb file
		/*		std::string pdb_dump_fname_("test2");
		 std::ofstream out( pdb_dump_fname_.c_str() );
		 pose.dump_pdb(out); //Testing out comment pdb, comment this out after test (GDL) */
		map<string, string> const comments = core::pose::get_all_comments(pose);
		if (comments.size() < 1) { /// SJF changed from <3 22Jul14
			utility_exit_with_message(
					"Please check comments field in the pdb file (header= ##Begin comments##), could not find any comments");
		}
///This code will cut the source pdb file name and extract the four letter code
		if (torsion_database_fname_ != "") {
			TR << "Torsion data base filename is: " << torsion_database_fname_ << std::endl;
			Pdb4LetName_ = dofs_pdb_name;
			//TR<<"Pdb4LetName_: "<<Pdb4LetName_<<std::endl;
			/*	    std::string pdb_dump_fname_("before_splice.pdb");
			 std::ofstream out( pdb_dump_fname_.c_str() );
			 pose.dump_pdb(out); //Testing out comment pdb, comment this out after test (GDL) */

		}
		else {
			Pdb4LetName_ = parse_pdb_code(source_pdb_);		//
		}
		if (!add_sequence_constraints_only_) {///If only doing sequence constraints then don't add to pose comments source name
			TR << "The current segment is: " << segment_type_ << " and the source pdb is " << Pdb4LetName_ << std::endl;
			core::pose::add_comment(pose, "segment_" + segment_type_, Pdb4LetName_);//change correct association between current loop and pdb file
		}
		load_pdb_segments_from_pose_comments(pose); // get segment name and pdb accosiation from comments in pdb file
		TR << "There are " << pdb_segments_.size() << " PSSM segments" << std::endl;

		runtime_assert(pdb_segments_.size()); //This assert is in place to make sure that the pdb file has the correct comments, otherwise this function will fail

		utility::vector1<SequenceProfileOP> profile_vector;

		profile_vector.clear(); //this vector holds all the pdb segment profiless

		BOOST_FOREACH( std::string const segment_type, segment_names_ordered_ ) { //<- Start of PDB segment iterator
			TR<<"segment_type: "<<segment_type<<std::endl;
			if (splice_segments_[ segment_type ]->pdb_profile(pdb_segments_[segment_type])==0) {
				utility_exit_with_message(" could not find the source pdb name: "+ pdb_segments_[segment_type]+ ", in pdb_profile_match file."+segment_type+" or PSSM file is missing\n");
			}
			TR<<"reading profile:"<< pdb_segments_[segment_type]<<std::endl;
			profile_vector.push_back( splice_segments_[ segment_type ]->pdb_profile( pdb_segments_[segment_type] ));
		} // <- End of PDB segment iterator
		TR << "The size of the profile vector is: " << profile_vector.size() << std::endl;

///Before upwweighting constraint we check that the PSSMs are properly aligned by making sure
///that the PSSM score of the falnking segments of the current designed segment agree with the identity of the aa (i.e if
/// we are designing L1 then we would expect that segments Frm.light1 and Frm.light2 have concensus aa identities)

		if ((ccd_) && (protein_family_.compare("antibodies") == 0)) { //if CCD is true and we doing this on antibodies then we're splicing out a segment, so it's important to ensure that the disulfides are in place. In rare cases where there are highly conserved cysteines in other parts of the protein this might lead to exit, but these cases are < 1/1000
			core::Size aapos = 0;
			//TR<<"TESTING PSSMs"<<std::endl;
			for (core::Size seg = 1; seg <= profile_vector.size(); seg++) {	//go over all the PSSM sements provided by the user
				for (core::Size pos /*go over profile ids*/= 1; pos <= profile_vector[seg]->size(); ++pos) {
					++aapos;		//go over pose residue
					TR_pssm << pose.residue(aapos).name1() << aapos << ","<< profile_vector[seg]->prof_row(pos) << std::endl;
					if ((profile_vector[seg]->prof_row(pos)[2]) > 8) {//If the profile vector holds a disulfide Cys it will have a pssm score over 8
						std::stringstream ss;
						std::string s;
						ss << pose.residue(aapos).name1();
						ss >> s;
						//TR<<"found a dis cys="<<s<<std::endl;
						if (s.compare("C") != 0) {
							std::string seqpos;
							std::ostringstream convert;
							convert << aapos; // insert the textual representation of 'Number' in the characters in the stream
							seqpos = convert.str();
							pose.dump_pdb( Pdb4LetName_+"_align_problem.pdb");
							utility_exit_with_message(" PSSM and pose might be misaligned, position " + s + seqpos + " should be a CYS\n");
						} //fi
					} //fi
				} //end inner segment for
			} //end pssm segment for
		}
		std::map<std::string,std::string> pose_comments=get_all_comments(pose);
		std::string H3_pdb_source=pose_comments["segment_H3"];
		//TR<<"H3_pdb_source: "<<H3_pdb_source<<std::endl;
		std::string H3_seq="";
		//TR<<"Size of pdb_to_H3_seq_map_: "<<pdb_to_H3_seq_map_.size()<<std::endl;
		if (pdb_to_H3_seq_map_.size()){
				H3_seq=pdb_to_H3_seq_map_.find(H3_pdb_source)->second;
		}
		//TR<<"H3 sequence: "<<H3_seq<<std::endl;
		return concatenate_profiles(profile_vector, segment_names_ordered_,H3_seq);

}

void Splice::load_pdb_segments_from_pose_comments(core::pose::Pose const & pose) {
	//	if(use_sequence_profiles_){
	//If we are using sequence profiles then the condition is true and function can run
	using namespace std;
	map<string, string> const comments = core::pose::get_all_comments(pose);
	TR << "The size of comments is: " << comments.size() << std::endl;
	for (std::map<string, string>::const_iterator i = comments.begin(); i != comments.end(); ++i) {
//TR<<"the size of j is: "<<j<<std::endl;
		std::string const key(i->first);
//TR<<"the size of j after i->first is: "<<j<<std::endl;
		std::string const val(i->second);
//TR<<"the size of j after i->second is: "<<j<<std::endl;
		if (key.substr(0, 7) != "segment") /// the expected format is segment_??, where we're interested in ??
			continue;
		std::string const short_key(key.substr(8, 1000));
		pdb_segments_[short_key] = val;
		TR << "recording segment/pdb pair: " << short_key << '/' << val << std::endl;
	}
	//	}
}

void Splice::modify_pdb_segments_with_current_segment(std::string const pdb_name) {
	pdb_segments_[segment_type_] = pdb_name;
}

// @brief utility function for computing which residues on chain1 are away from the interface
utility::vector1<core::Size> find_residues_on_chain1_inside_interface(core::pose::Pose const & pose, core::Size chainNum) {
	utility::vector1<core::Size> const chain1_interface;
	if (pose.conformation().num_chains()==1)
		return chain1_interface;//if no ligand then there are no interface residues;


	using namespace protocols::toolbox::task_operations;
	ProteinInterfaceDesignOperationOP pido = new ProteinInterfaceDesignOperation;
	if (chainNum == 1) {
		pido->repack_chain1(true);
		pido->design_chain1(true);
		pido->repack_chain2(false);
		pido->design_chain2(false);
		pido->interface_distance_cutoff(8.0);
		core::pack::task::TaskFactoryOP tf_interface(new core::pack::task::TaskFactory);
		tf_interface->push_back(pido);
///// FIND COMPLEMENT ////////
		utility::vector1<core::Size> const chain1_interface(
				protocols::rosetta_scripts::residue_packer_states(pose, tf_interface, true, true)); /// find packable but not designable residues; according to pido specifications above these will be on chain1 outside an 8A shell around chain2
		return chain1_interface;
	}
	else { //if user defined chain 2 as binder
		pido->repack_chain1(false);
		pido->design_chain1(false);
		pido->repack_chain2(true);
		pido->design_chain2(true);
		pido->interface_distance_cutoff(8.0);
		core::pack::task::TaskFactoryOP tf_interface(new core::pack::task::TaskFactory);
		tf_interface->push_back(pido);
///// FIND COMPLEMENT ////////
		utility::vector1<core::Size> const chain1_interface(
				protocols::rosetta_scripts::residue_packer_states(pose, tf_interface, true, true)); /// find packable but not designable residues; according to pido specifications above these will be on chain1 outside an 8A shell around chain2
		return chain1_interface;
	}

}

void Splice::add_sequence_constraints(core::pose::Pose & pose) {
	if (!add_sequence_constraints_only_) { ///If only doing sequence constraints then don't add to pose comments source name
		TR << "The current segment is: " << segment_type_ << " and the source pdb is " << Pdb4LetName_ << std::endl;
		core::pose::add_comment(pose, "segment_" + segment_type_, Pdb4LetName_); //change correct association between current loop and pdb file
	}

	if (use_sequence_profiles_) {
		using namespace core::scoring::constraints;

/// first remove existing sequence constraints
		TR << "Removing existing sequence profile constraints from pose" << std::endl;
		ConstraintCOPs constraints(pose.constraint_set()->get_all_constraints());
		TR << "Total number of constraints at start: " << constraints.size() << std::endl;
		core::Size cst_num(0);
		BOOST_FOREACH( ConstraintCOP const c, constraints ) {
			if( c->type() == "SequenceProfile" ) { //only remove profile sequence constraints
				pose.remove_constraint( c );
				cst_num++;
			}
		}
		TR << "Removed a total of " << cst_num << " sequence constraints." << std::endl;
		TR << "After removal the total number of constraints is: " << pose.constraint_set()->get_all_constraints().size()
				<< std::endl;
/// then impose new sequence constraints
		core::sequence::SequenceProfileOP seqprof(generate_sequence_profile(pose));
		TR << "chain_num: " << chain_num_ << std::endl;
		TR << "Chain length/seqprof size: "	<< pose.conformation().chain_end(chain_num_) - pose.conformation().chain_begin(chain_num_) + 1 << ", "<< seqprof->size() - 1 << std::endl;
		/*			std::string pdb_dump_fname_("after_splice.pdb");
		 std::ofstream out( pdb_dump_fname_.c_str() );
		 pose.dump_pdb(out); //Testi*/
		runtime_assert(	seqprof->size() - 1	== pose.conformation().chain_end(chain_num_) - pose.conformation().chain_begin(chain_num_) + 1); //Please note that the minus 1 after seqprof size is because seqprof size is always +1 to the actual size. Do not chnage this!!
		cst_num = 0;
		TR << "Up-weighting sequence constraints " << std::endl;

//If pose has more than one chain the sequence profile mapping needs to be modified accordingly
		core::id::SequenceMappingOP smap = new core::id::SequenceMapping();
		for (core::Size seqpos = 1; seqpos <= pose.total_residue(); ++seqpos) {
			if ((seqpos >= pose.conformation().chain_begin(chain_num_))
					and (seqpos <= pose.conformation().chain_end(chain_num_))) {
				smap->push_back(seqpos - pose.conformation().chain_begin(chain_num_) + 1);
				//if position is in chain >1 then it should have a sequence profile, the mapping should look like this: 0|0|0|1|2|3|
				// So the first positions ('0') does not have a seqeuence constraint
			}
			else {
				smap->push_back(0);
			}
		}
//smap->show();//uncomment to see mapping in tracer

			utility::vector1<core::Size> const non_upweighted_residues(find_residues_on_chain1_inside_interface(pose, chain_num_));
		for (core::Size seqpos = pose.conformation().chain_begin(chain_num_);	seqpos <= pose.conformation().chain_end(chain_num_); ++seqpos) {
			using namespace core::scoring::constraints;

			SequenceProfileConstraintOP spc(new SequenceProfileConstraint(pose, seqpos, seqprof, smap));
			//TR << "sepos=" << spc->seqpos() << std::endl;
			if (std::find(non_upweighted_residues.begin(), non_upweighted_residues.end(), seqpos)
					== non_upweighted_residues.end()) { //seqpos not in interface so upweight
				spc->weight(profile_weight_away_from_interface());
			}
			//TR << "Now adding constraint to aa: " << seqpos << pose.aa(seqpos)<< std::endl;
			//TR<<"The sequence profile row for this residue is: "<<seqprof->prof_row(seqpos-)<<std::endl;
			pose.add_constraint(spc);
			cst_num++;
			//TR << "Current constraints size: "<< pose.constraint_set()->get_all_constraints().size() << std::endl;
		}
		TR << "Added a total of " << cst_num << " sequence constraints." << std::endl;
		TR << "Now the pose has a total of " << pose.constraint_set()->get_all_constraints().size() << " constraints"
				<< std::endl;

/// just checking that the scorefxn has upweighted res_type_constraint
		core::Real const score_weight(scorefxn()->get_weight(core::scoring::res_type_constraint));
		TR << "res_type_constraint weight is set to " << score_weight << std::endl;
		if (score_weight <= 0.001)
			TR
					<< "Warning! res_type_constraint weight is low, even though I've just added sequence constraints to the pose! These sequence constraints will have no effect. This could be an ERROR"
					<< std::endl;
	}
}

///@brief apply coordinate constraints on the segment being inserted. "to" and "from" are residue number of the pose(!), anchor residue number is also on the pose
void Splice::add_coordinate_constraints(core::pose::Pose & pose, core::pose::Pose const & source_pose, core::Size from, core::Size to, core::Size anchor, std::string atom_type, core::pack::task::PackerTaskOP task) {
	core::scoring::constraints::ConstraintOPs cst;
	core::Size anchor_source = protocols::rosetta_scripts::find_nearest_res(source_pose, pose, anchor, 1/*chain*/);
	TR_constraints << "closest residue to anchor residue on source is : " <<anchor_source << std::endl;
	int res_diff = anchor_source - anchor;
	TR_constraints << "res diff in coordinate constarint is: " << res_diff << std::endl;
	core::Size const fixed_res(anchor);
	TR_constraints << "Anchor residue for the coordinate constraint is " << fixed_res << std::endl;
	TR_constraints << "Current pose "<<atom_type<< " xyz coordinate/source pdb "<<atom_type<< " xyz coordinate:" << std::endl;
	core::id::AtomID const anchor_atom(core::id::AtomID(pose.residue(fixed_res).atom_index("CA"), fixed_res));
	for (core::Size i = from; i <= to; ++i) {
		if (atom_type=="CB" && ((pose.residue(i).name3()=="GLY")||(source_pose.residue(i + res_diff).name3()=="GLY"))){//Gly doesn't have CB so we should just skip
			TR_constraints<<"Found GLY! Not applying CB constraints here"<<std::endl;
			continue;
		}

		if (atom_type=="CG"){
			if (pose.fold_tree().is_cutpoint(i)){
						TR<<i<<"is cutpoint skipping"<<std::endl;
						continue;//don't want to add CG constraints to cut point residues. This causes problems to CCD.
					}
			TR_constraints<<"Allowed aa's for residue "<<i<<" are: ";
			std::list< core::chemical::ResidueTypeCOP > allowed_aas =task->residue_task(i).allowed_residue_types();
			std::vector<char> allowed_aas_names;
			allowed_aas_names.clear();
			for (std::list< core::chemical::ResidueTypeCOP >::const_iterator restype = allowed_aas.begin();restype != allowed_aas.end(); ++restype) {
				TR_constraints<<(*restype )->name1()<<",";
				allowed_aas_names.push_back((*restype )->name1());
			}
			TR_constraints<<std::endl;
			std::vector<char> aromatic_and_his;
			aromatic_and_his.clear();
			aromatic_and_his.push_back('Y');aromatic_and_his.push_back('F');aromatic_and_his.push_back('W');aromatic_and_his.push_back('H');
			std::vector<char> intersect;
	    std::sort(allowed_aas_names.begin(), allowed_aas_names.end());
	    std::vector<char>::iterator it;
	    it = std::unique(allowed_aas_names.begin(), allowed_aas_names.end());
	    allowed_aas_names.resize( std::distance(allowed_aas_names.begin(),it) );
	    std::sort(aromatic_and_his.begin(), aromatic_and_his.end());
			std::set_intersection(allowed_aas_names.begin(),allowed_aas_names.end(),aromatic_and_his.begin(),aromatic_and_his.end(),back_inserter(intersect));
			TR<<"size of intersect is"<<intersect.size()<<std::endl;
			if ((allowed_aas_names.size()==1) && (pose.residue(i).name3()!="GLY")&&(pose.residue(i).name3()==source_pose.residue(i+res_diff).name3())){//Apply when we have strict conservation
				std::vector <core::Size> chi_vec;
				chi_vec.clear();
				for (core::Size chi=1;chi<source_pose.residue(i + res_diff).nchi();chi++){
					core::Real const chi_curr=source_pose.chi(chi,i + res_diff);
					pose.set_chi(chi,i,chi_curr);
					utility::vector1< core::Size > const chiAtoms=pose.residue(i).chi_atoms(chi);//get chi atoms from chi angle so we can apply coordiante constraints
					for (core::Size chiAtom=1;chiAtom<=4; chiAtom++){
						core::scoring::func::FuncOP coor_cont_fun = new core::scoring::func::HarmonicFunc(0.0, 1);
						TR<<"Applying constraints to chi_atom:"<<chiAtoms[chiAtom]<<pose.residue(i).atom_name(chiAtoms[chiAtom])<<",of residue "<<pose.residue(i).name3()<<i<<std::endl;
						cst.push_back(new core::scoring::constraints::CoordinateConstraint(core::id::AtomID(chiAtoms[chiAtom], i),anchor_atom, source_pose.residue(i + res_diff).xyz(chiAtoms[chiAtom]), coor_cont_fun));
						pose.add_constraints(cst);
					}//for chiAtom
					}//for chi
				using namespace std;
				string String = static_cast<ostringstream*>( &(ostringstream() << i) )->str();
				if (debug_)
					pose.dump_pdb(mover_name_+"_after_chi_change"+String+".pdb");

			}
			else if ((intersect.size()==allowed_aas_names.size())&&(std::find(aromatic_and_his.begin(), aromatic_and_his.end(),source_pose.residue(i + res_diff).name1())!=aromatic_and_his.end())){//if same size then all allowed identities are in the aromatic vector
				std::vector <core::Size> chi_vec;
				chi_vec.clear();
				for (core::Size chi=1;chi<source_pose.residue(i + res_diff).nchi();chi++){
					core::Real const chi_curr=source_pose.chi(chi,i + res_diff);
					pose.set_chi(chi,i,chi_curr);
					utility::vector1< core::Size > const chiAtoms=pose.residue(i).chi_atoms(chi);//get chi atoms from chi angle so we can apply coordiante constraints
					for (core::Size chiAtom=1;chiAtom<=4; chiAtom++){
						core::scoring::func::FuncOP coor_cont_fun = new core::scoring::func::HarmonicFunc(0.0, 1);
						cst.push_back(new core::scoring::constraints::CoordinateConstraint(core::id::AtomID(chiAtoms[chiAtom], i),anchor_atom, source_pose.residue(i + res_diff).xyz(chiAtoms[chiAtom]), coor_cont_fun));
						TR<<"Applying constraints to chi_atom:"<<chiAtoms[chiAtom]<<pose.residue(i).atom_name(chiAtoms[chiAtom])<<",of residue "<<pose.residue(i).name3()<<i<<std::endl;
						pose.add_constraints(cst);
					}//for chiAtom
				}//for chi
				using namespace std;
				string String = static_cast<ostringstream*>( &(ostringstream() << i) )->str();
				if (debug_)
				pose.dump_pdb(mover_name_+"after_chi_change"+String+".pdb");
			}
			continue;
		}
		core::scoring::func::FuncOP coor_cont_fun = new core::scoring::func::HarmonicFunc(0.0, 1);
		cst.push_back(
				new core::scoring::constraints::CoordinateConstraint(core::id::AtomID(pose.residue(i).atom_index(atom_type), i),
						anchor_atom, source_pose.residue(i + res_diff).atom(atom_type).xyz(), coor_cont_fun));
//Print xyz coor of current pose CA atoms vs. source pose
		TR_constraints << i << pose.aa(i) << " " << pose.residue(i).atom(atom_type).xyz()[0] << "," << pose.residue(i).atom(atom_type).xyz()[1]
				<< "," << pose.residue(i).atom(atom_type).xyz()[2] << " / " << i + res_diff << source_pose.aa(i + res_diff) << " "
				<< source_pose.residue(i + res_diff).atom(atom_type).xyz()[0] << ","	<< source_pose.residue(i + res_diff).atom(atom_type).xyz()[1] << "," << source_pose.residue(i + res_diff).atom(atom_type).xyz()[2]
				<< std::endl;
		pose.add_constraints(cst);
	}//for
	//scorefxn()->show(pose);
}//func

void Splice::add_dihedral_constraints(core::pose::Pose & pose, core::pose::Pose const & source_pose, core::Size start_res_on_pose, core::Size end_res_on_pose) {

	int residue_diff( 0 );
	if (protein_family_=="antibodies"){/*if using "segment" and "protein_family" tags then we are using Rosetta db PSSMs*/
	//use disulfide to calculate residue difference (absolute residue difference) between source_pose and pose)
		core::Size nearest_disulfide_on_pose(protocols::rosetta_scripts::find_nearest_disulfide(pose,start_res_on_pose));
		TR_constraints<<"Nearest disulfie on pose is:"<<nearest_disulfide_on_pose<<std::endl;
		core::Size nearest_disulfide_on_source(protocols::rosetta_scripts::find_nearest_res(source_pose,pose,nearest_disulfide_on_pose,1));
//	core::Size from = protocols::rosetta_scripts::find_nearest_res(source_pose, pose, start_res_on_pose, 1/*chain*/); //The following for loop itterates over the source pose residues
		residue_diff = nearest_disulfide_on_source - nearest_disulfide_on_pose;
		TR_constraints<<"Residue diff is:"<<residue_diff<<std::endl;
		TR_constraints << "closest disulfide on source is: " << nearest_disulfide_on_source<<source_pose.residue(nearest_disulfide_on_source).name1()<< std::endl;
		TR_constraints << "Residue difference :" << residue_diff << std::endl;
	}
		//inorder to compare the angles we keep track of the corresponding residues in the template pose
	TR_constraints << "Applying dihedral constraints to pose, Pose/Source PDB:" << std::endl;
	for (core::Size i = start_res_on_pose + residue_diff; i <= end_res_on_pose + residue_diff; ++i) {
		core::scoring::constraints::ConstraintOPs csts; //will hold dihedral constraints
//Set up constraints for the phi angle
		core::id::AtomID phi_resi_n(source_pose.residue_type(i).atom_index("N"), i);
		numeric::xyzVector<core::Real> xyz_Ni = source_pose.residue(i).atom("N").xyz();

		core::id::AtomID phi_resj_c(source_pose.residue_type(i - 1).atom_index("C"), i - 1);
		numeric::xyzVector<core::Real> xyz_Cj = source_pose.residue(i - 1).atom("C").xyz();

		core::id::AtomID phi_resi_co(source_pose.residue_type(i).atom_index("C"), i);
		numeric::xyzVector<core::Real> xyz_Ci = source_pose.residue(i).atom("C").xyz();

		core::id::AtomID phi_resi_ca(source_pose.residue_type(i).atom_index("CA"), i);
		numeric::xyzVector<core::Real> xyz_Cai = source_pose.residue(i).atom("CA").xyz();

		TR_constraints << "Phi: " << i - residue_diff << pose.aa(i - residue_diff) << ":" << pose.phi(i - residue_diff)
				<< " / " << i << source_pose.aa(i) << ":" << numeric::dihedral_degrees(xyz_Cj, xyz_Ni, xyz_Cai, xyz_Ci)
				<< std::endl;
		core::scoring::func::FuncOP di_const_func_phi = new core::scoring::func::CircularHarmonicFunc(
				(source_pose.phi(i) * numeric::constants::d::pi_2) / 360, 1);
		csts.push_back(
				new core::scoring::constraints::DihedralConstraint(phi_resj_c, phi_resi_n, phi_resi_ca, phi_resi_co,
						di_const_func_phi));
//for debuggin comment this out

//Set up constraints for the psi angle
		core::id::AtomID psi_resi_n(source_pose.residue_type(i).atom_index("N"), i);
		xyz_Ni = source_pose.residue(i).atom("N").xyz();

		core::id::AtomID psi_resj_n(source_pose.residue_type(i + 1).atom_index("N"), i + 1);
		numeric::xyzVector<core::Real> xyz_Nj = source_pose.residue(i + 1).atom("N").xyz();

		core::id::AtomID psi_resi_co(source_pose.residue_type(i).atom_index("C"), i);
		xyz_Ci = source_pose.residue(i).atom("C").xyz();

		core::id::AtomID psi_resi_ca(source_pose.residue_type(i).atom_index("CA"), i);
		xyz_Cai = source_pose.residue(i).atom("CA").xyz();

//for each residue the ideal angle is taken from the source pdb
		core::scoring::func::FuncOP di_const_func_psi = new core::scoring::func::CircularHarmonicFunc(
				(source_pose.psi(i) * numeric::constants::d::pi_2) / 360, 1);
		csts.push_back(
				new core::scoring::constraints::DihedralConstraint(psi_resi_n, psi_resi_ca, psi_resi_co, psi_resj_n,
						di_const_func_psi));
		TR_constraints << "Psi: " << i - residue_diff << pose.aa(i - residue_diff) << ":" << pose.psi(i - residue_diff)
				<< " / " << i << source_pose.aa(i) << ":" << numeric::dihedral_degrees(xyz_Ni, xyz_Cai, xyz_Ci, xyz_Nj)
				<< std::endl;
//Set up constraints for the omega angle
		core::id::AtomID omega_resj_n(source_pose.residue_type(i + 1).atom_index("N"), i + 1);
		xyz_Ni = source_pose.residue(i + 1).atom("N").xyz();

		core::id::AtomID omega_resi_ca(source_pose.residue_type(i).atom_index("CA"), i);
		xyz_Cai = source_pose.residue(i).atom("CA").xyz();

		core::id::AtomID omega_resi_co(source_pose.residue_type(i).atom_index("C"), i);
		xyz_Ci = source_pose.residue(i).atom("C").xyz();

		core::id::AtomID omega_resj_ca(source_pose.residue_type(i + 1).atom_index("CA"), i + 1);
		numeric::xyzVector<core::Real> xyz_Caj = source_pose.residue(i + 1).atom("CA").xyz();
		TR_constraints << "omega: " << i - residue_diff << pose.aa(i - residue_diff) << ":" << pose.omega(i - residue_diff)
				<< " / " << i << source_pose.aa(i) << ":" << numeric::dihedral_degrees(xyz_Cai, xyz_Ci, xyz_Nj, xyz_Caj)
				<< std::endl;
//for each residue the ideal angle is taken from the "donor" pdb
		core::scoring::func::FuncOP di_const_func_omega = new core::scoring::func::CircularHarmonicFunc(
				(source_pose.omega(i) * numeric::constants::d::pi_2) / 360, 1);
		csts.push_back(
				new core::scoring::constraints::DihedralConstraint(omega_resi_ca, omega_resi_co, omega_resj_n, omega_resj_ca,
						di_const_func_omega));

		pose.add_constraints(csts);
	}
	core::Real const score_weight(scorefxn()->get_weight(core::scoring::dihedral_constraint));
	TR_constraints << "dihedral_constraint weight is set to " << score_weight << std::endl;
	//scorefxn()->show(pose);
	//pose.dump_pdb("at_end_of_dihedral_const.pdb");
}

core::Real Splice::profile_weight_away_from_interface() const {
	return profile_weight_away_from_interface_;
}

void Splice::profile_weight_away_from_interface(core::Real const p) {
	profile_weight_away_from_interface_ = p;
}

/// @brief This is helper function that cuts out a file name, removing the extension and the path
std::string Splice::parse_pdb_code(std::string pdb_file_name) {
	const size_t last_slash_idx = pdb_file_name.find_last_of("\\/");
	if (std::string::npos != last_slash_idx) {
		pdb_file_name.erase(0, last_slash_idx + 1);
	}
	// Remove extension if present.
	const size_t period_idx = pdb_file_name.rfind('.');
	if (std::string::npos != period_idx) {
		pdb_file_name.erase(period_idx);
	}
	return pdb_file_name;

}
void Splice::rb_adjust_template(core::pose::Pose const & pose) const {
	if (!rb_sensitive())
		return;

	RBOutMover rbo;
	//	template_pose_->dump_pdb( "pre_jump_template.pdb" );
	core::pose::Pose copy_pose(pose); /// I don't want the fold tree to change on pose...
	core::pose::Pose chainA = *pose.split_by_chain(1);
	core::kinematics::Jump const pose_jump = rbo.get_disulf_jump(copy_pose, chainA);

	RBInMover rbi;
	rbi.set_fold_tree(*template_pose_);
	template_pose_->set_jump(1, pose_jump);
	//	template_pose_->dump_pdb( "post_jump_template.pdb" );
	//	pose.dump_pdb( "pose.pdb" );
	//	runtime_assert( 0 );
}
/// @brief Since we want to minimally perturb the actice site conformation (whether it be binding or catalytic) the cut site should be placed furthest away.
/// For each protein family we will probaly need specail definitions unless we find a more genral way. For antibodies I go from the conserved Trp res at the base of CDR1
/// and then continue going to the C-ter until I find the distal loops.
core::Size Splice::find_non_active_site_cut_site(core::pose::Pose const & pose) {
	using namespace core::sequence;
	SequenceProfileOP profile;
	if (protein_family_!="antibodies")
		utility_exit_with_message("Currently \"find_non_active_site_cut_site\" is only valid when using \"antibodies\" option with \"protein_family\" tag\n");
	TR<<"Placing cut away from functional site"<<std::endl;
	std::string const source_pdb_name(parse_pdb_code(pose.pdb_info()->name()));
	//use pssm to find conserved trp
	if (splice_segments_[ segment_type_ ]->pdb_profile( source_pdb_name )==0)
					utility_exit_with_message(" could not find the source pdb name: "+ source_pdb_name + ", in pdb_profile_match file."+segment_type_+" or PSSM file is missing\n");

	else
		profile=splice_segments_[ segment_type_ ]->pdb_profile( source_pdb_name );

	core::Size aapos=1;
	for (core::Size pos /*go over profile ids*/= 1; pos <= profile->size(); ++pos) {
		TR_pssm << pose.residue(aapos).name1() << aapos << ","<< profile->prof_row(pos) << std::endl;
		if ((profile->prof_row(pos)[19]) > 9) //Conserved CDR1 stem will have a pssm score over 8
			break;
		++aapos;//found conserved trp
	} //for
	core::scoring::dssp::Dssp dssp(pose);
							dssp.dssp_reduced();// switch to simplified H E L notation
	for (core::Size pos /*go over profile ids*/= aapos; pos <= pose.total_residue(); ++pos) {
							if (dssp.get_dssp_secstruct(pos) == 'L' ){ // allow site for cutting if it's either in a loop or if cutting secondary structure is allowed
								while (pose.residue(pos).name3()=="PRO"||pose.residue(pos+1).name3()=="PRO")
										pos=pos+1;//Can't place cut site after proline
								TR<<"Found cut site at:"<<pos<<pose.residue(pos).name1()<<std::endl;
								return pos+2;
							}
	}

	return 0;
}




} //splice
} //devel
