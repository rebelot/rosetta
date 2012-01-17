// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/features/SaltBridgeFeatures.hh
/// @brief  report comments stored with each pose
/// @author Matthew O'Meara

#ifndef INCLUDED_protocols_features_SaltBridgeFeatures_hh
#define INCLUDED_protocols_features_SaltBridgeFeatures_hh

// Unit Headers
#include <protocols/features/FeaturesReporter.hh>
#include <protocols/features/SaltBridgeFeatures.fwd.hh>

// Project Headers
#include <core/types.hh>
// AUTO-REMOVED #include <core/pose/Pose.hh>
#include <utility/vector1.fwd.hh>

// C++ Headers
#include <string>

#include <utility/vector1.hh>


namespace protocols{
namespace features{

class SaltBridgeFeatures : public FeaturesReporter {
public:
	SaltBridgeFeatures();

	SaltBridgeFeatures(core::Length distance_cutoff);

	SaltBridgeFeatures(SaltBridgeFeatures const & src);

	virtual ~SaltBridgeFeatures(){}

	core::Length
	distance_cutoff() const { return distance_cutoff_; }

	void
	distance_cutoff(core::Length d) { distance_cutoff_ = d; }

	///@brief return string with class name
	std::string
	type_name() const;

	///@brief return sql statements that setup the right tables
	std::string
	schema() const;

	///@brief return the set of features reporters that are required to
	///also already be extracted by the time this one is used.
	utility::vector1<std::string>
	features_reporter_dependencies() const;

	///@brief collect all the feature data for the pose
	core::Size
	report_features(
		core::pose::Pose const & pose,
		utility::vector1< bool > const &,
		core::Size struct_id,
		utility::sql_database::sessionOP db_session);

private:

	core::Length distance_cutoff_;

};


} // features namespace
} // protocols namespace

#endif // include guard
