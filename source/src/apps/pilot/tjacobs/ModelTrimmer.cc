// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet;
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file ModelTrimmer.cc
///
/// @brief Silly pilot app that reads a model file, removes models that fail certain criteria, and then dumps it back out.
/// This is hardcoded in lots of terrible ways. Ideally, this concept should be generalized and run strategically during the
/// actual generation of a SEWING construct, but that's for another day/PhD.
///
/// @author Tim Jacobs

//Package headers
#include <devel/init.hh>
#include <protocols/sewing/hashing/Hasher.hh>
#include <protocols/sewing/conformation/Model.hh>
#include <protocols/sewing/conformation/AssemblyFactory.hh>
#include <protocols/sewing/conformation/Assembly.hh>
#include <protocols/sewing/util/io.hh>

//Protocol headers
#include <core/types.hh>
#include <core/id/AtomID.hh>
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>
#include <protocols/features/SmotifFeatures.hh>

//Utility headers
#include <utility/io/izstream.hh>
#include <utility/string_util.hh>
#include <utility/vector1.hh>
#include <utility/file/FileName.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/sewing.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

//C++ headers
#include <map>
#include <set>
#include <iterator>


static basic::Tracer TR("ModelTrimmer");

//std::string
//check_parallel(protocols::sewing::SewSegment const & seg1, protocols::sewing::SewSegment const & seg2) {
//
// utility::vector1< numeric::xyzVector<core::Real> > ss1_coords;
// for(core::Size i=1; i<=seg1.residues_.size(); ++i) {
//  for(core::Size j=1; j<=seg1.residues_[i].basis_atoms_.size(); ++j) {
//   ss1_coords.push_back(seg1.residues_[i].basis_atoms_[j].coords_);
//  }
// }
//
// utility::vector1< numeric::xyzVector<core::Real> > ss2_coords;
// for(core::Size i=1; i<=seg2.residues_.size(); ++i) {
//  for(core::Size j=1; j<=seg2.residues_[i].basis_atoms_.size(); ++j) {
//   ss2_coords.push_back(seg2.residues_[i].basis_atoms_[j].coords_);
//  }
// }
//
// protocols::features::SmotifFeatures sm_features;
// core::Real distance, hoist, packing, meridian;
// sm_features.calculate_angles(ss1_coords, ss2_coords, distance, hoist, packing, meridian);
//
// std::stringstream values;
// values << int(distance) << "_" << int(hoist) << "_" << int(packing) << "_" << int(meridian);
// return values.str();
//}

