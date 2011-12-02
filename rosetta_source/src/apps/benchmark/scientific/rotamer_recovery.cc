// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file apps/benchmark/scientific/rotamer_recovery.cc
/// @brief this determines the percent of rotamers correctly predictied during a repack
/// @author Matthew O'Meara (mattjomeara@gmail.com)



// Unit Headers
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/rotamer_recovery/RotamerRecoveryMover.hh>
#include <protocols/rotamer_recovery/RotamerRecovery.hh>
#include <protocols/rotamer_recovery/RotamerRecoveryFactory.hh>


// Project Headers
#include <devel/init.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>
#include <basic/options/option_macros.hh>
#include <basic/Tracer.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

// Utility Headers
#include <utility/excn/EXCN_Base.hh>
#include <utility/exit.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>

// c++ Headers
#include <fstream>

using std::endl;
using std::ofstream;
using std::string;
using basic::Tracer;
using core::pose::PoseOP;
using core::import_pose::pose_from_pdb;
using core::pack::task::TaskFactory;
using core::pack::task::TaskFactoryOP;
using core::pack::task::operation::InitializeFromCommandline;
using core::pack::task::operation::ReadResfile;
using core::pack::task::operation::RestrictToRepacking;
using core::scoring::getScoreFunction;
using core::scoring::ScoreFunctionOP;
using devel::init;
using protocols::rotamer_recovery::RotamerRecoveryOP;
using protocols::rotamer_recovery::RotamerRecoveryFactory;
using protocols::rotamer_recovery::RotamerRecoveryMover;
using protocols::rotamer_recovery::RotamerRecoveryMoverOP;
using protocols::jd2::JobDistributor;
using utility::excn::EXCN_Base;

static Tracer TR("apps.benchmark.scientific.rotamer_recovery");

OPT_1GRP_KEY( String, rotamer_recovery, protocol )
OPT_1GRP_KEY( String, rotamer_recovery, comparer )
OPT_1GRP_KEY( String, rotamer_recovery, reporter )
//OPT_2GRP_KEY( File, rotamer_recovery, native_vs_decoy )
//OPT_2GRP_KEY( File, out, rotamer_recovery, method )
OPT_2GRP_KEY( File, out, file, rotamer_recovery )

void
register_options() {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	OPT( in::file::s );
	OPT( in::file::l );
	OPT( in::file::silent );
	OPT( in::file::native );
	OPT( packing::use_input_sc );
	OPT( packing::resfile );
	NEW_OPT( rotamer_recovery::protocol, "Rotamer Recovery Protocol component.", "RRProtocolMinPack");
	NEW_OPT( rotamer_recovery::comparer, "Rotamer Recovery Comparer component.", "RRComparerAutomorphicRMSD");
	NEW_OPT( rotamer_recovery::reporter, "Rotamer Recovery Reporter component.", "RRReporterHuman");
	//	NEW_OPT( rotamer_recovery::native_vs_decoy, "Table of describing which native structures the decoys are modeling.  The first column is native pdb filename and the second column is the decoy pdb filename.", "");
	NEW_OPT( out::file::rotamer_recovery, "Output File  Name for reporters that write their results to a file.", "");

}

int
main( int argc, char * argv [] )
{
  register_options();

  init(argc, argv);

  using namespace basic::options;
  using namespace basic::options::OptionKeys;
  using namespace basic::options::OptionKeys::out::file;

	string const & protocol( option[ rotamer_recovery::protocol ].value() );
	string const & reporter( option[ rotamer_recovery::reporter ].value() );
	string const & output_fname( option[ out::file::rotamer_recovery ].value() );
	string const & comparer( option[ rotamer_recovery::comparer ].value() );
	ScoreFunctionOP scfxn( getScoreFunction() );
	TaskFactoryOP task_factory( new TaskFactory );
	task_factory->push_back( new InitializeFromCommandline );
	if(option.has(OptionKeys::packing::resfile) && option[OptionKeys::packing::resfile].user()){
		task_factory->push_back( new ReadResfile );
	}
	task_factory->push_back( new RestrictToRepacking );

  RotamerRecoveryOP rotamer_recovery(
		RotamerRecoveryFactory::get_instance()->get_rotamer_recovery(
			protocol, comparer, reporter));

	RotamerRecoveryMoverOP rotamer_recovery_mover(
		new RotamerRecoveryMover(rotamer_recovery, scfxn, task_factory));


  try{
    JobDistributor::get_instance()->go( rotamer_recovery_mover );

		rotamer_recovery_mover->show();

		// if requsted write output to disk
		if(option[ out::file::rotamer_recovery].user()){
			ofstream fout;
			fout.open( output_fname.c_str(), std::ios::out );
			if( !fout.is_open() ){
				TR << "Unable to open output file '" << output_fname << "'." << endl;
				utility_exit();
			}
			rotamer_recovery_mover->show( fout );
			fout.close();
		}
  } catch (EXCN_Base & excn ) {
    TR.Error << "Exception: " << endl;
    excn.show( TR.Error );

    TR << "Exception: " << endl;
    excn.show( TR ); //so its also seen in a >LOG file
  }

  return 0;
}





