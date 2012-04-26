// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   devel/matdes/OligomericAverageDegreeFilter.hh
/// @brief  header file for OligomericAverageDegreeFilter class
/// @author Neil King (neilking@u.washington.edu)


#ifndef INCLUDED_devel_matdes_OligomericAverageDegreeFilter_hh
#define INCLUDED_devel_matdes_OligomericAverageDegreeFilter_hh

// Unit Headers
#include <devel/matdes/OligomericAverageDegreeFilter.fwd.hh>

// Package Headers
#include <protocols/filters/Filter.hh>

// Project Headers
#include <core/pose/Pose.fwd.hh>
#include <core/pack/task/operation/TaskOperation.fwd.hh>
#include <core/pack/task/TaskFactory.fwd.hh>

// Utility headers
#include <utility/vector1.fwd.hh>

// Parser headers
#include <protocols/moves/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <protocols/filters/Filter.fwd.hh>
#include <utility/tag/Tag.fwd.hh>

#include <utility/vector1.hh>


//// C++ headers

namespace devel {
namespace matdes {

class OligomericAverageDegreeFilter : public protocols::filters::Filter {
public:

	typedef protocols::filters::Filter Super;
	typedef protocols::filters::Filter Filter;
	typedef protocols::filters::FilterOP FilterOP;
	typedef core::Real Real;
	typedef core::pose::Pose Pose;

	typedef utility::tag::TagPtr TagPtr;
	typedef protocols::filters::Filters_map Filters_map;
	typedef protocols::moves::DataMap DataMap;
	typedef protocols::moves::Movers_map Movers_map;


public:// constructor/destructor


	// @brief default constructor
	OligomericAverageDegreeFilter();

	// @brief constructor with arguments
	OligomericAverageDegreeFilter( core::pack::task::TaskFactoryOP task_factory, core::Real const t, core::Real const d, core::Size jump );

	// @brief copy constructor
	OligomericAverageDegreeFilter( OligomericAverageDegreeFilter const & rval );

	virtual ~OligomericAverageDegreeFilter(){}


public:// virtual constructor


	// @brief make clone
	virtual protocols::filters::FilterOP clone() const;

	// @brief make fresh instance
	virtual protocols::filters::FilterOP fresh_instance() const;


public:// accessor

	// @brief get name of this filter
	virtual std::string name() const { return "OligomericAverageDegree"; }

public:// setters

	void task_factory( core::pack::task::TaskFactoryOP task_factory );
	void threshold( core::Real const t );
	void distance_threshold( core::Real const d );
	void jump_id( core::Size const jump );

public:// getters
	core::pack::task::TaskFactoryOP task_factory() const;
	core::Real threshold() const;
	core::Real distance_threshold() const;
	core::Size jump_id() const;

public:// parser

	virtual void parse_my_tag( TagPtr const tag,
		DataMap &,
		protocols::filters::Filters_map const &,
		Movers_map const &,
		Pose const & );


public:// virtual main operation

	// @brief returns true if the given pose passes the filter, false otherwise.
  virtual bool apply( core::pose::Pose const & pose ) const;

	/// @brief
  virtual core::Real report_sm( core::pose::Pose const & pose ) const;
  virtual void report( std::ostream & out, core::pose::Pose const & pose ) const;

	/// @brief calc oligomeric AverageDegree
  core::Real compute( core::pose::Pose const & pose ) const;


private:

  core::pack::task::TaskFactoryOP task_factory_;
  core::Real threshold_;
  core::Real distance_threshold_;
	core::Size jump_id_;

};

} // matdes
} // devel

#endif
