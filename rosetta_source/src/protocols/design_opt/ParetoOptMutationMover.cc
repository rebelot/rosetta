// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 sw=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @author Chris King (chrisk1@uw.edu)
//#include <algorithm >
#include <protocols/design_opt/PointMutationCalculator.hh>
#include <protocols/design_opt/ParetoOptMutationMover.hh>
#include <protocols/design_opt/ParetoOptMutationMoverCreator.hh>
#include <protocols/toolbox/task_operations/DesignAroundOperation.hh>
#include <core/pose/PDBInfo.hh>
#include <fstream>
// AUTO-REMOVED #include <utility/file/FileName.hh>
#include <iostream>
// AUTO-REMOVED #include <basic/options/keys/in.OptionKeys.gen.hh>
// AUTO-REMOVED #include <basic/options/option_macros.hh>
#include <numeric/random/random.hh>
#include <core/chemical/ResidueType.fwd.hh>
#include <core/pose/Pose.hh>
#include <core/conformation/Conformation.hh>
#include <core/conformation/Residue.hh>
#include <utility/tag/Tag.hh>
#include <protocols/filters/Filter.hh>
// AUTO-REMOVED #include <protocols/moves/DataMap.hh>
#include <basic/Tracer.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/PackerTask.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <core/pack/pack_rotamers.hh>
#include <core/scoring/ScoreFunction.hh>
// AUTO-REMOVED #include <core/pack/task/operation/TaskOperations.hh>
#include <core/chemical/ResidueType.hh>
#include <utility/vector1.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/jd2/util.hh>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>
#include <protocols/simple_moves/symmetry/SymPackRotamersMover.hh>
#include <protocols/jd2/Job.hh>
#include <utility/vector0.hh>
#include <core/pose/symmetry/util.hh>
#include <protocols/simple_moves/symmetry/SymMinMover.hh>

//Auto Headers
#include <basic/options/keys/OptionKeys.hh>

