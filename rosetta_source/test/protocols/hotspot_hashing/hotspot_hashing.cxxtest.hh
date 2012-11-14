// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/hotspot_hashing/hotspot_hashing.cxxtest.hh
/// @brief Test suite for protocols/hotspot_hashing/*
/// @author Alex Ford (fordas@uw.edu)
//
// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>
#include <util/pose_funcs.hh>

// Project headers
#include <basic/Tracer.hh>
#include <basic/options/keys/sicdock.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/options/option_macros.hh>

//Auto Headers
#include <utility/vector1.hh>
#include <numeric/random/random.hh>
#include <numeric/xyz.functions.hh>

#include <protocols/hotspot_hashing/SearchPattern.hh>
#include <protocols/hotspot_hashing/StubGenerator.hh>
#include <protocols/hotspot_hashing/SurfaceSearchPattern.hh>
#include <protocols/hotspot_hashing/SICSearchPattern.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/kinematics/Stub.hh>

#include <core/pose/Pose.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/kinematics/Stub.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/pack/task/TaskFactory.hh>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

static basic::Tracer TR("protocols.hotspot_hashing.hotspot_hashing.cxxtest");

namespace 
{
	/*
	using core::Size;
	using core::Real;
	using core::pose::Pose;
	using core::conformation::ResidueOP;
	using core::conformation::ResidueFactory;
	using core::chemical::ChemicalManager;
	using core::chemical::ResidueType;
	*/
	using core::kinematics::Stub;

  class HotspotHashingTests : public CxxTest::TestSuite {

    public:
      void setUp() 
      {
        core_init();
      }

			void test_stubcentroid_transform()
			{
				using namespace protocols::hotspot_hashing;

        // Initialize residue representation
        core::conformation::ResidueOP residue;
				core::chemical::ResidueTypeSetCAP residue_set( core::chemical::ChemicalManager::get_instance()->residue_type_set( core::chemical::FA_STANDARD ) );
				
				Vector xunit = Vector(1, 0, 0);
				Vector yunit = Vector(0, 1, 0);
				Vector zunit = Vector(0, 0, 1);

				{
					core::chemical::ResidueType const & restype( residue_set->name_map( "ALA" ) );
					residue = core::conformation::ResidueFactory::create_residue( restype );

					Vector stub_centroid = StubGenerator::residueStubCentroid(residue);
					Stub centroid_frame = StubGenerator::residueStubCentroidFrame(residue);

					Vector ca_location = residue->xyz("CA");
					// Using alanine residue, so centroid of SC heavyatoms will be CB position
					Vector cb_location = residue->xyz("CB");
					Vector c_location = residue->xyz("C");

					TS_ASSERT_DELTA(cb_location, stub_centroid, 1e-3);
					TS_ASSERT_DELTA(ca_location, centroid_frame.v, 1e-3);

					Vector ca_applied = centroid_frame.global2local(ca_location);
					Vector cb_applied = centroid_frame.global2local(cb_location);
					TS_ASSERT_DELTA(ca_applied, Vector(0, 0, 0), 1e-3);
					TS_ASSERT_DELTA(cb_applied.normalize(), Vector(1, 0, 0), 1e-3);
				}

				{
					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");

					Vector ca_location = residue->xyz("CA");
					// Using alanine residue, so centroid of SC heavyatoms will be CB position
					Vector cb_vector = residue->xyz("CB");
					cb_vector = cb_vector.normalize();
					Vector c_location = residue->xyz("C");

					TS_ASSERT_DELTA(ca_location, Vector(0, 0, 0), 1e-3);

					TS_ASSERT_DELTA(cb_vector, Vector(1, 0, 0), 1e-3);

					TS_ASSERT_DELTA(c_location.dot(Vector(0, 0, 1)), 0, 1e-3);
				}

			}