int
main( int argc, char * argv [] ) {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace protocols::sewing;

	// initialize core and read options
	devel::init(argc, argv);

	// Create a comments stream and write the date to it
	std::stringstream comments;
	time_t t = time(0);   // get time now
	struct tm * now = localtime( & t );
	comments << "#Model file created on " << (now->tm_year + 1900) << '-'
		<< (now->tm_mon + 1) << '-'
		<<  now->tm_mday
		<< std::endl;

	//Check for model file (either for reading or writing)
	if ( !option[sewing::model_file_name].user() || !option[sewing::new_model_file_name].user() ) {
		std::stringstream err;
		err << "You must provide both an input and output model file name to the ModelTrimmer using " <<
			"the -model_file_name flag and -new_model_file_name flags respectively";
		utility_exit_with_message(err.str());
	}
	std::map< int, Model > models;
	std::string model_filename = option[sewing::model_file_name];
	std::string new_model_filename = option[sewing::new_model_file_name];

	comments << "#This model file contains models from parent file " << model_filename << std::endl;

	std::string remove_any_dssp = option[sewing::remove_any_dssp];
	for ( core::Size i=0; i<remove_any_dssp.length(); ++i ) {
		if ( remove_any_dssp[i] != 'H' && remove_any_dssp[i] != 'E' && remove_any_dssp[i] != 'L' ) {
			std::stringstream err;
			err << "invalid DSSP character " << remove_any_dssp[i] << ". Please use only H(helix),E(strand), or L(loop).";
			utility_exit_with_message(err.str());
		}
	}
	if ( remove_any_dssp.length() > 0 ) {
		comments << "#All models with a segment containing dssp code(s) " << remove_any_dssp << " have been removed" << std::endl;
	}

	std::string remove_all_dssp = option[sewing::remove_all_dssp];
	for ( core::Size i=0; i<remove_all_dssp.length(); ++i ) {
		if ( remove_all_dssp[i] != 'H' && remove_all_dssp[i] != 'E' && remove_all_dssp[i] != 'L' ) {
			std::stringstream err;
			err << "invalid DSSP character " << remove_any_dssp[i] << ". Please use only H(helix),E(strand), or L(loop). ";
			utility_exit_with_message(err.str());
		}
	}
	if ( remove_all_dssp.length() > 0 ) {
		comments << "#All models composed of entirely loops and dssp code(s) " << remove_all_dssp << " have been removed" << std::endl;
	}


	comments << "#Any model with helical (H) segment(s) fewer than " << option[sewing::min_helix_length]
		<< " residues or greater than " << option[sewing::max_helix_length] << " residues have been removed" << std::endl;
	comments << "#Any model with strand (E) segment(s) fewer than " << option[sewing::min_strand_length]
		<< " residues or greater than " << option[sewing::max_strand_length] << " residues have been removed" << std::endl;
	comments << "#Any model with loop (L) segment(s) fewer than " << option[sewing::min_loop_length]
		<< " residues or greater than " << option[sewing::max_loop_length] << " residues have been removed" << std::endl;

	if ( option[sewing::model_should_have_at_least_one_E_at_terminal_segment] ) {
		comments << "#Only models that have at least 1 E segment at terminal segment are remained " << std::endl;
	}

	if ( option[sewing::model_should_have_at_least_one_E] ) {
		comments << "#Only models that have at least 1 E segment are remained " << std::endl;
	}

	if ( option[sewing::leave_models_with_E_terminal_ss] ) {
		comments << "#All models that lack 'two terminal beta-strands' have been removed " << std::endl;
		TR << "All models that lack 'two terminal beta-strands' have been removed" << std::endl;
	}

	if ( option[sewing::leave_parallel_way_H_bonded_models_by_terminal_strands_only] ) {
		comments << "#All models that lack 'terminal strands that are parallel way H-bonded with their backbone atoms' have been removed " << std::endl;
	}

	if ( option[sewing::leave_antiparallel_way_H_bonded_models_by_terminal_strands_only] ) {
		comments << "#All models that lack 'terminal strands that are anti-parallel way H-bonded with their backbone atoms' have been removed " << std::endl;
	}

	models = read_model_file(model_filename);
	std::map< int, Model >::iterator it = models.begin();
	std::map< int, Model >::iterator it_end = models.end();
	while ( it != it_end ) {
		Model model = it->second;
		if ( TR.Debug.visible() ) {
			TR.Debug << "model.model_id_: " << model.model_id_ << std::endl;
		}

		bool erase = false;

		//If a given model has *any* segments with this DSSP, remove it
		for ( core::Size i=0; i<remove_any_dssp.length(); ++i ) {
			for ( core::Size j=1; j<=model.segments_.size(); ++j ) {
				if ( model.segments_[j].dssp_ == remove_any_dssp[i] ) {
					if ( TR.Debug.visible() ) {
						TR.Debug << "i: " << i << std::endl;
						TR.Debug << "remove_any_dssp[i]: " << remove_any_dssp[i] << std::endl;
						TR.Debug << "j: " << j << std::endl;
						TR.Debug << "model.segments_[j].dssp_: " << model.segments_[j].dssp_ << std::endl;
					}
					erase = true;
				}
			}
		}

		//If a given model has *all* non-loop segments with this DSSP, remove it
		for ( core::Size i=0; (!erase) && i<remove_all_dssp.length(); ++i ) {
			bool all=true;
			for ( core::Size j=1; j<=model.segments_.size(); ++j ) {
				if ( model.segments_[j].dssp_ != remove_all_dssp[i] && model.segments_[j].dssp_ != 'L' ) {
					all=false;
				}
			}
			if ( all ) {
				if ( TR.Debug.visible() ) {
					TR.Debug << "remove_all_dssp[i] && model.segments_[j].dssp_ != 'L' related" << std::endl;
				}
				erase = true;
			}
		}


		if ( (!erase) && (option[sewing::model_should_have_at_least_one_E_at_terminal_segment]) ) {
			if ( (model.segments_[1].dssp_ != 'E') && (model.segments_[model.segments_.size()].dssp_ != 'E') ) {
				erase=true;
			}
		}

		if ( (!erase) && (option[sewing::model_should_have_at_least_one_E]) ) {
			bool model_have_E = false;
			for ( core::Size i=1; i<=model.segments_.size(); ++i ) {
				if ( model.segments_[i].dssp_ == 'E' ) {
					model_have_E = true;
					break; // essential for faster performance
				}
			}
			if ( !model_have_E ) {
				erase=true;
			}
		}

		for ( core::Size i=1; (!erase) && i<=model.segments_.size(); ++i ) {
			if ( model.segments_[i].dssp_ == 'H' &&
					((int)model.segments_[i].residues_.size() > option[sewing::max_helix_length] ||
					(int)model.segments_[i].residues_.size() < option[sewing::min_helix_length] ) ) {
				if ( TR.Debug.visible() ) {
					TR.Debug << "model.segments_[i].dssp_ == 'H' " << std::endl;
				}
				erase=true;
			} else if ( model.segments_[i].dssp_ == 'E' &&
					((int)model.segments_[i].residues_.size() > option[sewing::max_strand_length] ||
					(int)model.segments_[i].residues_.size() < option[sewing::min_strand_length] ) ) {
				if ( TR.Debug.visible() ) {
					TR.Debug << "model.segments_[i].dssp_ == 'E' " << std::endl;
				}
				erase=true;
			} else if ( model.segments_[i].dssp_ == 'L' &&
					((int)model.segments_[i].residues_.size() > option[sewing::max_loop_length] ||
					(int)model.segments_[i].residues_.size() < option[sewing::min_loop_length] ) ) {
				if ( TR.Debug.visible() ) {
					TR.Debug << "model.segments_[i].dssp_ == 'L' " << std::endl;
				}
				erase=true;
			}
		}//for(core::Size i=1; (!erase) && i<=model.segments_.size(); ++i) {

		//If a given model lacks appropriate DSSP in its two terminal secondary structures, remove it
		if ( (!erase) && option[sewing::leave_models_with_E_terminal_ss] ) {
			if ( model.segments_[1].dssp_ != 'E' || model.segments_[model.segments_.size()].dssp_ != 'E' ) {
				erase=true;
			}
		}

		// erase=true if first and last segments are not H-bonded by their backbones
		if ( (!erase) && (option[sewing::leave_parallel_way_H_bonded_models_by_terminal_strands_only] ||
				option[sewing::leave_antiparallel_way_H_bonded_models_by_terminal_strands_only]) ) {

			if ( option[sewing::leave_antiparallel_way_H_bonded_models_by_terminal_strands_only] ) {
				std::string model_is_H_bonded_by_terminal_strands = see_whether_model_is_H_bonded_by_terminal_strands(model, "antiparallel");
				//TR << "model_is_H_bonded_by_terminal_strands: " << model_is_H_bonded_by_terminal_strands << std::endl;
				if ( model_is_H_bonded_by_terminal_strands!="antiparallel" ) {
					erase=true;
				}
			}

			if ( option[sewing::leave_parallel_way_H_bonded_models_by_terminal_strands_only] ) {
				std::string model_is_H_bonded_by_terminal_strands = see_whether_model_is_H_bonded_by_terminal_strands(model, "parallel");
				//TR << "model_is_H_bonded_by_terminal_strands: " << model_is_H_bonded_by_terminal_strands << std::endl;
				if ( model_is_H_bonded_by_terminal_strands!="parallel" ) {
					erase=true;
				}
			}
		}

		if ( TR.Debug.visible() ) {
			TR.Debug << "erase bool: " << erase << std::endl;
			TR.Debug << std::endl;
		}

		if ( !erase ) {
			//   //////TESTING///////
			//   ++counter;
			//   std::cout << "Not erasing" << std::endl;
			//   AssemblyOP assembly = AssemblyFactory::create_assembly("continuous");
			//   assembly->add_model(0, it->second, false);
			//   core::pose::Pose model_pose = assembly->to_pose(core::chemical::FA_STANDARD, false);
			//   std::string values = check_parallel(model.segments_[1], model.segments_[2]);
			//   model_pose.dump_pdb("model_"+utility::to_string(it->first)+"_"+values+".pdb");
			//   if(counter>50) { std::exit(0); }
			//   //////TESTING///////

			++it;
		} else {
			models.erase(it++);
		}
	}//while(it != it_end) {
	write_model_file(comments.str(), models, new_model_filename);
}