namespace protocols {
namespace design_opt {

static basic::Tracer TR( "protocols.design_opt.ParetoOptMutationMover" );
using namespace core;
using namespace chemical;
using utility::vector1;
using std::pair;

///@brief default ctor
ParetoOptMutationMover::ParetoOptMutationMover() :
	Mover( ParetoOptMutationMoverCreator::mover_name() ),
	task_factory_( NULL ),
//	filters_( NULL ), /* how set default vecgtor of NULLs? */
//	sample_type_( "low" ),
	relax_mover_( NULL ),
	scorefxn_( NULL ),
	diversify_lvl_( 1 ),
	dump_pdb_( false ),
	stopping_condition_( NULL )
{}

//full ctor
ParetoOptMutationMover::ParetoOptMutationMover(
	core::pack::task::TaskFactoryOP task_factory,
	core::scoring::ScoreFunctionOP scorefxn,
	protocols::moves::MoverOP relax_mover,
	vector1< protocols::filters::FilterOP > filters,
	vector1< std::string > sample_types,
	bool dump_pdb,
	core::Size diversify_lvl,
	protocols::filters::FilterOP stopping_condition
) :
	Mover( ParetoOptMutationMoverCreator::mover_name() )
{
	task_factory_ = task_factory;
	filters_ = filters;
	relax_mover_ = relax_mover;
	scorefxn_ = scorefxn;
	sample_types_ = sample_types;
	diversify_lvl_ = diversify_lvl;
	dump_pdb_ = dump_pdb;
	stopping_condition_ = stopping_condition;
}

//destruction!
ParetoOptMutationMover::~ParetoOptMutationMover(){}

//creators
protocols::moves::MoverOP
ParetoOptMutationMoverCreator::create_mover() const {
	return new ParetoOptMutationMover;
}

protocols::moves::MoverOP
ParetoOptMutationMover::clone() const{
	return new ParetoOptMutationMover( *this );
}

//name getters
std::string
ParetoOptMutationMoverCreator::keyname() const
{
	return ParetoOptMutationMoverCreator::mover_name();
}

std::string
ParetoOptMutationMoverCreator::mover_name()
{
	return "ParetoOptMutationMover";
}

std::string
ParetoOptMutationMover::get_name() const {
  return ParetoOptMutationMoverCreator::mover_name();
}

// setter - getter pairs
void
ParetoOptMutationMover::relax_mover( protocols::moves::MoverOP mover ){
	relax_mover_ = mover;
}

protocols::moves::MoverOP
ParetoOptMutationMover::relax_mover() const{
	return relax_mover_;
}

void
ParetoOptMutationMover::filters( vector1< protocols::filters::FilterOP > filters ){
	filters_ = filters;
}

vector1< protocols::filters::FilterOP > ParetoOptMutationMover::filters() const{
	return filters_;
}

void
ParetoOptMutationMover::task_factory( core::pack::task::TaskFactoryOP task_factory )
{
	task_factory_ = task_factory;
}

core::pack::task::TaskFactoryOP
ParetoOptMutationMover::task_factory() const
{
	return task_factory_;
}

void
ParetoOptMutationMover::dump_pdb( bool const dump_pdb ){
  dump_pdb_ = dump_pdb;
}

bool
ParetoOptMutationMover::dump_pdb() const{
  return dump_pdb_;
}

void
ParetoOptMutationMover::sample_types( vector1< std::string > const sample_types ){
  sample_types_ = sample_types;
}

vector1< std::string >
ParetoOptMutationMover::sample_types() const{
  return sample_types_;
}

void
ParetoOptMutationMover::diversify_lvl( core::Size const diversify_lvl ){
  diversify_lvl_ = diversify_lvl;
}

core::Size
ParetoOptMutationMover::diversify_lvl() const{
  return diversify_lvl_;
}

void
ParetoOptMutationMover::scorefxn( core::scoring::ScoreFunctionOP scorefxn ){
	scorefxn_ = scorefxn;
}

core::scoring::ScoreFunctionOP
ParetoOptMutationMover::scorefxn() const{
	return scorefxn_;
}

//utility funxns for comparing values in sort
bool
cmp_pair_by_first_vec_val(
	pair< AA, vector1< Real > > const pair1,
	pair< AA, vector1< Real > > const pair2 )
{
	return pair1.second[ 1 ] < pair2.second[ 1 ];
}

bool
cmp_pair_vec_by_first_vec_val(
  pair< Size, vector1< pair< AA, vector1< Real > > > > const pair1,
  pair< Size, vector1< pair< AA, vector1< Real > > > > const pair2 )
{
  return pair1.second[ 1 ].second[ 1 ] < pair2.second[ 1 ].second[ 1 ];
}

//TODO: this should also compare fold trees
bool
ParetoOptMutationMover::pose_coords_are_same( core::pose::Pose const & pose1, core::pose::Pose const & pose2 )
{
	//first check for all restype match, also checks same number res
	if( !pose1.conformation().sequence_matches( pose2.conformation() ) ) return false;
	//then check for all coords identical
	for ( Size i = 1; i <= pose1.total_residue(); ++i ) {
		core::conformation::Residue const & rsd1( pose1.residue( i ) );
		core::conformation::Residue const & rsd2( pose2.residue( i ) );
		//check same n atoms
		if( rsd1.natoms() != rsd2.natoms() ) return false;
		//and coords
		for( Size ii = 1; ii <= rsd1.natoms(); ++ii ) {
			if( rsd1.xyz( ii ).x() != rsd2.xyz( ii ).x() ) return false;
			if( rsd1.xyz( ii ).y() != rsd2.xyz( ii ).y() ) return false;
			if( rsd1.xyz( ii ).z() != rsd2.xyz( ii ).z() ) return false;
		}    
	}    
	return true;
}

//Yes, I'm aware this is the slowest and simplest implementation of pareto front
// identification, but this needs to be finished 2 days ago
// I can make it all fancy later
//calc_single_pos_pareto_front
//takes a set (vec1) of coords
//populates boolean vector w/ is_pareto?
void
calc_pareto_front(
	vector1< vector1< Real > > const & coords,
	vector1< bool > & is_pfront
){
	assert( coords.size() == is_pfront.size() );
	//n coords, d dimensions
	Size n( coords.size() );
	Size d( coords[ 1 ].size() );
	//for each point
	for( Size i = 1; i <= n; ++i ){
		bool is_dom( false );
		//check if strictly dominated by another point
		for( Size j = 1; j <= n; ++j ){
			if( j == i ) continue;
			is_dom = true;
			for( Size k = 1; k <= d; ++k ){
				//i not dominated by j if less in any dimension
				if( coords[ i ][ k ] <= coords[ j ][ k ] ){
					is_dom = 0;
					break;
				}
			}
			if( is_dom ) break;
		}
		if( is_dom ) is_pfront[ i ] = false;
		else is_pfront[ i ] = true;
	}
}

//removes seqpos aa/vals from set that are not pareto optimal
void
ParetoOptMutationMover::filter_seqpos_pareto_opt_ptmuts(){
	for( Size iseq = 1; iseq <= seqpos_aa_vals_vec_.size(); ++iseq ){
		vector1< bool > is_pfront( seqpos_aa_vals_vec_[ iseq ].second.size(), false );
		vector1< vector1< Real > > vals;
		//get a vec of vecs from the data
		for( Size i = 1; i <= seqpos_aa_vals_vec_[ iseq ].second.size(); ++i ){
			vals.push_back( seqpos_aa_vals_vec_[ iseq ].second[ i ].second );
		}
		//get is_pareto bool vec
		calc_pareto_front( vals, is_pfront );
		//replace aa/vals vector with pareto opt only
		vector1< pair< AA, vector1< Real > > > pfront_aa_vals;
		for( Size iaa = 1; iaa <= seqpos_aa_vals_vec_[ iseq ].second.size(); ++iaa ){
			if( is_pfront[ iaa ] ) pfront_aa_vals.push_back( seqpos_aa_vals_vec_[ iseq ].second[ iaa ] );
		}
		seqpos_aa_vals_vec_[ iseq ].second = pfront_aa_vals;
	}
}

//filters a pose vec for pareto opt only
void
filter_pareto_opt_poses(
	vector1< pose::Pose > & poses,
	vector1< vector1< Real > > & vals
){
	//get is_pareto bool vec
	vector1< bool > is_pfront( poses.size(), false );
	calc_pareto_front( vals, is_pfront );
	//remove entries that are not pareto
	vector1< pose::Pose > pfront_poses;
	vector1< vector1< Real > > pfront_vals;
	for( Size ipose = 1; ipose <= poses.size(); ++ipose ){
		if( is_pfront[ ipose ] ){
			pfront_poses.push_back( poses[ ipose ] );
			pfront_vals.push_back( vals[ ipose ] );
		}
	}
	poses = pfront_poses;
	vals = pfront_vals;
}

void
ParetoOptMutationMover::apply( core::pose::Pose & pose )
{
	using namespace core::pack::task;
	using namespace core::pack::task::operation;
	using namespace core::chemical;

	//store input pose
	core::pose::Pose start_pose( pose );

	//create vec of pairs of seqpos, vector of AA/val pairs that pass input filter
	//then combine them into a pareto opt pose set
	//only calc the ptmut data and pareto set once per pose, not at every nstruct iteration
	//but how will we know if that data is still valid? what if the pose has chnged?
	//the best answer is to store the pose passed to apply in a private variable (ref_pose_)
	//and only calc if ref_pose_ is still undef or doesnt match apply pose
	//also recalc if pareto pose set is empty
	if( pfront_poses_.empty() || ref_pose_.empty() || !pose_coords_are_same( start_pose, ref_pose_ ) ){
		//reset our private data
		seqpos_aa_vals_vec_.clear();
		pfront_poses_.clear();
		pfront_pose_iter_ = 1;
		//and (re)set ref_pose_ to this pose
		ref_pose_ = start_pose;

		//get the point mut values
		design_opt::PointMutationCalculatorOP ptmut_calc( new design_opt::PointMutationCalculator(
					task_factory(), scorefxn(), relax_mover(), filters(), sample_types(), dump_pdb() ) );
		ptmut_calc->calc_point_mut_filters( start_pose, seqpos_aa_vals_vec_ );

		//this part sorts the seqpos/aa/val data so that we init with something good (1st)
		//first over each seqpos by aa val, then over all seqpos by best aa val
		for( Size ivec = 1; ivec <= seqpos_aa_vals_vec_.size(); ++ivec ){
			//skip if aa/vals vector is empty
			if( seqpos_aa_vals_vec_[ ivec ].second.empty() ) continue;
			//sort aa/vals in incr val order
			std::sort( seqpos_aa_vals_vec_[ ivec ].second.begin(),
					seqpos_aa_vals_vec_[ ivec ].second.end(), cmp_pair_by_first_vec_val );
		}
		//now sort seqpos_aa_vals_vec_ by *first* (lowest) val in each seqpos vector, low to high
		//uses cmp_pair_vec_by_first_vec_val to sort based on second val in
		//first pair element of vector in pair( size, vec( pair ) )
		std::sort( seqpos_aa_vals_vec_.begin(), seqpos_aa_vals_vec_.end(), cmp_pair_vec_by_first_vec_val );

		//this part gets rid of ptmuts that are not pareto opt
		filter_seqpos_pareto_opt_ptmuts();

		TR<<"Combining independently pareto optimal mutations… " << std::endl;

		//init pareto opt poses with first mutations for now
		//TODO: is there a better way to init?
		Size iseq_init( 1 );
		//the resi index is the first part of the pair
		Size resi_init( seqpos_aa_vals_vec_[ iseq_init ].first );
		for( Size iaa = 1; iaa <= seqpos_aa_vals_vec_[ iseq_init ].second.size(); ++iaa ){
			AA target_aa( seqpos_aa_vals_vec_[ iseq_init ].second[ iaa ].first );
			pose::Pose new_pose( start_pose );
			ptmut_calc->mutate_and_relax( new_pose, resi_init, target_aa );
			//dont need to eval filters because we already know they passed because are in the table
			pfront_poses_.push_back( new_pose );
		}

		//now try to combine pareto opt mutations
		for( Size iseq = 2; iseq <= seqpos_aa_vals_vec_.size(); ++iseq ){
			//create new pose vec to hold all combinations
			vector1< pose::Pose > new_poses;
			vector1< vector1< Real > > new_poses_filter_vals;
			//the resi index is the first part of the pair
			Size resi( seqpos_aa_vals_vec_[ iseq ].first );
			TR << "Combining " << pfront_poses_.size() << " pareto opt poses with mutations at residue " << resi << std::endl;
			//over each current pfront pose
			for( Size ipose = 1; ipose <= pfront_poses_.size(); ++ipose ){
				//over all aa's at seqpos
				for( Size iaa = 1; iaa <= seqpos_aa_vals_vec_[ iseq ].second.size(); ++iaa ){
					AA target_aa( seqpos_aa_vals_vec_[ iseq ].second[ iaa ].first );
					pose::Pose new_pose( pfront_poses_[ ipose ] );

					bool filter_pass;
					vector1< Real > vals;
					ptmut_calc->mutate_and_relax( new_pose, resi, target_aa );
					ptmut_calc->eval_filters( new_pose, filter_pass, vals );
					//only check this guy for pareto if passes
					if( !filter_pass ) continue;
					new_poses.push_back( new_pose );
					new_poses_filter_vals.push_back( vals );
				}
			}
			TR << "Filtering " << new_poses.size() << " new poses from mutations at residue " << resi << std::endl;
			//filter new_poses for the pareto opt set
			filter_pareto_opt_poses( new_poses, new_poses_filter_vals );
			//and reset poses
			pfront_poses_ = new_poses;

			//TODO: how eval stopping cond w/ multiple poses?
			// stop optimizing a given pose if it meets condition, but let others keep going?
		}
	}
	//assign the apply pose to the next pfront_pose 
	pose = pfront_poses_[ pfront_pose_iter_ ];
	//increment pose iterator to get new pfront_pose at nstruct+1
	pfront_pose_iter_ += 1;
	//loop back if we go through them all
	if( pfront_pose_iter_ > pfront_poses_.size() ) pfront_pose_iter_ = 1;
}


void
ParetoOptMutationMover::add_filter( protocols::filters::FilterOP filter, std::string const sample_type )
{
  filters_.push_back( filter );
  sample_types_.push_back( sample_type );
}

//parse rosetta scripts tags
void
ParetoOptMutationMover::parse_my_tag( utility::tag::TagPtr const tag,
		protocols::moves::DataMap & data,
		protocols::filters::Filters_map const &filters,
		protocols::moves::Movers_map const & movers,
		core::pose::Pose const & )
{
	TR << "ParetoOptMutationMover"<<std::endl;
	task_factory( protocols::rosetta_scripts::parse_task_operations( tag, data ) );
	//load relax mover
	std::string const relax_mover_name( tag->getOption< std::string >( "relax_mover", "null" ) );
	protocols::moves::Movers_map::const_iterator mover_it( movers.find( relax_mover_name ) );
	if( mover_it == movers.end() )
		utility_exit_with_message( "Relax mover "+relax_mover_name+" not found" );
	relax_mover( mover_it->second );
	//load diversify_lvl
	diversify_lvl( tag->getOption< core::Size >( "diversify_lvl", core::Size( 1 ) ) );
	//load scorefxn
	scorefxn( protocols::rosetta_scripts::parse_score_function( tag, data ) );
	//load dump_pdb
	dump_pdb( tag->getOption< bool >( "dump_pdb", false ) );
	if( tag->hasOption( "stopping_condition" ) ){
		std::string const stopping_filter_name( tag->getOption< std::string >( "stopping_condition" ) );
		stopping_condition( protocols::rosetta_scripts::parse_filter( stopping_filter_name, filters ) );
		TR<<"Defined stopping condition "<<stopping_filter_name<<std::endl;
	}

	/*
	//load sample_type -- No! no point in having single-filter for pareto optimization
	sample_type( tag->getOption< std::string >( "sample_type", "low" ) );
	//load single filter
	{
		std::string const filter_name( tag->getOption< std::string >( "filter", "true_filter" ) );
		protocols::filters::Filters_map::const_iterator find_filt( filters.find( filter_name ) );
		if( find_filt == filters.end() )
			utility_exit_with_message( "Filter "+filter_name+" not found" );
		add_filter( find_filt->second->clone(), sample_type_ );
	}
	*/

	//load multiple filters
  utility::vector1< utility::tag::TagPtr > const branch_tags( tag->getTags() );
  foreach( utility::tag::TagPtr const btag, branch_tags ){
    if( btag->getName() == "Filters" ){
      utility::vector1< utility::tag::TagPtr > const filters_tags( btag->getTags() );
      foreach( utility::tag::TagPtr const ftag, filters_tags ){
        std::string const filter_name( ftag->getOption< std::string >( "filter_name" ) );
        Filters_map::const_iterator find_filt( filters.find( filter_name ));
        if( find_filt == filters.end() ) {
          TR.Error << "Error !! filter not found in map: \n" << tag << std::endl;
          runtime_assert( find_filt != filters.end() );
        }
        std::string const samp_type( ftag->getOption< std::string >( "sample_type", "low" ));
        add_filter( find_filt->second, samp_type );
      } //foreach ftag
    }// fi Filters
    else
      utility_exit_with_message( "tag name " + btag->getName() + " unrecognized." );
  }//foreach btag



}


} // moves
} // protocols