      void test_placeresidueattransform() 
      {
				using namespace protocols::hotspot_hashing;
        // Initialize target pose
        core::pose::Pose targetPose = create_test_in_pdb_pose();


				Vector xunit = Vector(1, 0, 0);
				Vector yunit = Vector(0, 1, 0);
				Vector zunit = Vector(0, 0, 1);

				// Null transform
				{
					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");
					core::pose::Pose testPose(targetPose);
					Stub transform;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						TS_ASSERT_EQUALS(jumpindex, testPose.num_jump());
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						TS_ASSERT_EQUALS(jumpindex, targetPose.num_jump() + 1);
						TS_ASSERT_EQUALS(residueindex, targetPose.total_residue() + 1);

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						Vector ca_location = placedResidue.xyz("CA");
						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						Vector cb_vector = placedResidue.xyz("CB");
						cb_vector = cb_vector.normalize();
						Vector c_location = placedResidue.xyz("C");

						TS_ASSERT_DELTA(ca_location, Vector(0, 0, 0), 1e-3);

						TS_ASSERT_DELTA(cb_vector, Vector(1, 0, 0), 1e-3);

						TS_ASSERT_DELTA(c_location.dot(Vector(0, 0, 1)), 0, 1e-3);
				}

				//Translate alone
				{
					Vector translation(10, 5, 2.5);

					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");
					core::pose::Pose testPose(targetPose);

					Stub transform;
					transform.v = translation;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						TS_ASSERT_EQUALS(jumpindex, testPose.num_jump());
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						Vector ca_location = placedResidue.xyz("CA");
						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						Vector cb_vector = placedResidue.xyz("CB") - ca_location;
						cb_vector = cb_vector.normalize();

						TS_ASSERT_DELTA(ca_location, translation, 1e-3);

						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						TS_ASSERT_DELTA(cb_vector, Vector(1, 0, 0), 1e-3);

						TS_ASSERT_DELTA((placedResidue.xyz("C") - translation).dot(Vector(0, 0, 1)), 0, 1e-3);
				}

				//Rotate in xy plane, centroid moved to y axis, CA in xy plane
				{
					Matrix rotation = rotation_matrix(xunit.cross(yunit), angle_of(xunit, yunit));

					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");
					core::pose::Pose testPose(targetPose);
					Stub transform;
					transform.M = rotation;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						TS_ASSERT_EQUALS(jumpindex, testPose.num_jump());
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						TS_ASSERT_DELTA(placedResidue.xyz(placedResidue.atom_index("CA")), Vector(0, 0, 0), 1e-3);

						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						TS_ASSERT_DELTA((placedResidue.xyz("CB") + Vector(0, 0, 0)).normalize(), Vector(0, 1, 0), 1e-3);

						TS_ASSERT_DELTA((placedResidue.xyz("C")).dot(Vector(0, 0, 1)), 0, 1e-3);
				}

				//Rotate in yz plane, centroid unmoved, CA in xz plane 
				{
					Matrix rotation = numeric::rotation_matrix(yunit.cross(zunit), angle_of(yunit, zunit));

					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");
					core::pose::Pose testPose(targetPose);
					Stub transform;
					transform.M = rotation;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						TS_ASSERT_EQUALS(jumpindex, testPose.num_jump());
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						Vector ca_xyz = placedResidue.xyz("CA");
						TS_ASSERT_DELTA(ca_xyz, Vector(0, 0, 0), 1e-3);

						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						Vector cb_xyz = placedResidue.xyz("CB");
						TS_ASSERT_DELTA(cb_xyz.normalize(), Vector(1, 0, 0), 1e-3);

						Vector c_xyz = placedResidue.xyz("C");
						TS_ASSERT_DELTA(c_xyz.dot(Vector(0, 1, 0)), 0, 1e-3);
				}

				//Rotate in xz plane, centroid moved to z axis, CA in yz plane 
				{
					Matrix rotation = numeric::rotation_matrix(xunit.cross(zunit), angle_of(xunit, zunit));

					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");
					core::pose::Pose testPose(targetPose);
					Stub transform;
					transform.M = rotation;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						TS_ASSERT_EQUALS(jumpindex, testPose.num_jump());
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						Vector ca_xyz = placedResidue.xyz("CA");
						TS_ASSERT_DELTA(ca_xyz, Vector(0, 0, 0), 1e-3);

						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						Vector cb_xyz = placedResidue.xyz("CB");
						TS_ASSERT_DELTA(cb_xyz.normalize(), Vector(0, 0, 1), 1e-3);

						Vector c_xyz = placedResidue.xyz("C");
						TS_ASSERT_DELTA(c_xyz.dot(Vector(1, 0, 0)), 0, 1e-3);
				}

				//Translate and rotate
				{
					Vector translation(10, 5, 2.5);
					Matrix rotation = numeric::rotation_matrix(xunit.cross(zunit), angle_of(xunit, zunit));

					core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");
					core::pose::Pose testPose(targetPose);
					Stub transform;
					transform.v = translation;
					transform.M = rotation;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						TS_ASSERT_EQUALS(jumpindex, testPose.num_jump());
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						TS_ASSERT_DELTA(placedResidue.xyz(placedResidue.atom_index("CA")), translation, 1e-3);

						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						TS_ASSERT_DELTA((placedResidue.xyz("CB") - translation).normalize(), Vector(0, 0, 1), 1e-3);

						TS_ASSERT_DELTA((placedResidue.xyz("C") - translation).dot(Vector(1, 0, 0)), 0, 1e-3);
				}
			}

