// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/flxbb/filters/HighestEnergyRegion.cc
/// @brief Design residue patches with worst energy
/// @author Tom Linsky (tlinsky@uw.edu)

// unit headers
#include <devel/denovo_design/HighestEnergyRegion.hh>
#include <devel/denovo_design/HighestEnergyRegionCreator.hh>

// package headers
#include <protocols/denovo_design/filters/PsiPredInterface.hh>
#include <protocols/flxbb/utility.hh>

// project headers
#include <core/conformation/Residue.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/pack/task/operation/TaskOperation.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pose/metrics/CalculatorFactory.hh>
#include <core/pose/Pose.hh>
#include <protocols/jd2/parser/BluePrint.hh>
#include <protocols/moves/DataMap.hh>
#include <protocols/simple_moves/MutateResidue.hh>
#include <protocols/toolbox/SelectResiduesByLayer.hh>
#include <protocols/toolbox/match_enzdes_util/util_functions.hh>
#include <protocols/toolbox/pose_metric_calculators/PackstatCalculator.hh>
#include <protocols/toolbox/task_operations/DesignAroundOperation.hh>

// utility headers
#include <basic/MetricValue.hh>
#include <basic/Tracer.hh>
#include <numeric/random/random.hh>
#include <utility/tag/Tag.hh>

static basic::Tracer TR( "protocols.flxbb.filters.HighestEnergyRegion" );

