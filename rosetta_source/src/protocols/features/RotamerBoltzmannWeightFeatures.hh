// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/features/RotamerBoltzmannWeightFeatures.hh
/// @brief  report rotamer boltzmann weights to features Statistics Scientific Benchmark
/// @author Matthew O'Meara

#ifndef INCLUDED_protocols_features_RotamerBoltzmannWeightFeatures_hh
#define INCLUDED_protocols_features_RotamerBoltzmannWeightFeatures_hh

// Unit Headers
#include <protocols/features/FeaturesReporter.hh>
#include <protocols/features/RotamerBoltzmannWeightFeatures.fwd.hh>

//External
#include <boost/uuid/uuid.hpp>

// Project Headers
#include <core/types.hh>
// AUTO-REMOVED #include <core/pose/Pose.hh>
// AUTO-REMOVED #include <core/scoring/ScoreFunction.hh>
#include <protocols/filters/Filter.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <protocols/moves/DataMap.fwd.hh>
// AUTO-REMOVED #include <protocols/protein_interface_design/filters/RotamerBoltzmannWeight.hh>
// AUTO-REMOVED #include <utility/sql_database/DatabaseSessionManager.hh>
#include <utility/vector1.fwd.hh>

// C++ Headers
#include <string>

#include <core/scoring/ScoreFunction.fwd.hh>
#include <protocols/simple_filters/RotamerBoltzmannWeight.fwd.hh>
#include <utility/vector1.hh>


namespace protocols{
namespace features{

class RotamerBoltzmannWeightFeatures : public protocols::features::FeaturesReporter {
public:
	RotamerBoltzmannWeightFeatures();

	RotamerBoltzmannWeightFeatures(
		core::scoring::ScoreFunctionOP scfxn);


	RotamerBoltzmannWeightFeatures(RotamerBoltzmannWeightFeatures const & src);

	virtual ~RotamerBoltzmannWeightFeatures();

	///@brief return string with class name
	std::string
	type_name() const;

	///@brief generate the table schemas and write them to the database
	void
	write_schema_to_db(
		utility::sql_database::sessionOP db_session) const;

private:
	///@brief generate the rotamer_boltzmann_weight table schema
	void
	write_rotamer_boltzmann_weight_table_schema(
		utility::sql_database::sessionOP db_session) const;

public:
	///@brief return the set of features reporters that are required to
	///also already be extracted by the time this one is used.
	utility::vector1<std::string>
	features_reporter_dependencies() const;

	void
	parse_my_tag(
		utility::tag::TagPtr const tag,
		protocols::moves::DataMap & data,
		protocols::filters::Filters_map const & /*filters*/,
		protocols::moves::Movers_map const & /*movers*/,
		core::pose::Pose const & /*pose*/);

	///@brief collect all the feature data for the pose
	core::Size
	report_features(
		core::pose::Pose const & pose,
		utility::vector1< bool > const & relevant_residues,
		boost::uuids::uuid struct_id,
		utility::sql_database::sessionOP db_session);

private:
	protocols::simple_filters::RotamerBoltzmannWeightFilterOP rotamer_boltzmann_weight_;

};

} // namespace
} // namespace

#endif // include guard
