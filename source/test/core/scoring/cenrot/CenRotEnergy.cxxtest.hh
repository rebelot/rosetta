// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   test/core/scoring/methods/RamachandranEnergy.cxxtest.hh
/// @brief  test suite for core::scoring::RamachandranEnergy.cc
/// @author Andrew Leaver-Fay

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/util/deriv_funcs.hh>
#include <test/util/pose_funcs.hh>
#include <test/core/init_util.hh>

#include <platform/types.hh>

// Unit headers
#include <core/scoring/CenRotEnvPairPotential.hh>
#include <core/scoring/methods/CenPairEnergy.hh>
#include <core/scoring/methods/CenRotEnvEnergy.hh>
#include <core/scoring/methods/EnergyMethodOptions.hh>

// Numeric headers
#include <numeric/conversions.hh>


// Project headers
// AUTO-REMOVED #include <core/conformation/Residue.hh>
// AUTO-REMOVED #include <core/kinematics/MoveMap.hh>
// AUTO-REMOVED #include <core/optimization/MinimizerMap.hh>
#include <protocols/simple_moves/SwitchResidueTypeSetMover.hh>
#include <core/optimization/MinimizerOptions.hh>
#include <core/optimization/AtomTreeMinimizer.hh>

//Auto Headers
#include <utility/vector1.hh>


// --------------- Test Class --------------- //

// using declarations
using namespace core;
using namespace core::pose;
using namespace core::scoring;
using namespace core::scoring::methods;

class CenRotModelEnergyTests : public CxxTest::TestSuite {

public:


	// --------------- Fixtures --------------- //

	// Define a test fixture (some initial state that several tests share)
	// In CxxTest, setUp()/tearDown() are executed around each test case. If you need a fixture on the test
	// suite level, i.e. something that gets constructed once before all the tests in the test suite are run,
	// suites have to be dynamically created. See CxxTest sample directory for example.

	// Shared initialization goes here.
	void setUp() {
		using namespace core;
		core_init_with_additional_options( "-corrections:score:cenrot" );
	}

	// Shared finalization goes here.
	void tearDown() {}

	// env should be tested along with pair
	void test_cen_rot_pair_env_energy()
	{
		using namespace core;
		using namespace core::pose;
		using namespace core::scoring;
		using namespace core::scoring::methods;
		
		Pose pose = create_trpcage_ideal_pose();
		protocols::simple_moves::SwitchResidueTypeSetMover to_cenrot("centroid_rot");
		to_cenrot.apply(pose);
		ScoreFunction sfxn;
		sfxn.set_weight( cen_rot_pair, 1.0 );
		sfxn.set_weight( cen_rot_pair, 1.0 );
		sfxn.set_weight( cen_rot_pair_ang, 1.0 );
		Real start_score = sfxn(pose);
		//std::cout.precision(15);
		//std::cout << start_score << std::endl;
		TS_ASSERT_DELTA(start_score, -2.24141308160167, 1e-12);
	}

	void test_cen_rot_vdw()
	{
		using namespace core;
		using namespace core::pose;
		using namespace core::scoring;
		using namespace core::scoring::methods;
		
		Pose pose = create_trpcage_ideal_pose();
		protocols::simple_moves::SwitchResidueTypeSetMover to_cenrot("centroid_rot");
		to_cenrot.apply(pose);
		ScoreFunction sfxn;
		EnergyMethodOptions options(sfxn.energy_method_options());
		options.atom_vdw_atom_type_set_name("centroid_rot");
		sfxn.set_energy_method_options(options);
		sfxn.set_weight( vdw, 1.0 );
		Real start_score = sfxn(pose);
		//std::cout.precision(15);
		//std::cout << start_score << std::endl;
		TS_ASSERT_DELTA(start_score, 5.2352964257915, 1e-12);
	}

	void test_cen_rot_dun()
	{
		using namespace core;
		using namespace core::pose;
		using namespace core::scoring;
		using namespace core::scoring::methods;
		
		Pose pose = create_trpcage_ideal_pose();
		protocols::simple_moves::SwitchResidueTypeSetMover to_cenrot("centroid_rot");
		to_cenrot.apply(pose);
		ScoreFunction sfxn;
		sfxn.set_weight( cen_rot_dun, 1.0 );
		Real start_score = sfxn(pose);
		TS_ASSERT_DELTA(start_score, 26.4134299633991, 1e-12);
	}

	void test_cen_rot_repack()
	{
		//
	}

	void test_cen_rot_atomtree_min()
	{
		using namespace core;
		using namespace core::pose;
		using namespace core::scoring;
		using namespace core::scoring::methods;
		using namespace core::optimization;
	}

	void test_cen_rot_cart_min()
	{
		//
	}

};