namespace protocols {
namespace flxbb {
namespace filters {

// default constructor
HighestEnergyRegionOperation::HighestEnergyRegionOperation()
	: region_shell_( 8.0 ),
		regions_to_design_( 1 ),
		repack_non_selected_( false ),
		use_cache_( false ),
		scorefxn_( NULL ),
		cached_pose_( NULL )
{}

// copy constructor
HighestEnergyRegionOperation::HighestEnergyRegionOperation( HighestEnergyRegionOperation const & rval )
	: region_shell_( rval.region_shell_ ),
		regions_to_design_( rval.regions_to_design_ ),
		repack_non_selected_( rval.repack_non_selected_ ),
		use_cache_( rval.use_cache_ ),
		scorefxn_( rval.scorefxn_ ),
		residues_to_design_( rval.residues_to_design_ ),
		cached_pose_( rval.cached_pose_ )
{}

// destructor
HighestEnergyRegionOperation::~HighestEnergyRegionOperation()
{}

/// @brief make clone
core::pack::task::operation::TaskOperationOP
HighestEnergyRegionOperation::clone() const {
  return new HighestEnergyRegionOperation( *this );
}

/// @brief utility function that compares two resid-probability pairs and returns true of the probability of the first is greater than probability of the second
bool compare_prob_energy( std::pair< core::Size, core::Real > const & p1,
									 std::pair< core::Size, core::Real > const & p2 ) {
	return ( p1.second > p2.second );
}

/// @brief same as teh function above, only for packstat
bool compare_pack_energy( std::pair< core::Size, core::Real > const & p1,
													std::pair< core::Size, core::Real > const & p2 ) {
	return ( p1.second <= p2.second );
}

/// @brief apply
void
HighestEnergyRegionOperation::apply( Pose const & pose, core::pack::task::PackerTask & task ) const
{
 	utility::vector1< core::Size > res_list( residues_to_design( pose ) );

  // now we can just apply a DesignAround operation using the residues that don't match
  protocols::toolbox::task_operations::DesignAroundOperation design_around;
	runtime_assert( region_shell_ >= -0.000001 );
  design_around.design_shell( region_shell_ );
	// if we want to repack the whole protein, a huge repack shell is specified.
	if ( repack_non_selected_ ) {
		design_around.repack_shell( 1000.0 );
	} else {
		design_around.repack_shell( region_shell_ );
	}

  TR << "Residues to design are: ";
  for ( core::Size i=1; i<=res_list.size(); i++) {
    TR << res_list[i] << " ";
    design_around.include_residue( res_list[i] );
  }
  TR << std::endl;
  design_around.apply( pose, task );
}

void
HighestEnergyRegionOperation::parse_tag( utility::tag::TagPtr tag )
{
	region_shell_ = tag->getOption< core::Real >( "region_shell", region_shell_ );
	regions_to_design_ = tag->getOption< core::Size >( "regions_to_design", regions_to_design_ );
	repack_non_selected_ = tag->getOption< core::Size >( "repack_non_selected", repack_non_selected_ );
	/*std::string const scorefxn_name( tag->getOption< std::string >( "scorefxn", "score12" ) );
	scorefxn_ = new core::scoring::ScoreFunction();
	scorefxn_->initialize_from_file("sp2_correction.wts");
	scorefxn_->set_weight( core::scoring::hack_elec, 0.5 );
	scorefxn_->set_weight( core::scoring::atom_pair_constraint, 1.0 );
	scorefxn_->set_weight( core::scoring::angle_constraint, 1.0 );
	scorefxn_->set_weight( core::scoring::dihedral_constraint, 1.0 );
	scorefxn_->set_weight( core::scoring::backbone_stub_constraint, 1.0 );
	scorefxn_->set_weight( core::scoring::coordinate_constraint, 1.0 );*/
}

/// @brief Gets a list of residues to design, but uses cached result if present
utility::vector1< core::Size >
HighestEnergyRegionOperation::residues_to_design( core::pose::Pose const & pose ) const
{
	if ( use_cache_ ) {
		return residues_to_design_;
	} else {
		return get_residues_to_design( pose );
	}
}

/// @brief Gets a list of residues for design
utility::vector1< core::Size >
HighestEnergyRegionOperation::get_residues_to_design( core::pose::Pose const & pose ) const
{
	utility::vector1< core::Size > residues_to_design;
	//	TR << "Size of residues_to_design_ = " << residues_to_design_.size() << std::endl;
	if ( use_cache_ && ( residues_to_design_.size() >= 1 ) ) {
		for ( core::Size i=1; i<=residues_to_design_.size(); ++i ) {
			residues_to_design.push_back( residues_to_design_[i] );
		}
	} else {
		TR << "scanning for highest energy region" << std::endl;
		// create a map and insert pairs for residue and energies
		utility::vector1< std::pair< core::Size, core::Real > > res_to_prob;

		// work on a copy of the pose
		core::pose::Pose posecopy( pose );
		runtime_assert( scorefxn_ );
		(*scorefxn_)( posecopy );

		for ( core::Size resi=1; resi<=posecopy.total_residue(); ++resi ) {
			utility::vector1<bool> outside_of_region( posecopy.total_residue(), true );

			protocols::toolbox::task_operations::DesignAroundOperation des_around;
			des_around.design_shell( region_shell_ );
			des_around.repack_shell( region_shell_ );
			des_around.include_residue( resi );
			core::pack::task::PackerTaskOP tmp_task = core::pack::task::TaskFactory::create_packer_task( posecopy );
			des_around.apply( posecopy, *tmp_task );

			// get residue numbers that are being designed
			core::Size residues_in_region( 0 );
			for ( core::Size resj=1; resj<=posecopy.total_residue(); ++resj ) {
				if ( tmp_task->being_designed( resj ) ) {
					outside_of_region[resj] = false;
					++residues_in_region;
				}
			}
			core::Real region_score = scorefxn_->get_sub_score( posecopy, outside_of_region );
			//TR << "Residues found in region of " << region_shell_ << " around res " << resi << " : " << residues_in_region << ", score=" << region_score << " Normalized: " << region_score/residues_in_region <<  std::endl;
			// add score and residue num to list
			res_to_prob.push_back( std::pair< core::Size, core::Real >( resi, region_score ) );
		}
		// sort the map based on the psipred probability
		std::sort( res_to_prob.begin(), res_to_prob.end(), compare_prob_energy );
		//TR << "Top member of sorted probability list is " << res_to_prob[1].second << std::endl;
		for ( core::Size j=1; ( j<=res_to_prob.size() && j<=regions_to_design_ ); ++j ) {
			residues_to_design.push_back( res_to_prob[j].first );
		}
	}
	return residues_to_design;
}

void
HighestEnergyRegionOperation::cache_result( core::pose::Pose const & pose )
{
	utility::vector1< core::Size > res_list( get_residues_to_design( pose ) );
	// clear cache
	residues_to_design_.clear();
	// add new residues to cache
	for ( core::Size i=1; i<=res_list.size(); ++i ) {
		residues_to_design_.push_back( res_list[i] );
	}
	cached_pose_ = new core::pose::Pose( pose );
	TR << "Cached pose and a list of residues of size: " << residues_to_design_.size() << std::endl;
}

void
HighestEnergyRegionOperation::set_scorefxn( core::scoring::ScoreFunctionOP scorefxn )
{
	// clone the scorefunction just in case something happens
	scorefxn_ = scorefxn->clone();
}

core::pack::task::operation::TaskOperationOP
HighestEnergyRegionOperationCreator::create_task_operation() const {
	return new HighestEnergyRegionOperation;
}

std::string
HighestEnergyRegionOperationCreator::keyname() const {
	return "HighestEnergyRegion";
}

/// @brief tells this task operation whether it should use the cache when it is applied (and not determine residues to design again)
void
HighestEnergyRegionOperation::set_use_cache( bool const use_cache ) {
	use_cache_ = use_cache;
}

/// @brief accessor function to access the cached pose, if it exists
core::pose::PoseCOP
HighestEnergyRegionOperation::cached_pose() const {
	return cached_pose_;
}

/// @brief initializes cache of allowed amino acids
void
HighestEnergyRegionOperation::initialize_aa_cache( core::pose::Pose const & pose )
{
	allowed_aas_.clear();
	for ( core::Size i=1; i<=pose.total_residue(); ++i ) {
		utility::vector1< bool > allowed_aas( core::chemical::num_canonical_aas, true );
		allowed_aas_.push_back( allowed_aas );
	}
}

/// @brief Gets list of allowed amino acids at position resi
utility::vector1< bool > const &
HighestEnergyRegionOperation::allowed_aas( core::Size const resi ) const
{
	runtime_assert( resi <= allowed_aas_.size() );
	runtime_assert( resi > 0 );
	return allowed_aas_[resi];
}

/// @brief Sets amino acid aa as disallowed at position resi
void
HighestEnergyRegionOperation::disallow_aa( core::Size const resi, char const aa )
{
	runtime_assert( resi <= allowed_aas_.size() );
	runtime_assert( resi > 0 );
	allowed_aas_[resi][core::chemical::aa_from_oneletter_code( aa )] = false;
}

/// @brief set the number of regions to be designed
void
HighestEnergyRegionOperation::set_regions_to_design( core::Size const num_regions )
{
	regions_to_design_ = num_regions;
}

/// @brief Returns the number of regions selected for design
core::Size
HighestEnergyRegionOperation::regions_to_design() const
{
	return regions_to_design_;
}

/// @brief set the shell size for the regions to be examined and designed
void
HighestEnergyRegionOperation::set_region_shell( core::Real const region_shell )
{
	region_shell_ = region_shell;
}

/// @brief accessor for regions shell size
core::Real
HighestEnergyRegionOperation::region_shell() const
{
	return region_shell_;
}

/// @brief mutator for repack_non_selected -- sets behavior for whether we should repack or fix areas outside the design region
void
HighestEnergyRegionOperation::repack_non_selected( bool const repack_non_selected )
{
	repack_non_selected_ = repack_non_selected;
}

/// @brief accessor for repack_non_selected member var
bool
HighestEnergyRegionOperation::repack_non_selected() const
{
	return repack_non_selected_;
}

////////////////////////////////////////////////////////////////////////////////
/// DesignByPackStat
////////////////////////////////////////////////////////////////////////////////

/// @brief default constructor
DesignByPackStatOperation::DesignByPackStatOperation() :
	HighestEnergyRegionOperation()
{}

/// @brief copy constructor
DesignByPackStatOperation::DesignByPackStatOperation( DesignByPackStatOperation const & rval ) :
	HighestEnergyRegionOperation( rval )
{}

/// @brief destructor
DesignByPackStatOperation::~DesignByPackStatOperation()
{}

/// @brief make clone
core::pack::task::operation::TaskOperationOP
DesignByPackStatOperation::clone() const
{
	return new DesignByPackStatOperation( *this );
}

/// @brief Gets a list of residues for design
utility::vector1< core::Size >
DesignByPackStatOperation::get_residues_to_design( core::pose::Pose const & pose ) const
{
	// check for calculator; create if it doesn't exist
	if ( ! core::pose::metrics::CalculatorFactory::Instance().check_calculator_exists( "PackStat" ) ) {
		toolbox::pose_metric_calculators::PackstatCalculator calculator;
		core::pose::metrics::CalculatorFactory::Instance().register_calculator( "PackStat", calculator.clone() );
	}

	basic::MetricValue< utility::vector1< core::Real > > residue_packstat;
	pose.metric( "PackStat", "residue_packstat", residue_packstat );

	utility::vector1< core::Size > residues_to_design;

	utility::vector1< std::pair< core::Size, core::Real > > res_to_score;
	for ( core::Size i=1; i<=residue_packstat.value().size(); ++i ) {
		res_to_score.push_back( std::pair< core::Size, core::Real >( i, residue_packstat.value()[i] ) );
	}

	// compute SASA and exclude surface-accessible residues.
	toolbox::SelectResiduesByLayer srbl( true, true, true );
	srbl.compute( pose, pose.secstruct() );

	// sort the vector based on the psipred probability
	std::sort( res_to_score.begin(), res_to_score.end(), compare_pack_energy );

	for ( core::Size j=1; ( j<=res_to_score.size() &&	j<=regions_to_design() ); ++j ) {
		if ( srbl.rsd_sasa( res_to_score[j].first ) <= 40 ) {
			TR.Debug << "Res=" << res_to_score[j].first << ", Packstat=" << res_to_score[j].second << " (SASA=" << srbl.rsd_sasa( res_to_score[j].first ) << ")" <<std::endl;
			residues_to_design.push_back( res_to_score[j].first );
		} else {
			TR.Debug << "Res=" << res_to_score[j].first << " is surface (SASA=" << srbl.rsd_sasa( res_to_score[j].first ) << ")" << std::endl;
		}
	}

	return residues_to_design;
}

////////////////////////////////////////////////////////////////////////////////
/// DesignRandomRegion
////////////////////////////////////////////////////////////////////////////////

/// @brief default constructor
DesignRandomRegionOperation::DesignRandomRegionOperation() :
	HighestEnergyRegionOperation()
{}

/// @brief destructor
DesignRandomRegionOperation::~DesignRandomRegionOperation()
{}

/// @brief make clone
core::pack::task::operation::TaskOperationOP
DesignRandomRegionOperation::clone() const
{
	return new DesignRandomRegionOperation( *this );
}

/// @brief Gets a list of residues for design
utility::vector1< core::Size >
DesignRandomRegionOperation::get_residues_to_design( core::pose::Pose const & pose ) const
{
	utility::vector1< core::Size > residues_to_design;
	// initialize with residue numbers in order
	for ( core::Size i=1; i<=pose.total_residue(); ++i ) {
		residues_to_design.push_back( i );
	}

	// randomize by swapping the element at each position with a random element from later in the vector
	for ( core::Size i=1; i<=residues_to_design.size(); ++i ) {
		core::Size const target( numeric::random::random_range( i, residues_to_design.size() ) );
		assert( target <= residues_to_design.size() );
		assert( target >= 1 );
		core::Size const target_value( residues_to_design[ target ] );
		residues_to_design[ target ] = residues_to_design[ i ];
		residues_to_design[ i ] = target_value;
	}

	//TR.Debug << "Random array=" << residues_to_design << std::endl;
	return residues_to_design;
}

//namespaces
}
}
}
