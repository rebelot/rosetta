// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/rosetta_scripts/ParsedProtocol.cc
/// @author Sarel Fleishman (sarelf@u.washington.edu)

// Unit Headers
#include <protocols/rosetta_scripts/ParsedProtocol.hh>
#include <protocols/rosetta_scripts/ParsedProtocolCreator.hh>
#include <protocols/moves/NullMover.hh>
#include <protocols/moves/DataMapObj.hh>
#include <protocols/moves/DataMap.hh>

// Project Headers
#include <protocols/moves/Mover.hh>
#include <core/pose/Pose.hh>
#include <protocols/moves/MoverStatus.hh>

#include <core/kinematics/Jump.hh>
#include <basic/Tracer.hh>

#include <core/scoring/ScoreFunction.hh>

#include <core/pose/symmetry/util.hh>
#include <core/scoring/symmetry/SymmetricScoreFunction.hh>

#include <protocols/filters/Filter.hh>
#include <protocols/filters/BasicFilters.hh>
#include <utility/tag/Tag.hh>
#include <protocols/moves/ResId.hh>

// JD2 headers
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/JobOutputter.hh>
#include <protocols/jd2/Job.hh>
// Utility Headers

// AUTO-REMOVED #include <core/pose/symmetry/util.hh>
// AUTO-REMOVED #include <core/conformation/symmetry/util.hh>

//Numeric Headers
#include <numeric/random/random.hh>
#include <numeric/random/random_permutation.hh>

// C++ headers
#include <map>
#include <string>
// AUTO-REMOVED #include <set>
#include <algorithm>

#include <protocols/jd2/Job.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <boost/foreach.hpp>