      void test_placeresidueattransform_blankpose() 
      {
				using namespace protocols::hotspot_hashing;
        // Initialize target pose
        core::pose::Pose targetPose;

        // Initialize residue representation
        core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");

				Vector xunit = Vector(1, 0, 0);
				Vector yunit = Vector(0, 1, 0);
				Vector zunit = Vector(0, 0, 1);

				// Null transform
				{
					core::pose::Pose testPose(targetPose);
					Stub transform;

					core::Size jumpindex;
					core::Size residueindex;

					StubGenerator::placeResidueAtTransform(
							testPose,
							residue,
							transform,
							jumpindex,
							residueindex);

						// Jump index will be zero for the new residue
						TS_ASSERT_EQUALS(jumpindex, 0);
						TS_ASSERT_EQUALS(residueindex, testPose.total_residue());

						TS_ASSERT_EQUALS(jumpindex, 0);
						TS_ASSERT_EQUALS(residueindex, targetPose.total_residue() + 1);

						core::conformation::Residue const &placedResidue = testPose.residue(residueindex);

						TS_ASSERT_EQUALS(placedResidue.type().name(), "ALA");

						Vector ca_location = placedResidue.xyz("CA");
						// Using alanine residue, so centroid of SC heavyatoms will be CB position
						Vector cb_vector = placedResidue.xyz("CB");
						cb_vector = cb_vector.normalize();
						Vector c_location = placedResidue.xyz("C");

						TS_ASSERT_DELTA(ca_location, Vector(0, 0, 0), 1e-3);

						TS_ASSERT_DELTA(cb_vector, Vector(1, 0, 0), 1e-3);

						TS_ASSERT_DELTA(c_location.dot(Vector(0, 0, 1)), 0, 1e-3);
				}

				//
			}

      void test_lsmsearchpattern() 
      {
				using namespace protocols::hotspot_hashing;

				core::Real angle_sampling = 90;
				core::Real translocation_sampling = 1;
				core::Real max_radius = 2;
				core::Real distance_sampling = 1;
				core::Real max_distance = 2;


				VectorPair lsm_zero(
							Vector(0, 0, 0),
							Vector(1, 1, 1));
				LSMSearchPattern zero_pattern(
						lsm_zero,
						angle_sampling,
						translocation_sampling,
						max_radius,
						distance_sampling,
						max_distance);

				utility::vector1<Stub> points_zero = zero_pattern.Searchpoints();
				TS_ASSERT_EQUALS(points_zero.size(), 4 /*angles*/ * 13 /*trans*/ * 3 /*dist*/);

				// Assert on location
				foreach(Stub transform, points_zero)
				{
					TS_ASSERT_LESS_THAN_EQUALS(lsm_zero.position.distance(transform.v), std::sqrt( max_radius * max_radius + max_distance * max_distance));
				}

				//LSM taken from test structure
				VectorPair lsm_test(
						Vector(64.856, 29.883, 42.865),
						Vector(-0.56293, 0.20554, -0.80054));
				LSMSearchPattern test_pattern(
						lsm_test,
						90,
						1,
						2,
						1,
						2);
				utility::vector1<Stub> points_test = test_pattern.Searchpoints();
				TS_ASSERT_EQUALS(points_test.size(), 4 /*angles*/ * 13 /*trans*/ * 3 /*dist*/);

				foreach(Stub transform, points_test)
				{
					TS_ASSERT_LESS_THAN_EQUALS(lsm_test.position.distance(transform.v), std::sqrt( max_radius * max_radius + max_distance * max_distance));
				}
			}
			 
			void place_and_assert_transform(core::pose::Pose & targetPose, core::conformation::ResidueOP residue, Stub transform)
			{
				using namespace protocols::hotspot_hashing;
				core::Size jumpindex;
				core::Size residueindex;

				StubGenerator::placeResidueAtTransform(
						targetPose,
						residue,
						transform,
						jumpindex,
						residueindex);

				core::conformation::ResidueCOP placed_residue(targetPose.residue(residueindex));

				Vector residue_CA_location = placed_residue->xyz(placed_residue->atom_index("CA"));
				TS_ASSERT_DELTA(residue_CA_location, transform.v, 1e-3);

				Vector stubcentroid = StubGenerator::residueStubCentroid(placed_residue);
				Vector CA_centroid_vector = (stubcentroid - residue_CA_location).normalize();
				Vector xunit_rotated = transform.M * Vector(1, 0, 0);

				TS_ASSERT_DELTA(CA_centroid_vector, xunit_rotated, 1e-3); 		
			}

