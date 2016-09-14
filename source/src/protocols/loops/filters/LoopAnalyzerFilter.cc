// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/loops/filters/LoopAnalyzerFilter.cc
/// @brief @brief LoopAnalyzerFilter examines loop structures and packages extra scores into a Job object; you can filter on its "LAM score".  This was originally LoopAnalyzerMover (which treated its Pose as const anyway), but has been converted to a filter.
/// @author Steven Lewis (smlewi@gmail.com)

#include <protocols/loops/filters/LoopAnalyzerFilter.hh>
#include <protocols/loops/filters/LoopAnalyzerFilterCreator.hh>

#include <protocols/analysis/LoopAnalyzerMover.hh>

// Package Headers
#include <protocols/loops/Loops.hh>

#include <protocols/loop_modeling/utilities/rosetta_scripts.hh>

// Project Headers
#include <core/pose/Pose.hh>

// Utility Headers
#include <core/types.hh>
#include <basic/Tracer.hh>
#include <utility/tag/Tag.hh>

static THREAD_LOCAL basic::Tracer TR( "protocols.loops.filters.LoopAnalyzerFilter" );

namespace protocols {
namespace loops {
namespace filters {

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////LoopAnalyzerFilter///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
LoopAnalyzerFilter::LoopAnalyzerFilter():
	protocols::filters::Filter( "LoopAnalyzerFilter" )
{}

LoopAnalyzerFilter::LoopAnalyzerFilter( protocols::loops::Loops const & loops, bool const tracer ) :
	protocols::filters::Filter( "LoopAnalyzerFilter" ),
	loops_(protocols::loops::LoopsCOP( new protocols::loops::Loops(loops) ) ),
	tracer_(tracer)
{}

LoopAnalyzerFilter::~LoopAnalyzerFilter()
{}

LoopAnalyzerFilter::LoopAnalyzerFilter( LoopAnalyzerFilter const & rhs ) :
	//utility::pointer::ReferenceCount(),
	Filter(),
	loops_(protocols::loops::LoopsCOP( new protocols::loops::Loops(*(rhs.loops_)) ) ),
	tracer_(rhs.tracer_)
{}

void
LoopAnalyzerFilter::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & ,
	protocols::filters::Filters_map const & ,
	protocols::moves::Movers_map const & ,
	core::pose::Pose const & )
{
	set_use_tracer(tag->getOption< bool >( "use_tracer", false ) );
	set_loops(protocols::loop_modeling::utilities::parse_loops_from_tag(tag));
}

protocols::filters::FilterOP
LoopAnalyzerFilter::clone() const
{
	return protocols::filters::FilterOP( new LoopAnalyzerFilter( *this ) );
}


protocols::filters::FilterOP
LoopAnalyzerFilter::fresh_instance() const
{
	return protocols::filters::FilterOP( new LoopAnalyzerFilter );
}

std::string
LoopAnalyzerFilter::get_name() const
{
	return LoopAnalyzerFilterCreator::filter_name();
}

/// @details Calculates a bunch of loop-geometry related score terms and dumps them to tracer / Job object for output with the Pose.
bool
LoopAnalyzerFilter::apply( core::pose::Pose const & /*input_pose*/ ) const
{

	/*
	TR << "running LoopAnalyzerFilter" << std::endl;
	//prep pose
	core::pose::Pose pose(input_pose); //protecting input pose from our chainbreak changes (and its energies object).  Really inefficient, as LAM does this too, but it's a cost of the predefined Mover and Filter interfaces.

	//make LAM, run lam
	protocols::analysis::LoopAnalyzerMover LAM(*(get_loops()), get_use_tracer());
	LAM.apply( pose );
	*/

	return true;
} //LoopAnalyzerFilter::apply

core::Real
LoopAnalyzerFilter::report_sm( core::pose::Pose const & input_pose ) const
{
	TR << "running LoopAnalyzerFilter" << std::endl;
	//prep pose
	core::pose::Pose pose(input_pose); //protecting input pose from our chainbreak changes (and its energies object).  Really inefficient, as LAM does this too, but it's a cost of the predefined Mover and Filter interfaces.

	//make LAM, run lam
	protocols::analysis::LoopAnalyzerMover LAM(*(get_loops()), get_use_tracer());
	LAM.apply( pose );
	return LAM.get_total_score();
}

void
LoopAnalyzerFilter::report( std::ostream &, core::pose::Pose const & ) const
{
	// std::cout << "STEVEN TODO";
}

//////////////////////////getters, setters/////////////////////
/// @brief set loops object, because public setters/getters are a rule
void LoopAnalyzerFilter::set_loops( protocols::loops::LoopsCOP loops ) { loops_ = loops; }

/// @brief get loops object, because public setters/getters are a rule
protocols::loops::LoopsCOP const & LoopAnalyzerFilter::get_loops( void ) const { return loops_; }

/////////////// Creator ///////////////

protocols::filters::FilterOP
LoopAnalyzerFilterCreator::create_filter() const
{
	return protocols::filters::FilterOP( new LoopAnalyzerFilter );
}

std::string
LoopAnalyzerFilterCreator::keyname() const
{
	return LoopAnalyzerFilterCreator::filter_name();
}

std::string
LoopAnalyzerFilterCreator::filter_name()
{
	return "LoopAnalyzerFilter";
}

} //protocols
} //loops
} //filters