//Auto Headers
#define foreach BOOST_FOREACH
namespace protocols {
namespace rosetta_scripts {

static basic::Tracer TR( "protocols.rosetta_scripts.ParsedProtocol" );
static basic::Tracer TR_call_order( "protocols.rosetta_scripts.ParsedProtocol_call_order" );
static basic::Tracer TR_report( "protocols.rosetta_scripts.ParsedProtocol.REPORT" );
static numeric::random::RandomGenerator RG(48569);

typedef core::Real Real;
typedef core::pose::Pose Pose;

using namespace core;
using namespace std;

std::string
ParsedProtocolCreator::keyname() const
{
	return ParsedProtocolCreator::mover_name();
}

protocols::moves::MoverOP
ParsedProtocolCreator::create_mover() const {
	return new ParsedProtocol;
}

std::string
ParsedProtocolCreator::mover_name()
{
	return "ParsedProtocol";
}

ParsedProtocol::ParsedProtocol() :
	protocols::moves::Mover( "ParsedProtocol" ),
	final_scorefxn_( 0 ), // By default, don't rescore with any scorefunction.
	mode_("sequence"),
	last_attempted_mover_idx_( 0 ),
	report_call_order_( false )
{}

/// @detailed Takes care of the docking, design and filtering moves. pre_cycle and pose_cycle can
/// be setup in derived classes to setup variables before and after these cycles.
void
ParsedProtocol::apply( Pose & pose )
{
	protocols::moves::Mover::set_last_move_status( protocols::moves::FAIL_RETRY );
	pose.update_residue_neighbors();

	//fpd search the mover-filter pairs backwards for movers that have remaining poses
	utility::vector1< mover_filter_pair >::const_reverse_iterator rmover_it;
	const utility::vector1< mover_filter_pair >::const_reverse_iterator movers_crend = movers_.rend();
	if(mode_ == "sequence")
	{
		for ( rmover_it=movers_.rbegin() ; rmover_it != movers_crend; ++rmover_it ) {
			core::pose::PoseOP checkpoint = (*rmover_it).first.first->get_additional_output();

			// otherwise continue where we left off
			if (checkpoint) {
				std::string const mover_name( rmover_it->first.first->get_name() );

				// if mode_ is not 'sequence' then checkpointing is unsupported
				if (checkpoint && mode_ != "sequence")
					utility_exit_with_message("Mover "+mover_name+" returned multiple poses in a ParsedProtocol with mode!=sequence");

				TR<<"=======================RESUMING FROM "<<mover_name<<"======================="<<std::endl;
				pose = *checkpoint;

				if( ! apply_filter( pose, *rmover_it) ) {
//					final_score(pose);
					return;
				} else {
					break;
				}
			}
		}
	}

	if(mode_ == "sequence"){
		sequence_protocol(pose, rmover_it.base());
	}else if(mode_ =="random_order"){
		random_order_protocol(pose);
	}else if(mode_ =="single_random"){
		random_single_protocol(pose);
	}else
	{
		TR <<"WARNING: mode is " << mode_ << " .This is not a valid ParsedProtocol Mode, your pose is being ignored" <<std::endl;
	}
	if( get_last_move_status() == protocols::moves::MS_SUCCESS ) // no point scoring a failed trajectory (and sometimes you get etable vs. pose atomset mismatches
		final_score(pose);
}

std::string
ParsedProtocol::get_name() const {
	return ParsedProtocolCreator::mover_name();
}

/// Tricky! movers are cloned into the protocol b/c their apply functions (which are nonconst) could accumulate
/// state information. Filters are safe and are therefore merely registered.
/// Under this state of affairs, a mover or filter may be called many times in the protocol, and it will be
/// guaranteed to have no state accumulation.
void ParsedProtocol::add_mover( protocols::moves::MoverCOP mover, std::string const mover_name, protocols::filters::FilterOP filter ) {
	protocols::moves::MoverOP mover_p = mover->clone();
	protocols::filters::FilterOP filter_p = filter;
	mover_filter_pair p( std::pair< protocols::moves::MoverOP, std::string > ( mover_p, mover_name ), filter_p );
	movers_.push_back( p );
}

void ParsedProtocol::final_scorefxn( core::scoring::ScoreFunctionCOP scorefxn )
{
	final_scorefxn_ = scorefxn;
}

core::scoring::ScoreFunctionCOP ParsedProtocol::final_scorefxn() const
{
	return final_scorefxn_;
}

void
ParsedProtocol::final_score(core::pose::Pose & pose) const {
	core::scoring::ScoreFunctionCOP scorefxn = final_scorefxn() ;

	if( ! scorefxn ) { return; }

	if (core::pose::symmetry::is_symmetric(pose)) {
		scorefxn = new core::scoring::symmetry::SymmetricScoreFunction(scorefxn);
	}
	(*scorefxn)(pose);
}

void
ParsedProtocol::report_all( Pose const & pose ) const {
	TR_report<<"=============Starting final report================"<<std::endl;
	foreach(mover_filter_pair mover_pair, movers_){
		TR_report<<"============Begin report for "<<mover_pair.second->get_user_defined_name()<<"=================="<<std::endl;
		mover_pair.second->report( TR_report, pose );
		TR_report<<"============End report for "<<mover_pair.second->get_user_defined_name()<<"=================="<<std::endl;
	}
	TR_report.flush();
}

void
ParsedProtocol::report_filters_to_job( Pose const & pose) const {
	using protocols::jd2::JobDistributor;
	protocols::jd2::JobOP job_me( JobDistributor::get_instance()->current_job() );
	for( utility::vector1< mover_filter_pair >::const_iterator mover_it = movers_.begin();
			 mover_it!=movers_.end(); ++mover_it ) {
		core::Real const filter_value( (*mover_it).second->report_sm( pose ) );
		if( filter_value > -9999 )
			job_me->add_string_real_pair((*mover_it).second->get_user_defined_name(), filter_value);
	}
}

void
ParsedProtocol::report_all_sm( std::map< std::string, core::Real > & score_map, Pose const & pose ) const {
	for( utility::vector1< mover_filter_pair >::const_iterator mover_it = movers_.begin();
		 mover_it!=movers_.end(); ++mover_it ) {
		 core::Real const filter_value( (*mover_it).second->report_sm( pose ) );
		 if( filter_value >= -9999 )
			score_map[ (*mover_it).second->get_user_defined_name() ] = filter_value;
	}
}

ParsedProtocol::iterator
ParsedProtocol::begin(){
	return movers_.begin();
}

ParsedProtocol::const_iterator
ParsedProtocol::begin() const{
	return movers_.begin();
}

ParsedProtocol::iterator
ParsedProtocol::end(){
	return movers_.end();
}
ParsedProtocol::~ParsedProtocol() {}

ParsedProtocol::const_iterator
ParsedProtocol::end() const{
	return movers_.end();
}

/// @details sets resid for the constituent filters and movers
void
ParsedProtocol::set_resid( core::Size const resid ){
	for( iterator it( movers_.begin() ); it!=movers_.end(); ++it ){
		using namespace protocols::moves;
		modify_ResId_based_object( it->first.first, resid );
		modify_ResId_based_object( it->second, resid );
	}
}

protocols::moves::MoverOP ParsedProtocol::clone() const
{
	return protocols::moves::MoverOP( new protocols::rosetta_scripts::ParsedProtocol( *this ) );
}

void
ParsedProtocol::parse_my_tag(
	TagPtr const tag,
	protocols::moves::DataMap &data,
	protocols::filters::Filters_map const &filters,
	protocols::moves::Movers_map const &movers,
	core::pose::Pose const & )
{
	using namespace protocols::moves;
	using namespace utility::tag;

	TR<<"ParsedProtocol mover with the following movers and filters\n";

	mode_=tag->getOption<string>("mode", "sequence");
	if(mode_ != "sequence" && mode_ != "random_order" && mode_ != "single_random"){
		utility_exit_with_message("Error: mode must be sequence, random_order, or single_random");
	}

	utility::vector0< TagPtr > const dd_tags( tag->getTags() );
	utility::vector1< core::Real > a_probability( dd_tags.size(), 1.0/dd_tags.size() );
	core::Size count( 1 );
	for( utility::vector0< TagPtr >::const_iterator dd_it=dd_tags.begin(); dd_it!=dd_tags.end(); ++dd_it ) {
		TagPtr const tag_ptr = *dd_it;

		MoverOP mover_to_add;
		protocols::filters::FilterOP filter_to_add;

		bool mover_defined( false ), filter_defined( false );

		std::string mover_name="null"; // user must specify a mover name. there is no valid default.
		runtime_assert( !( tag_ptr->hasOption("mover_name") && tag_ptr->hasOption("mover") ) );
		if( tag_ptr->hasOption( "mover_name" ) ){
			mover_name = tag_ptr->getOption<string>( "mover_name", "null" );
			mover_defined = true;
		}
		else if( tag_ptr->hasOption( "mover" ) ){
			mover_name = tag_ptr->getOption<string>( "mover", "null" );
			mover_defined = true;
		}
		//runtime_assert( mover_name ); // redundant with mover find below

		if( data.has( "stopping_condition", mover_name ) && tag->hasOption( "name" ) ){
			TR<<"ParsedProtocol's mover "<<mover_name<<" requests its own stopping condition. This ParsedProtocol's stopping_condition will point at the mover's"<<std::endl;
			data.add( "stopping_condition", tag->getOption< std::string >( "name" ), data.get< protocols::moves::DataMapObj< bool > * >( "stopping_condition", mover_name ) );
		}

		std::string filter_name="true_filter"; // used in case user does not specify a filter name.
		runtime_assert( !( tag_ptr->hasOption("filter_name") && tag_ptr->hasOption( "filter" ) ) );
		if( tag_ptr->hasOption( "filter_name" ) ){
			filter_name = tag_ptr->getOption<string>( "filter_name", "true_filter" );
			filter_defined = true;
		} else if( tag_ptr->hasOption( "filter" ) ){
			filter_name = tag_ptr->getOption<string>( "filter", "true_filter" );
			filter_defined = true;
		}

		if( mover_defined ){
			Movers_map::const_iterator find_mover( movers.find( mover_name ) );
			if( find_mover == movers.end() ) {
				TR.Error<<"mover not found in map. skipping:\n"<<tag_ptr<<std::endl;
				runtime_assert( find_mover != movers.end() );
				continue;
			}
			mover_to_add = find_mover->second;
		}	else {
			mover_to_add = new NullMover;
		}
		if( filter_defined ){
			protocols::filters::Filters_map::const_iterator find_filter( filters.find( filter_name ));
			if( find_filter == filters.end() ) {
				TR.Error<<"filter not found in map. skipping:\n"<<tag_ptr<<std::endl;
				runtime_assert( find_filter != filters.end() );
				continue;
			}
			filter_to_add = find_filter->second;
		} else {
			filter_to_add = new protocols::filters::TrueFilter;
		}
		add_mover( mover_to_add, mover_name, filter_to_add );
		TR << "added mover \"" << mover_name << "\" with filter \"" << filter_name << "\"\n";
		if( mode_ == "single_random" ){
			a_probability[ count ] = tag_ptr->getOption< core::Real >( "apply_probability", 1.0/dd_tags.size() );
			TR<<"and execution probability of "<<a_probability[ count ]<<'\n';
		}
		count++;
	}
	if( mode_ == "single_random" )
		apply_probability( a_probability );
	report_call_order( tag->getOption< bool >( "report_call_order", false ) );
	TR.flush();
}

/// @detailed Looks for any submovers that have additional output poses to process.
/// If any are found, run remainder of parsed protocol.
core::pose::PoseOP
ParsedProtocol::get_additional_output( )
{
	//fpd search the mover-filter pairs backwards; look for movers that have remaining poses
	core::pose::PoseOP pose=NULL;
	utility::vector1< mover_filter_pair >::const_reverse_iterator rmover_it;
	const utility::vector1< mover_filter_pair >::const_reverse_iterator movers_crend = movers_.rend();
	for ( rmover_it=movers_.rbegin() ; rmover_it != movers_crend; ++rmover_it ) {
		core::pose::PoseOP checkpoint = (*rmover_it).first.first->get_additional_output();
		if (checkpoint) {
			std::string const mover_name( rmover_it->first.first->get_name() );
			TR<<"=======================RESUMING FROM "<<mover_name<<"======================="<<std::endl;
			pose = checkpoint;

			if( ! apply_filter( *pose, *rmover_it) ) {
				return pose;
			} else {
				break;
			}
		}
	}

	// no saved poses?  return now
	if (!pose) return NULL;

	// if mode_ is not 'sequence' then checkpointing is unsupported
	if (mode_ != "sequence")
		utility_exit_with_message("ParsedProtocol returned multiple poses in a ParsedProtocol with mode!=sequence");

	// otherwise pick up from the checkpoint
	for( utility::vector1< mover_filter_pair >::const_iterator mover_it = rmover_it.base();
			mover_it!=movers_.end(); ++mover_it ) {
		if ( ! apply_mover_filter_pair( *pose, *mover_it ) ) {
			return pose;
		}
	}
	protocols::moves::Mover::set_last_move_status( protocols::moves::MS_SUCCESS ); // tell jobdistributor to save pose
	TR<<"setting status to success"<<std::endl;

	// report filter values to the job object as string_real_pair
	report_filters_to_job( *pose );
	// report filter values to tracer output
	report_all( *pose );
	// rescore the pose with either score12 or a user-specified scorefunction. this ensures that all output files end up with scores.
//	core::scoring::ScoreFunctionOP scorefxn = core::scoring::getScoreFunction();
//	(*scorefxn)(*pose);

	return pose;
}

bool ParsedProtocol::apply_mover_filter_pair(Pose & pose, mover_filter_pair const & mover_pair)
{
	std::string const mover_name( mover_pair.first.first->type() );

	mover_pair.first.first->set_native_pose( get_native_pose() );
	TR<<"=======================BEGIN MOVER "<<mover_name<<"=======================\n{"<<std::endl;
	mover_pair.first.first->apply( pose );
	TR<<"\n}\n=======================END MOVER "<<mover_name<<"======================="<<std::endl;

	// Split out filter application in seperate function to allow for reuse in resuming from additional output pose cases.
	return apply_filter( pose, mover_pair);
}

bool ParsedProtocol::apply_filter(Pose & pose, mover_filter_pair const & mover_pair)
{
	std::string const filter_name( mover_pair.second->get_user_defined_name() );

	TR<<"=======================BEGIN FILTER "<<filter_name<<"=======================\n{"<<std::endl;
	info().insert( info().end(), mover_pair.first.first->info().begin(), mover_pair.first.first->info().end() );
	pose.update_residue_neighbors();
	moves::MoverStatus status( mover_pair.first.first->get_last_move_status() );
	bool const pass( status==protocols::moves::MS_SUCCESS  && mover_pair.second->apply( pose ) );
	TR<<"\n}\n=======================END FILTER "<<filter_name<<"======================="<<std::endl;
	if( !pass ) {
		if( status != protocols::moves::MS_SUCCESS ) {
			TR << "Mover " << mover_pair.first.first->get_name() << " reports failure!" << std::endl;
			protocols::moves::Mover::set_last_move_status( status );
		} else {
			TR << "Filter " << filter_name << " reports failure!" << std::endl;
      protocols::moves::Mover::set_last_move_status( protocols::moves::FAIL_RETRY );
		}
		return false;
	}
	return true;
}

void ParsedProtocol::finish_protocol(Pose & pose) {
	protocols::moves::Mover::set_last_move_status( protocols::moves::MS_SUCCESS ); // tell jobdistributor to save pose
	TR<<"setting status to success"<<std::endl;

	// report filter values to the job object as string_real_pair
	report_filters_to_job( pose );
	// report filter values to tracer output
	report_all( pose );

  using namespace protocols::jd2;
	protocols::jd2::JobOP job2 = jd2::JobDistributor::get_instance()->current_job();
	std::string job_name (JobDistributor::get_instance()->job_outputter()->output_name( job2 ) );
	if( report_call_order() ){
		TR_call_order << job_name<<" ";
		foreach( mover_filter_pair const p, movers_ )
			TR_call_order<<p.first.second<<" ";
		TR_call_order<<std::endl;
	}
	// rescore the pose with either score12 or a user-specified scorefunction. this ensures that all output files end up with scores.
//	core::scoring::ScoreFunctionOP scorefxn = core::scoring::getScoreFunction();
//	(*scorefxn)(pose);

}


void ParsedProtocol::sequence_protocol(Pose & pose, utility::vector1< mover_filter_pair >::const_iterator mover_it_in)
{
	//bool last_mover=false;
	for( utility::vector1< mover_filter_pair >::const_iterator mover_it = mover_it_in;
			mover_it!=movers_.end(); ++mover_it ) {

		if(!apply_mover_filter_pair(pose, *mover_it)) {
			return;
		}
	}

	// we're done! mark as success
	finish_protocol( pose );
}


void ParsedProtocol::random_order_protocol(Pose & pose){
	numeric::random::random_permutation(movers_.begin(),movers_.end(),RG);
	for(utility::vector1<mover_filter_pair>::const_iterator it = movers_.begin(); it != movers_.end();++it)
	{
		if(!apply_mover_filter_pair(pose, *it))
		{
			return;
		}
	}
	// we're done! mark as success
	finish_protocol( pose );
}

utility::vector1< core::Real >
ParsedProtocol::apply_probability() {
	core::Real sum( 0 );
	foreach( core::Real const prob, apply_probability_ )
		sum += prob;
	runtime_assert( sum >= 0.999 && sum <= 1.001 );
	return apply_probability_;
}

void
ParsedProtocol::apply_probability( utility::vector1< core::Real > a ){
	apply_probability_ = a;
	runtime_assert( apply_probability_.size() == movers_.size() );
	core::Real sum( 0 );
	foreach( core::Real const prob, apply_probability_ )
		sum += prob;
	runtime_assert( sum >= 0.999 && sum <= 1.001 );
}

void ParsedProtocol::random_single_protocol(Pose & pose){
	core::Real const random_num( RG.uniform() );
	core::Real sum( 0.0 );
	core::Size mover_index( 0 );
	foreach( core::Real const probability, apply_probability() ){
		sum += probability; mover_index++;
		if( sum >= random_num )
			break;
	}
	last_attempted_mover_idx( mover_index );
	if(!apply_mover_filter_pair(pose, movers_[mover_index])) {
		return;
	}

	// we're done! mark as success
	finish_protocol( pose );
}

} //rosetta_scripts
} //protocols