      /*void _lsmsearchpattern_placement() 
			{
				using namespace protocols::hotspot_hashing;
        // Initialize target pose
        core::pose::Pose targetPose;
				core::import_pose::pose_from_pdb( targetPose, "protocols/hotspot_hashing/3ve0_IJ.pdb" );

        // Initialize residue representation
        core::conformation::ResidueOP residue;
				core::chemical::ResidueTypeSetCAP residue_set( core::chemical::ChemicalManager::get_instance()->residue_type_set( core::chemical::FA_STANDARD ) );
        core::chemical::ResidueType const & restype( residue_set->name_map( "ALA" ) );
        residue = core::conformation::ResidueFactory::create_residue( restype );

				//LSM taken from test structure
				VectorPair lsm_test(
						Vector(57.718,-16.524,29.748),
						Vector(0.16551,-0.93345,0.21081));

				LSMSearchPattern test_pattern(
						lsm_test,
						90,
						1,
						2,
						1,
						2);
				utility::vector1<Stub> points_test = test_pattern.Searchpoints();

				foreach(Stub transform, points_test)
				{
					core::pose::Pose testPose(targetPose);

					place_and_assert_transform(testPose, residue, transform);
				}
			}*/

			void test_surfacesearchpattern_generation()
			{
				using namespace protocols::hotspot_hashing;
				using core::kinematics::Stub;

        // Initialize target pose
        core::pose::Pose pose;

        core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");

				core::Size jumpindex;
				core::Size residueindex;
				StubGenerator::placeResidueAtTransform(pose, residue, core::kinematics::default_stub, jumpindex, residueindex);

				SurfaceSearchPattern searchpattern(pose, core::pack::task::TaskFactoryOP(NULL), 1);

				utility::vector1<VectorPair> source_surface_vectors = searchpattern.surface_vectors();
				utility::vector1<Stub> generated_stubs = searchpattern.Searchpoints();

				TS_ASSERT_EQUALS(generated_stubs.size(), source_surface_vectors.size());

				for(core::Size i = 1; i <= generated_stubs.size(); i++)
				{
					VectorPair surface_vector = source_surface_vectors[i];
					Stub generated_stub = generated_stubs[i];

					Vector surface_normal_in_stub = generated_stub.global2local(surface_vector.direction + surface_vector.position);
					Vector surface_location_in_stub = generated_stub.global2local(surface_vector.position);

					TS_ASSERT_DELTA(surface_normal_in_stub.normalize(), Vector(-1, 0, 0), 1e-3);
					TS_ASSERT_DELTA(surface_location_in_stub, Vector(0, 0, 0), 1e-3);
				}
			}

			void test_sicsearchpattern()
			{
				using namespace protocols::hotspot_hashing;
				using core::kinematics::Stub;

        // Initialize target pose
        core::pose::Pose residue_pose;

        // Initialize residue representation
        core::conformation::ResidueOP residue = StubGenerator::getStubByName("ALA");

				core::Size jumpindex;
				core::Size residueindex;
				StubGenerator::placeResidueOnPose(residue_pose, residue);

				residue_pose.dump_pdb("residue_pose.pdb");

				Stub slide_stub(
						Vector(0, 0, 0),
						Vector(1, 0, 0),
						Vector(0, 1, 0));

				SearchPatternOP slide_pattern(new ConstPattern(slide_stub));

				{
					core::pose::Pose pre_slide;

					core::conformation::ResidueOP pre_slide_residue = StubGenerator::getStubByName("ALA");
					StubGenerator::moveIntoStubFrame(pre_slide_residue, slide_stub);
					StubGenerator::placeResidueOnPose(pre_slide, pre_slide_residue);

					pre_slide.dump_pdb("pre_slide.pdb");
				}

				core::Real clash_distance = basic::options::option[basic::options::OptionKeys::sicdock::clash_dis]();

				SICPatternAtTransform sic_pattern(residue_pose, residue_pose, slide_pattern);

				utility::vector1<core::kinematics::Stub> searchpoints = sic_pattern.Searchpoints();

				TS_ASSERT_EQUALS(1, searchpoints.size());

				{
					core::pose::Pose post_slide;

					core::conformation::ResidueOP post_slide_residue = StubGenerator::getStubByName("ALA");
					StubGenerator::moveIntoStubFrame(post_slide_residue, searchpoints[1]);
					StubGenerator::placeResidueOnPose(post_slide, post_slide_residue);

					post_slide.dump_pdb("post_slide.pdb");
					// Expected distance between CB atoms is 2*clash_distance
					Vector start_ca_loc = slide_stub.local2global( residue->xyz("CB"));
					Vector start_cb_loc = slide_stub.local2global( residue->xyz("CB"));
					Vector contact_cb_loc = searchpoints[1].local2global( residue->xyz("CB"));
					Vector src_cb_loc = residue->xyz("CB");

					Vector src_to_contact_cb = contact_cb_loc - src_cb_loc;

					TS_ASSERT_DELTA(src_to_contact_cb, Vector(clash_distance, 0, 0), 1e-3);

					TS_ASSERT_DELTA(src_to_contact_cb.x(), clash_distance, 1e-3);
					TS_ASSERT_DELTA(src_to_contact_cb.y(), 0, 1e-3);
					TS_ASSERT_DELTA(src_to_contact_cb.z(), 0, 1e-3);
				}
			}
  };
}
