// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/pack/interaction_graph/LinearMemoryInteractionGraph.cc
/// @brief
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

#include <core/pack/interaction_graph/LinearMemoryInteractionGraph.hh>

/// Debugging headers
#include <core/pose/Pose.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/conformation/Residue.hh>
// AUTO-REMOVED #include <basic/options/after_opts.hh>
// AUTO-REMOVED #include <basic/options/option.hh>
// AUTO-REMOVED #include <basic/options/keys/packing.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

// Utility headers
#include <utility/excn/Exceptions.hh>

#include <iostream>

#include <utility/vector1.hh>
#include <ObjexxFCL/FArray1A.hh>

//Auto Headers
#include <core/scoring/EnergyGraph.hh>

namespace core {
namespace pack {
namespace interaction_graph {

static thread_local basic::Tracer T( "core.pack.interaction_graph.linmem_ig", basic::t_error );

/// @brief For testing the LinMemIG, you'll want to set this to true
bool const debug = { false };


/// @brief main constructor, no default or copy constructors
LinearMemNode::LinearMemNode(
	InteractionGraphBase * owner,
	int node_id,
	int num_states
) :
	OnTheFlyNode( owner, node_id, num_states ),
	current_state_( 0 ),
	curr_state_one_body_energy_( 0.0f ),
	curr_state_total_energy_( 0.0f ),
	alternate_state_( 0 ),
	alternate_state_one_body_energy_( 0 ),
	alternate_state_total_energy_( 0 ),
	alternate_state_is_being_considered_( false ),
	already_prepped_for_simA_( false ),
	accepted_rejected_substitution_history_( ACCEPTANCE_REJECTION_HISTORY_LENGTH, 0 ),
	accepted_history_head_( 1 ),
	num_recently_accepted_( 0 ),
	filled_substitution_history_( false )
{
	rhq_.num_elements( num_states );
}

LinearMemNode::~LinearMemNode()
{}

void
LinearMemNode::prepare_for_simulated_annealing()
{
	if (! get_edge_vector_up_to_date() ) update_internal_vectors();
	// mark out-of-date anything where coordinates have changed
	for (int ii = 1; ii <= get_num_states(); ++ii)
	{
		if ( already_prepped_for_simA_ /*&& ! get_coordinates_current( ii )*/ )
		{
			for (int jj = 1; jj <= get_num_incident_edges(); ++jj)
			{
				get_incident_linmem_edge(jj)->reset_state_energies(
					get_node_index(),
					ii,
					rhq_.pos_in_history_queue( ii )
				);
			}
		}
		//mark_coordinates_current( ii );
	}
	already_prepped_for_simA_ = true;

	num_recently_accepted_ =  0;
	filled_substitution_history_ = false;
	accepted_history_head_ = 1;

	return;
}

void
LinearMemNode::print() const
{

	T << "LinearMemNode " << get_node_index() << " with " << get_num_states() << " states" << std::endl;
	T << "curr_state " << current_state_ << " ";
	T << "curr_state_sparse_mat_info_ ";
	T << curr_state_sparse_mat_info_.get_aa_type() << " ";
	T << curr_state_sparse_mat_info_.get_state_ind_for_this_aa_type() << " ";
	T << "Curr One Body Energy: " << curr_state_one_body_energy_ << std::endl;
	T << "Curr Two Body Energies:";
	for (int ii = 1; ii <= get_num_incident_edges(); ++ii)
	{
		T << " " << get_index_of_adjacent_node(ii) << ":" << curr_state_two_body_energies_[ ii ];
	}
	T << std::endl;

	if ( ! alternate_state_is_being_considered_ ) return;
	T << "Alt One Body Energy: " << alternate_state_one_body_energy_ << std::endl;
	T << "Alt Two Body Energies:";
	for (int ii = 1; ii <= get_num_incident_edges(); ++ii)
	{
		T << " " << get_index_of_adjacent_node(ii) << ":" << alternate_state_two_body_energies_[ ii ];
	}
	T << std::endl  << "-----------------" << std::endl;


}

unsigned int
LinearMemNode::count_static_memory() const
{
	return sizeof( LinearMemNode );
}

unsigned int
LinearMemNode::count_dynamic_memory() const
{
	unsigned int total_memory = OnTheFlyNode::count_dynamic_memory();

	total_memory += rhq_.dynamic_memory_usage();
	total_memory += aa_neighbors_for_edges_.size() * sizeof( unsigned char );
	total_memory += neighbors_curr_state_.size() * sizeof( int );
	total_memory += neighbors_state_recent_history_index_.size() * sizeof( int );
	total_memory += neighbors_curr_state_sparse_info_.size() * sizeof( SparseMatrixIndex );

	total_memory += curr_state_two_body_energies_.size() * sizeof( core::PackerEnergy );
	total_memory += alternate_state_two_body_energies_.size() * sizeof( core::PackerEnergy );

	total_memory += accepted_rejected_substitution_history_.size() * sizeof( int );

	return total_memory;
}


///@brief puts the LinMemNode in the unassigned state
void
LinearMemNode::assign_zero_state()
{
	current_state_ = 0;
	alternate_state_ = 0;
	alternate_state_is_being_considered_ = false;

	curr_state_one_body_energy_ = 0.0f;
	std::fill(
		curr_state_two_body_energies_.begin(),
		curr_state_two_body_energies_.end(),
		0.0f);
	curr_state_total_energy_ = 0.0f;

	for (int ii = 1; ii <= get_num_incident_edges(); ++ii )
	{
		get_incident_linmem_edge(ii)->
			acknowledge_state_zeroed( get_node_index() );
	}

	return;
}


////@brief assigns a new state to the Node
void
LinearMemNode::assign_state(int new_state)
{
debug_assert( new_state >= 0 && new_state <= get_num_states());

	if (new_state == 0) assign_zero_state();
	else
	{
		//T << "assign_state: node -  " << get_node_index() << " new state " << new_state << "...";
		current_state_ = new_state;
		curr_state_sparse_mat_info_ =
			get_sparse_mat_info_for_state( current_state_ );
		curr_state_one_body_energy_ = get_one_body_energy( current_state_ );
		curr_state_total_energy_ = curr_state_one_body_energy_;
		alternate_state_is_being_considered_ = false;
		int bumped_recent_history_index = update_recent_history( current_state_ );

		for (int ii = 1; ii <= get_num_incident_edges(); ++ii )
		{
			get_incident_linmem_edge(ii)->acknowledge_state_change(
				get_node_index(),
				current_state_,
				curr_state_sparse_mat_info_,
				bumped_recent_history_index,
				rhq_.head_of_queue(),
				curr_state_two_body_energies_[ii]
			);

			curr_state_total_energy_ += curr_state_two_body_energies_[ ii ];
		}
		//T<< "..done" << std::endl;
	}
	if ( debug ) {
		get_on_the_fly_owner()->non_const_pose().replace_residue( get_rotamer(current_state_).seqpos(), get_rotamer( current_state_ ), false );
		get_on_the_fly_owner()->score_function()( get_on_the_fly_owner()->non_const_pose() );
	}
	return;
}


void
LinearMemNode::partial_assign_state( int new_state )
{
	if (new_state == 0 )
	{
		assign_zero_state();
		return;
	}

	current_state_ = new_state;
	curr_state_sparse_mat_info_ =
		get_sparse_mat_info_for_state( current_state_ );
	int bumped_recent_history_index = update_recent_history( current_state_ );

	for (int ii = 1; ii <= get_num_incident_edges(); ++ii )
	{
		get_incident_linmem_edge(ii)->acknowledge_partial_state_change(
			get_node_index(),
			current_state_,
			curr_state_sparse_mat_info_,
			bumped_recent_history_index,
			rhq_.head_of_queue()
		);
	}
	alternate_state_is_being_considered_ = false;
}


void LinearMemNode::complete_state_assignment()
{
	if ( current_state_ == 0 ) return;

	curr_state_total_energy_ = curr_state_one_body_energy_ =
		get_one_body_energy( current_state_ );
	for (int ii = 1; ii <= get_num_incident_edges(); ++ii)
	{
		curr_state_two_body_energies_[ ii ] =
			get_incident_linmem_edge( ii )->
			get_energy_following_partial_state_assignment();
		curr_state_total_energy_ += curr_state_two_body_energies_[ ii ];
	}
}


core::PackerEnergy
LinearMemNode::project_deltaE_for_substitution
(
	int alternate_state,
	core::PackerEnergy & prev_node_energy
)
{
	alternate_state_is_being_considered_ = true;
	//procrastinated_ = false;
	//T << "proj_deltaE: node -  " << get_node_index()
	// << " alt state " << alternate_state << "...";

	alternate_state_ = alternate_state;
	int alternate_state_recent_history_index = rhq_.pos_in_history_queue( alternate_state_ );

	bool store_rpes = num_recently_accepted_ < THRESHOLD_ACCEPTANCE_RATE_FOR_RPE_STORAGE;

	alt_state_sparse_mat_info_ = get_sparse_mat_info_for_state( alternate_state );
	alternate_state_one_body_energy_ = get_one_body_energy( alternate_state );
	alternate_state_total_energy_ = alternate_state_one_body_energy_;
	prev_node_energy = curr_state_total_energy_;

	int aa_neighb_linear_index_offset = aa_neighbors_for_edges_.
		index(1, 1, alt_state_sparse_mat_info_.get_aa_type() ) - 1;

	for (int ii = 1; ii <= get_num_incident_edges();
		++ii, aa_neighb_linear_index_offset += get_num_aa_types())
	{

		if ( neighbors_curr_state_[ ii ] != 0 &&
			aa_neighbors_for_edges_[ aa_neighb_linear_index_offset
				+ neighbors_curr_state_sparse_info_[ ii ].get_aa_type() ] ) {

			alternate_state_two_body_energies_[ ii ] =
				get_incident_linmem_edge( ii )->get_energy_for_alt_state(
					store_rpes,
					get_node_index(),
					alternate_state_,
					alternate_state_recent_history_index,
					neighbors_curr_state_[ ii ],
					neighbors_state_recent_history_index_[ ii ]
			);

		} else {
			alternate_state_two_body_energies_[ ii ] = 0;
		}

		alternate_state_total_energy_ += alternate_state_two_body_energies_[ ii ];
	}

	if ( debug && ! get_owner()->any_vertex_state_unassigned() ) {
	debug_assert( get_rotamer(alternate_state_).seqpos() == get_rotamer(current_state_).seqpos() );
		get_on_the_fly_owner()->non_const_pose().replace_residue( get_rotamer(alternate_state_).seqpos(), get_rotamer( alternate_state_ ), false);
		Real score_after = get_on_the_fly_owner()->score_function()( get_on_the_fly_owner()->non_const_pose() );
		/// Now handled automatically.  get_on_the_fly_owner()->score_function().accumulate_residue_total_energies( get_on_the_fly_owner()->non_const_pose() );
		Real rep_after = get_on_the_fly_owner()->pose().energies().residue_total_energies( get_rotamer(alternate_state_).seqpos() )[ scoring::fa_rep ];

		get_on_the_fly_owner()->non_const_pose().replace_residue( get_rotamer(current_state_).seqpos(), get_rotamer( current_state_ ), false);
		Real score_before = get_on_the_fly_owner()->score_function()( get_on_the_fly_owner()->non_const_pose() );
		/// Now handled automatically.  get_on_the_fly_owner()->score_function().accumulate_residue_total_energies( get_on_the_fly_owner()->non_const_pose() );
		Real rep_before = get_on_the_fly_owner()->pose().energies().residue_total_energies( get_rotamer(alternate_state_).seqpos() )[ scoring::fa_rep ];

		Real actual_score_delta = score_after - score_before;
		Real projected_score_delta = alternate_state_total_energy_ - curr_state_total_energy_;
		Real delta_delta = actual_score_delta - projected_score_delta;

		if ( (std::abs( delta_delta ) > 0.001 && std::abs( delta_delta / score_after ) > 10E-5) &&
			(rep_after < 4 && rep_before < 4) ) {

			T << "Score before: " << score_before << " Score after " << score_after << " delta: " << actual_score_delta;
			T << " projected delta: " << projected_score_delta << " delta delta: " << delta_delta << " rep: " << rep_before << " " << rep_after <<  std::endl;


			/// LOOK AT CURRENT ENERGIES
			Size const seqpos( get_rotamer(alternate_state_).seqpos() );
			T << "Problem rotamer substitution at " << seqpos << ": from " << get_rotamer( current_state_).name() << " to " << get_rotamer(alternate_state_).name() << std::endl;
			T << "CURR One body energies: ";
			T << get_on_the_fly_owner()->score_function().weights().dot( get_on_the_fly_owner()->pose().energies().onebody_energies( get_rotamer(alternate_state_).seqpos() ) ) << std::endl;
			T << "internal one body energies: " << curr_state_one_body_energy_ << std::endl;
			T << "location: curr_state_one_body_energy_  " << & curr_state_one_body_energy_ << std::endl;

			{ //scope
			scoring::EnergyGraph const & energygraph = get_on_the_fly_owner()->pose().energies().energy_graph();
			for ( core::graph::Graph::EdgeListConstIter
					iter = energygraph.get_node( seqpos )->const_edge_list_begin(),
					iter_end = energygraph.get_node( seqpos)->const_edge_list_end();
					iter != iter_end; ++iter ) {
				bool corresponding_edge_found_in_ig( false );
				scoring::EnergyMap const tbemap( (static_cast< scoring::EnergyEdge const * > (*iter))->fill_energy_map() );
				Size const other_node_index = (*iter)->get_other_ind( seqpos );
				Real const real_energy = get_on_the_fly_owner()->score_function().weights().dot( tbemap );
				for ( Size ii = 1; ii <= (Size) get_num_incident_edges(); ++ii ) {
					if ( (Size) get_index_of_adjacent_node( ii ) != other_node_index ) continue;
					corresponding_edge_found_in_ig = true;
					if ( std::abs( real_energy - curr_state_two_body_energies_[ ii ]) > 0.001 ) {
						T << "Other residue: " << get_adjacent_linmem_node( ii )->get_rotamer( neighbors_curr_state_[ ii ]).name() << std::endl;
						T << "CURR Real score: edge to " << other_node_index << " energy: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;
						T << "CURR Predicted score: edge to " << get_index_of_adjacent_node( ii ) << " energy: " << curr_state_two_body_energies_[ ii ] << std::endl;
						T << "CURR Real - Predicted: " << real_energy - curr_state_two_body_energies_[ ii ] << std::endl;

						tbemap.show_nonzero( T );
						T << std::endl;

						int const this_aa( curr_state_sparse_mat_info_.get_aa_type());
						int const other_aa( neighbors_curr_state_sparse_info_[ii].get_aa_type() );
						T << "Sparse matrix info: (this,other): " ;
						T << (int)  get_incident_linmem_edge( ii )->get_sparse_aa_neighbor_info()( this_aa, other_aa );
						T << " (other,this): ";
						T << (int) get_incident_linmem_edge( ii )->get_sparse_aa_neighbor_info()( other_aa, this_aa ) << std::endl;

						core::PackerEnergy recomputed = compute_rotamer_pair_energy( ii, current_state_, neighbors_curr_state_[ ii ] );
						T << "Recomputed energy: " << recomputed << std::endl;

						scoring::EnergyMap tbemap;
						get_on_the_fly_owner()->score_function().eval_ci_2b(
							get_on_the_fly_owner()->pose().residue( get_node_index() ),
							get_on_the_fly_owner()->pose().residue( other_node_index ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						get_on_the_fly_owner()->score_function().eval_cd_2b(
							get_on_the_fly_owner()->pose().residue( get_node_index() ),
							get_on_the_fly_owner()->pose().residue( other_node_index ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						T << "Rescored from pose: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;

						tbemap.zero();
						get_on_the_fly_owner()->score_function().eval_ci_2b(
							get_rotamer( current_state_ ),
							get_on_the_fly_owner()->pose().residue( other_node_index ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						get_on_the_fly_owner()->score_function().eval_cd_2b(
							get_rotamer( current_state_ ),
							get_on_the_fly_owner()->pose().residue( other_node_index ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						T << "Rescored combo: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;


						tbemap.zero();
						get_on_the_fly_owner()->score_function().eval_ci_2b(
							get_adjacent_linmem_node( ii )->get_rotamer( neighbors_curr_state_[ ii ]),
							get_on_the_fly_owner()->pose().residue( get_node_index() ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						get_on_the_fly_owner()->score_function().eval_cd_2b(
							get_adjacent_linmem_node( ii )->get_rotamer( neighbors_curr_state_[ ii ]),
							get_on_the_fly_owner()->pose().residue( get_node_index() ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						T << "Rescored combo 2: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;


						/// Check if order dependence is causing a bug -- res1 and res2 should not have to be ordered in the
						/// residue pair energy calls
						tbemap.zero();
						get_on_the_fly_owner()->score_function().eval_ci_2b(
							get_on_the_fly_owner()->pose().residue( other_node_index ),
							get_rotamer( current_state_ ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						get_on_the_fly_owner()->score_function().eval_cd_2b(
							get_on_the_fly_owner()->pose().residue( other_node_index ),
							get_rotamer( current_state_ ),
							get_on_the_fly_owner()->pose(),
							tbemap );
						T << "Rescored combo swapped: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;

						/// Check if order dependence is causing a bug -- res1 and res2 should not have to be ordered in the
						/// residue pair energy calls
						tbemap.zero();
						get_on_the_fly_owner()->score_function().eval_ci_2b(
							get_on_the_fly_owner()->pose().residue( get_node_index() ),
							get_adjacent_linmem_node( ii )->get_rotamer( neighbors_curr_state_[ ii ]),
							get_on_the_fly_owner()->pose(),
							tbemap );
						get_on_the_fly_owner()->score_function().eval_cd_2b(
							get_on_the_fly_owner()->pose().residue( get_node_index() ),
							get_adjacent_linmem_node( ii )->get_rotamer( neighbors_curr_state_[ ii ]),
							get_on_the_fly_owner()->pose(),
							tbemap );
						T << "Rescored combo 2 swapped: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;

						//These references are useful in GDB if you need to debug.
						//
						//conformation::Residue const & res_in_pose = get_on_the_fly_owner()->pose().residue( get_node_index() );
						//conformation::Residue const & res_on_node = get_rotamer( current_state_ );
						//conformation::Residue const & other_res_in_pose = get_on_the_fly_owner()->pose().residue( other_node_index );
						//conformation::Residue const & other_res_on_node = get_adjacent_linmem_node( ii )->get_rotamer( neighbors_curr_state_[ ii ]);
						//LinearMemNode * neighbor = get_adjacent_linmem_node( ii );

						break;
					}
					if ( !corresponding_edge_found_in_ig ) {
						T << "Did not find edge in energy map to " << other_node_index << " with energy " << real_energy << " in the interaction graph!" << std::endl;
					}
				}
			}
			}// end scope

			/// Look at interaction graph edges that are absent from the energy graph
			{ //scope
			scoring::EnergyGraph const & energygraph = get_on_the_fly_owner()->pose().energies().energy_graph();
			for ( Size ii = 1; ii <= (Size) get_num_incident_edges(); ++ii ) {
				Size const other_node_index = (Size) get_index_of_adjacent_node( ii );
				if ( curr_state_two_body_energies_[ ii ] == 0 ) continue;
				bool found_similar_edge( false );

				for ( core::graph::Graph::EdgeListConstIter
						iter = energygraph.get_node( seqpos )->const_edge_list_begin(),
						iter_end = energygraph.get_node( seqpos)->const_edge_list_end();
						iter != iter_end; ++iter ) {
					if ( other_node_index != (Size) (*iter)->get_other_ind( seqpos ) ) continue;
					found_similar_edge = true;

					//scoring::EnergyMap const & tbemap( (static_cast< scoring::EnergyEdge const * > (*iter))->energy_map() );
					//Real const real_energy = get_on_the_fly_owner()->score_function().weights().dot( tbemap );

				}
				if ( ! found_similar_edge ) {
					T << "Edge in lmig CUR to node " << other_node_index << " with energy: " << curr_state_two_body_energies_[ ii ] << " absent from energy graph!" << std::endl;
				}
			}
			} // end scope

			/// Place the alternate rotamer on the pose and rescore.
			get_on_the_fly_owner()->non_const_pose().replace_residue( get_rotamer(alternate_state_).seqpos(), get_rotamer( alternate_state_ ), false);
			get_on_the_fly_owner()->score_function()( get_on_the_fly_owner()->non_const_pose() );

			T << "ALT One body energies: ";
			T << get_on_the_fly_owner()->score_function().weights().dot( get_on_the_fly_owner()->pose().energies().onebody_energies( get_rotamer(alternate_state_).seqpos() ) ) << std::endl;
			T << "internal one body energies: " << alternate_state_one_body_energy_ << std::endl;
			T << "location: alternate_state_one_body_energy_  " << & alternate_state_one_body_energy_ << std::endl;


			{ //scope
			scoring::EnergyGraph const & energygraph = get_on_the_fly_owner()->pose().energies().energy_graph();
			for ( core::graph::Graph::EdgeListConstIter
					iter = energygraph.get_node( seqpos )->const_edge_list_begin(),
					iter_end = energygraph.get_node( seqpos)->const_edge_list_end();
					iter != iter_end; ++iter ) {
				bool corresponding_edge_found_in_ig( false );
				scoring::EnergyMap const tbemap( (static_cast< scoring::EnergyEdge const * > (*iter))->fill_energy_map() );
				Size const other_node_index = (*iter)->get_other_ind( seqpos );
				Real const real_energy = get_on_the_fly_owner()->score_function().weights().dot( tbemap );
				for ( Size ii = 1; ii <= (Size) get_num_incident_edges(); ++ii ) {
					if ( (Size) get_index_of_adjacent_node( ii ) != other_node_index ) continue;
					corresponding_edge_found_in_ig = true;
					if ( std::abs( real_energy - alternate_state_two_body_energies_[ ii ]) > 0.001 ) {
						T << "ALT Real score: edge to " << other_node_index << " energy: " << get_on_the_fly_owner()->score_function().weights().dot( tbemap ) << std::endl;
						T << "ALT Predicted score: edge to " << get_index_of_adjacent_node( ii ) << " energy: " << alternate_state_two_body_energies_[ ii ] << std::endl;
						T << "ALT Real - Predicted: " << real_energy - alternate_state_two_body_energies_[ ii ] << std::endl;
						tbemap.show_nonzero( T );
						T << std::endl;

						int const this_aa( alt_state_sparse_mat_info_.get_aa_type());
						int const other_aa( neighbors_curr_state_sparse_info_[ii].get_aa_type() );
						T << "Sparse matrix info: (this,other): " ;
						T << (int) get_incident_linmem_edge( ii )->get_sparse_aa_neighbor_info()( this_aa, other_aa );
						T << " (other,this): ";
						T << (int) get_incident_linmem_edge( ii )->get_sparse_aa_neighbor_info()( other_aa, this_aa ) << std::endl;

						core::PackerEnergy recomputed = compute_rotamer_pair_energy( ii, alternate_state_, neighbors_curr_state_[ ii ] );
						T << "Recomputed energy: " << recomputed << std::endl;

						break;
					}
					if ( !corresponding_edge_found_in_ig ) {
						T << "Did not find edge in energy map to " << other_node_index << " with energy " << real_energy << " in the interaction graph!" << std::endl;
					}

				}
			}
			}// end scope
			/// Look at interaction graph edges that are absent from the energy graph
			{ //scope
			scoring::EnergyGraph const & energygraph = get_on_the_fly_owner()->pose().energies().energy_graph();
			for ( Size ii = 1; ii <= (Size) get_num_incident_edges(); ++ii ) {
				Size const other_node_index = (Size) get_index_of_adjacent_node( ii );
				if ( alternate_state_two_body_energies_[ ii ] == 0 ) continue;
				bool found_similar_edge( false );

				for ( core::graph::Graph::EdgeListConstIter
						iter = energygraph.get_node( seqpos )->const_edge_list_begin(),
						iter_end = energygraph.get_node( seqpos)->const_edge_list_end();
						iter != iter_end; ++iter ) {
					if ( other_node_index != (Size) (*iter)->get_other_ind( seqpos ) ) continue;
					found_similar_edge = true;

					//scoring::EnergyMap const & tbemap( (static_cast< scoring::EnergyEdge const * > (*iter))->energy_map() );
					//Real const real_energy = get_on_the_fly_owner()->score_function().weights().dot( tbemap );

				}
				if ( ! found_similar_edge ) {
					T << "Edge in lmig ALT to node " << other_node_index << " with energy: " << alternate_state_two_body_energies_[ ii ] << " absent from energy graph!" << std::endl;
				}
			}
			} // end scope


			get_on_the_fly_owner()->non_const_pose().replace_residue( get_rotamer(alternate_state_).seqpos(), get_rotamer( current_state_ ), false);
			get_on_the_fly_owner()->score_function()( get_on_the_fly_owner()->non_const_pose() );
		}
	} // end debug

	return alternate_state_total_energy_ - curr_state_total_energy_;

}


///@brief commits the last substitution that was considered by this Node
void
LinearMemNode::commit_considered_substitution()
{
debug_assert( alternate_state_is_being_considered_ );

	current_state_ = alternate_state_;
	curr_state_sparse_mat_info_ = alt_state_sparse_mat_info_;
	curr_state_one_body_energy_ = alternate_state_one_body_energy_;
	curr_state_total_energy_ = alternate_state_total_energy_;

	//copies from [1] to end
	//utility::vector1< core::PackerEnergy >::iterator alt_position1 = alternate_state_two_body_energies_.begin();
	//utility::vector1< core::PackerEnergy >::iterator curr_position1 = curr_state_two_body_energies_.begin();

	std::copy( alternate_state_two_body_energies_.begin(),
		alternate_state_two_body_energies_.end(),
		curr_state_two_body_energies_.begin() );

	int bumped_recent_history_index = update_recent_history( current_state_ );

	for ( int ii = 1; ii <= get_num_incident_edges(); ++ii )
	{
		get_incident_linmem_edge(ii)->acknowledge_substitution(
			get_node_index(),
			curr_state_two_body_energies_[ ii ],
			current_state_,
			curr_state_sparse_mat_info_,
			bumped_recent_history_index,
			rhq_.head_of_queue(),
			neighbors_curr_state_[ ii ]
		);
	}

	alternate_state_is_being_considered_ = false;

	++accepted_history_head_;
	if (accepted_history_head_ > ACCEPTANCE_REJECTION_HISTORY_LENGTH )
	{
		accepted_history_head_ = 1;
		filled_substitution_history_ = true;
	}
	if ( ! filled_substitution_history_ ||
		accepted_rejected_substitution_history_( accepted_history_head_ ) == REJECTED )
	{
		++num_recently_accepted_;
	}
	accepted_rejected_substitution_history_( accepted_history_head_ ) = ACCEPTED;

	if ( debug ) {
		get_on_the_fly_owner()->non_const_pose().replace_residue( get_rotamer(current_state_).seqpos(), get_rotamer( current_state_ ), false );
		get_on_the_fly_owner()->score_function()( get_on_the_fly_owner()->non_const_pose() );
	}

	return;
}

void
LinearMemNode::acknowledge_last_substititon_not_committed()
{
	alternate_state_is_being_considered_ = false;
	++accepted_history_head_;
	if (accepted_history_head_ > ACCEPTANCE_REJECTION_HISTORY_LENGTH )
	{
		accepted_history_head_ = 1;
		filled_substitution_history_ = true;
	}
	if ( filled_substitution_history_ &&
		accepted_rejected_substitution_history_( accepted_history_head_ ) == ACCEPTED )
	{
		--num_recently_accepted_;
	}
	accepted_rejected_substitution_history_( accepted_history_head_ ) = REJECTED;
}

core::PackerEnergy
LinearMemNode::compute_pair_energy_for_current_state(
	int edge_making_energy_request
)
{
	if ( aa_neighbors_for_edges_(
			neighbors_curr_state_sparse_info_[ edge_making_energy_request ].get_aa_type(),
			edge_making_energy_request,
			curr_state_sparse_mat_info_.get_aa_type() ) ) {
		return compute_rotamer_pair_energy(
			edge_making_energy_request,
			current_state_,
			neighbors_curr_state_[ edge_making_energy_request ]
		);
	} else {
		return 0;
	}

}

core::PackerEnergy
LinearMemNode::compute_pair_energy_for_alternate_state(
	int edge_making_energy_request
)
{
	return compute_rotamer_pair_energy(
		edge_making_energy_request,
		alternate_state_,
		neighbors_curr_state_[ edge_making_energy_request ] );
}


void
LinearMemNode::acknowledge_neighbors_partial_state_substitution(
	int edge_to_altered_neighbor,
	int other_node_new_state,
	SparseMatrixIndex const & other_node_new_state_sparse_info,
	int other_state_recent_history_index
)
{
	curr_state_total_energy_ = 0;
	curr_state_two_body_energies_[ edge_to_altered_neighbor ] = 0;
	neighbors_curr_state_[ edge_to_altered_neighbor ] = other_node_new_state;
	neighbors_curr_state_sparse_info_[ edge_to_altered_neighbor ]  =
		other_node_new_state_sparse_info;
	neighbors_state_recent_history_index_[ edge_to_altered_neighbor ] =
		other_state_recent_history_index;
}



void LinearMemNode::set_recent_history_size(
	int num_states_to_maintain_in_recent_history
)
{
	rhq_.history_size( num_states_to_maintain_in_recent_history );
}

int
LinearMemNode::get_recent_history_size() const
{
	return rhq_.history_size();
}


void
LinearMemNode::print_internal_energies() const
{
	T << "curr_state " << current_state_ << " ";
	T << "curr_state_sparse_mat_info_ ";
	T << curr_state_sparse_mat_info_.get_aa_type() << " ";
	T << curr_state_sparse_mat_info_.get_state_ind_for_this_aa_type() << " ";
	T << "curr_state_one_body_energy_ ";
	T << curr_state_one_body_energy_ << " ";
	T << "curr_state_total_energy_" << curr_state_total_energy_ << " ";
	for (int ii = 1; ii <= get_num_incident_edges(); ++ii)
	{
		T << "(" << get_index_of_adjacent_node(ii) << ":" << curr_state_two_body_energies_[ ii ] << ") ";
	}
	T << std::endl;
}


void
LinearMemNode::update_internal_energy_sums()
{
debug_assert( get_edge_vector_up_to_date() );
	curr_state_total_energy_ = 0;
	for (int ii = 1; ii <= get_num_incident_edges(); ++ii)
	{
		curr_state_total_energy_ +=
			get_incident_linmem_edge(ii)->get_current_two_body_energy();
	}
	curr_state_total_energy_ += curr_state_one_body_energy_;
	return;
}



void LinearMemNode::update_internal_vectors()
{
	NodeBase::update_edge_vector();
	neighbors_curr_state_.resize( get_num_incident_edges());
	neighbors_curr_state_sparse_info_.resize( get_num_incident_edges());
	neighbors_state_recent_history_index_.resize( get_num_incident_edges() );

	aa_neighbors_for_edges_.dimension(
		get_num_aa_types(), get_num_incident_edges(), get_num_aa_types());

	//copy sparse aa-neighbor info from edges
	int count_neighbs_with_higher_indices = 0;
	for (int ii = 1; ii <= get_num_incident_edges(); ++ii)
	{
		neighbors_curr_state_sparse_info_[ii].set_aa_type( 1 );
		neighbors_state_recent_history_index_[ii] = 0;

		ObjexxFCL::FArray2D< unsigned char > const & edge_aa_neighbs =
			get_incident_linmem_edge(ii)->get_sparse_aa_neighbor_info();

		if ( get_node_index() < get_index_of_adjacent_node(ii) ) {
			++count_neighbs_with_higher_indices;
			for ( int jj = 1; jj <= get_num_aa_types(); ++jj ) {
				for ( int kk = 1; kk <= get_num_aa_types(); ++kk ) {
					aa_neighbors_for_edges_(kk, ii, jj) = edge_aa_neighbs(kk, jj);
				}
			}
		} else {
			for ( int jj = 1; jj <= get_num_aa_types(); ++jj ) {
				for ( int kk = 1; kk <= get_num_aa_types(); ++kk ) {
					aa_neighbors_for_edges_(kk, ii, jj) = edge_aa_neighbs(jj, kk);
				}
			}
		}
	}

	curr_state_two_body_energies_.resize( get_num_incident_edges());
	alternate_state_two_body_energies_.resize( get_num_incident_edges());
	return;
}


// @ brief - allow derived class to "drive" through the deltaE calculation
void
LinearMemNode::calc_deltaEpd( int alternate_state )
{
	core::PackerEnergy dummy(0.0f);
	project_deltaE_for_substitution( alternate_state, dummy );
}

int
LinearMemNode::update_recent_history( int state )
{
	return rhq_.push_to_front_of_history_queue( state );
}


//-----------------------------------------------------------------//

core::PackerEnergy const LinearMemEdge::NOT_YET_COMPUTED_ENERGY = -1234;



LinearMemEdge::LinearMemEdge(
	InteractionGraphBase* owner,
	int first_node_ind,
	int second_node_ind
):
	OnTheFlyEdge( owner, first_node_ind, second_node_ind),
	sparse_aa_neighbors_(
		get_linmem_ig_owner()->get_num_aatypes(),
		get_linmem_ig_owner()->get_num_aatypes(),
		(unsigned char) 0 ),
	curr_state_energy_( 0.0f ),
	partial_state_assignment_( false ),
	preped_for_sim_annealing_( false )
{
	store_rpes_[ 0 ] = store_rpes_[ 1 ] = true;
}

LinearMemEdge::~LinearMemEdge()
{}

void
LinearMemEdge::set_sparse_aa_info(
	ObjexxFCL::FArray2_bool const & aa_neighbors
)
{
	for (int ii = 1; ii <= get_linmem_ig_owner()->get_num_aatypes(); ++ii) {
		for (int jj = 1; jj <= get_linmem_ig_owner()->get_num_aatypes(); ++jj) {
			if ( aa_neighbors( jj, ii ) ) {
				sparse_aa_neighbors_( jj, ii ) = 1;
			} else {
				sparse_aa_neighbors_( jj, ii ) = 0;
			}
		}
	}
}


void LinearMemEdge::force_aa_neighbors( int node1aa, int node2aa)
{
	sparse_aa_neighbors_( node2aa, node1aa ) = 1;
}

void LinearMemEdge::force_all_aa_neighbors()
{
	for (int ii = 1; ii <= get_linmem_ig_owner()->get_num_aatypes(); ++ii) {
		for (int jj = 1; jj <= get_linmem_ig_owner()->get_num_aatypes(); ++jj) {
			sparse_aa_neighbors_( jj, ii ) = 1;
		}
	}
}

bool
LinearMemEdge::get_sparse_aa_info(
	int node1aa,
	int node2aa
) const
{
	return sparse_aa_neighbors_( node2aa, node1aa ) != 0;
}

core::PackerEnergy LinearMemEdge::get_two_body_energy( int const , int const ) const
{
	throw utility::excn::EXCN_Msg_Exception( "Method unimplemented: LinearMemEdge::get_two_body_energy" );
	return 0.0;
}

void
LinearMemEdge::declare_energies_final()
{}

void
LinearMemEdge::prepare_for_simulated_annealing()
{
	for (int ii = 0; ii < 2; ++ii ) {
		if ( ! store_rpes_[ ii ] ) wipe( ii );
		store_rpes_[ ii ] = true;
	}

	if ( preped_for_sim_annealing_ ) return;

	for (int ii = 0; ii < 2; ++ii) {
		int const other = ! ii;
		stored_rpes_[ ii ].dimension( get_num_states_for_node( other ),
			get_linmem_node( ii )->get_recent_history_size() );
		stored_rpes_[ ii ] = NOT_YET_COMPUTED_ENERGY;
	}

	preped_for_sim_annealing_ = true;
}

unsigned int
LinearMemEdge::count_static_memory() const
{
	return sizeof( LinearMemEdge );
}


unsigned int
LinearMemEdge::count_dynamic_memory() const
{
	unsigned int total_memory = OnTheFlyEdge::count_dynamic_memory();
	total_memory += sparse_aa_neighbors_.size() * sizeof( unsigned char );
	total_memory += stored_rpes_[ 0 ].size() * sizeof( core::PackerEnergy );
	total_memory += stored_rpes_[ 1 ].size() * sizeof( core::PackerEnergy );

	return total_memory;
}

/// @details  DANGER: this will not update the cached energies on the nodes this edge is incident upon.
void
LinearMemEdge::set_edge_weight( Real weight )
{
	Real const reweight_factor = weight / edge_weight();
	for (int ii = 0; ii < 2; ++ii) {
		for ( Size jj = 1; jj <= stored_rpes_[ ii ].size(); ++jj ) {
			if ( stored_rpes_[ ii ][ jj ] != NOT_YET_COMPUTED_ENERGY ) {
				stored_rpes_[ ii ][ jj ] *= reweight_factor;
			}
		}
	}
	edge_weight( weight );
}


core::PackerEnergy
LinearMemEdge::get_current_two_body_energy() const
{
	return curr_state_energy_;
}


void
LinearMemEdge::acknowledge_state_change(
	int node_ind,
	int new_state,
	SparseMatrixIndex const & new_state_sparse_info,
	int bumped_recent_history_index,
	int new_state_recent_history_index,
	core::PackerEnergy & new_energy
)
{
	int node_substituted =  ( node_ind == get_node_index(0) ? 0 : 1);
	int node_not_substituted = ! node_substituted;

	handle_bumped_recent_history_state_for_node(
		node_substituted,
		node_not_substituted,
		bumped_recent_history_index );

	curr_state_energy_ = get_linmem_node( 0 )->
		compute_pair_energy_for_current_state(
		get_edges_position_in_nodes_edge_vector( 0 ) );

	store_curr_state_energy();

	new_energy = curr_state_energy_;

	get_linmem_node( node_not_substituted )->
		acknowledge_neighbors_state_substitution
		(
		get_edges_position_in_nodes_edge_vector( node_not_substituted ),
		curr_state_energy_,
		new_state,
		new_state_sparse_info,
		new_state_recent_history_index
		);

	return;
}


void
LinearMemEdge::acknowledge_state_zeroed( int node_ind )
{
	int node_substituted = ( node_ind == get_node_index(0) ? 0 : 1);
	int node_not_substituted = ! node_substituted;

	curr_state_energy_ = 0;
	SparseMatrixIndex dummy_sparse_info;
	dummy_sparse_info.set_aa_type( 1 );
	dummy_sparse_info.set_state_ind_for_this_aa_type(1);

	get_linmem_node( node_not_substituted )->
		acknowledge_neighbors_state_substitution
		(
		get_edges_position_in_nodes_edge_vector( node_not_substituted ),
		curr_state_energy_,
		0,
		dummy_sparse_info,
		0
		);
	return;
}


void LinearMemEdge::acknowledge_partial_state_change(
	int node_ind,
	int new_state,
	SparseMatrixIndex const & new_state_sparse_info,
	int bumped_recent_history_index,
	int new_state_recent_history_index
)
{
	int node_substituted =  ( node_ind == get_node_index(0) ? 0 : 1);
	int node_not_substituted = ! node_substituted;

	handle_bumped_recent_history_state_for_node(
		node_substituted,
		node_not_substituted,
		bumped_recent_history_index );

	curr_state_energy_ = 0;

	get_linmem_node( node_not_substituted )->
		acknowledge_neighbors_partial_state_substitution
		(
		get_edges_position_in_nodes_edge_vector( node_not_substituted ),
		new_state,
		new_state_sparse_info,
		new_state_recent_history_index
		);
	partial_state_assignment_ = true;
	return;
}


core::PackerEnergy
LinearMemEdge::get_energy_following_partial_state_assignment()
{
	if (partial_state_assignment_
			&& get_linmem_node(0)->get_current_state() != 0
			&& get_linmem_node(1)->get_current_state() != 0) {

		curr_state_energy_ = get_linmem_node( 0 )->
			compute_pair_energy_for_current_state(
			get_edges_position_in_nodes_edge_vector( 0 ) );
		partial_state_assignment_ = false;
		store_curr_state_energy();
	}
	return curr_state_energy_;
}

void
LinearMemEdge::reset_state_energies(
	int node_index,
	int state,
	int recent_history_id
)
{
	int node_with_reset_state = node_index == get_node_index( 0 ) ? 0 : 1;
	int other = ! node_with_reset_state;

	if ( recent_history_id != 0 ) {
		ObjexxFCL::FArray1A< core::PackerEnergy > row_to_wipe(
			stored_rpes_[ node_with_reset_state ]( 1, recent_history_id ),
			get_num_states_for_node( other ) );
		row_to_wipe = NOT_YET_COMPUTED_ENERGY;
	}

	for (unsigned int ii = 1; ii <= stored_rpes_[ other ].size2(); ++ii) {
		stored_rpes_[ other ]( state, ii ) = NOT_YET_COMPUTED_ENERGY;
	}

}

core::PackerEnergy
LinearMemEdge::get_energy_for_alt_state
(
	bool store_rpes,
	int changing_node_index,
	int alternate_state,
	int alternate_state_recent_history_index,
	int other_node_curr_state,
	int other_node_state_recent_history_index
)
{
debug_assert( other_node_curr_state != 0 );

	//T << "get_energy_for_alt_state: " << get_node_index( 0 )  << " " << get_node_index( 1 ) << " srpe: " << store_rpes;
	//T << " chID " << changing_node_index << " alt: " << alternate_state << " altHI: " << alternate_state_recent_history_index;
	//T << " oncurr: " << other_node_curr_state << " oncurrHI: " << other_node_state_recent_history_index << std::endl;

	bool assignment_of_interest = debug && false; //get_node_index(0) == 67 && get_node_index(1) == 68;

	///if ( false ) {
	if ( assignment_of_interest ){
		T << "get_energy_for_alt_state: " << get_node_index( 0 )  << " " << get_node_index( 1 ) << " srpe: " << store_rpes;
		T << " chID " << changing_node_index << " alt: " << alternate_state << " altHI: " << alternate_state_recent_history_index;
		T << " oncurr: " << other_node_curr_state << " oncurrHI: " << other_node_state_recent_history_index << std::endl;
		T << "store_rpes_[ 0 ] " << store_rpes_[ 0 ] << "store_rpes_[ 1 ] " << store_rpes_[ 1 ] << std::endl;
	}

	int const node_changing = changing_node_index == get_node_index( 0 ) ? 0 : 1;
	int const node_not_changing = ! node_changing;

	if ( store_rpes && ! store_rpes_[ node_changing ] ) {
		wipe( node_changing );
	}
	store_rpes_[ node_changing ] = store_rpes;

	if ( store_rpes_[ node_changing ] && alternate_state_recent_history_index != 0 ) {
		alt_state_energy_ = stored_rpes_[ node_changing ]( other_node_curr_state, alternate_state_recent_history_index );
		if (alt_state_energy_ != NOT_YET_COMPUTED_ENERGY ) {

			if ( assignment_of_interest ) {
				T << "retrieving from stored_rpes_[ " << node_changing << " ] " << alt_state_energy_ << std::endl;
				T << "stored at location: " << & ( stored_rpes_[ node_changing ]( other_node_curr_state, alternate_state_recent_history_index ) ) << std::endl;
			}

			return alt_state_energy_;
		}
	}

	if ( store_rpes_[ node_not_changing ] ) {
		alt_state_energy_ = stored_rpes_[ node_not_changing ]( alternate_state, other_node_state_recent_history_index );
		if ( alt_state_energy_ != NOT_YET_COMPUTED_ENERGY ) {

			if ( assignment_of_interest ) {
				T << "retrieving from stored_rpes_[ " << node_not_changing << " ] " << alt_state_energy_ << std::endl;
				T << "stored at location: " << & ( stored_rpes_[ node_not_changing ]( alternate_state, other_node_state_recent_history_index ) ) << std::endl;
			}
			return alt_state_energy_;
		}
	}

	alt_state_energy_ = get_linmem_node( node_changing )->
		compute_pair_energy_for_alternate_state(
		get_edges_position_in_nodes_edge_vector( node_changing ));

	//if ( false ) {
	if ( assignment_of_interest ) {
		T << "get_energy_for_alt_state: computing energy: " << alt_state_energy_ << std::endl;
	}

	if ( store_rpes_[ node_changing ] && alternate_state_recent_history_index != 0 ) {
		stored_rpes_[ node_changing ]( other_node_curr_state, alternate_state_recent_history_index ) = alt_state_energy_;
	}
	if ( store_rpes_[ node_not_changing ] ) {
	debug_assert( other_node_state_recent_history_index );
		if ( assignment_of_interest ) {
			T << "about to write alt_state_energy_ to location: " << & ( stored_rpes_[ node_not_changing ]( alternate_state, other_node_state_recent_history_index ) ) << std::endl;
		}

		stored_rpes_[ node_not_changing ]( alternate_state, other_node_state_recent_history_index ) = alt_state_energy_;
	}

	return alt_state_energy_;
}

int LinearMemEdge::get_two_body_table_size() const
{
	return ( stored_rpes_[ 0 ].size() + stored_rpes_[ 1 ].size() );
}



ObjexxFCL::FArray2D< unsigned char > const &
LinearMemEdge::get_sparse_aa_neighbor_info( )
{
	return sparse_aa_neighbors_;
}


void
LinearMemEdge::print_current_energy() const
{
	T << "LinearMemEdge: " << get_node_index( 0 ) << "/" << get_node_index( 1 );
	T << " energy= " << curr_state_energy_ << std::endl;
}


void
LinearMemEdge::handle_bumped_recent_history_state_for_node
(
	int node_substituted,
	int node_not_substituted,
	int bumped_recent_history_index
)
{
	if ( ! store_rpes_[ node_substituted ] || bumped_recent_history_index == 0 ) return;

	ObjexxFCL::FArray1A< core::PackerEnergy > row_to_reset(
		stored_rpes_[ node_substituted ](1, bumped_recent_history_index ),
		get_num_states_for_node( node_not_substituted ) );
	row_to_reset = NOT_YET_COMPUTED_ENERGY;
}

void
LinearMemEdge::store_curr_state_energy()
{
	int curr_states[ 2 ];
	int recent_history_ids[ 2 ];
	for (int ii = 0; ii < 2; ++ii) {
		curr_states[ ii ] = get_linmem_node( ii )->get_current_state();
		recent_history_ids[ ii ] = get_linmem_node( ii )->get_curr_state_recent_state_id();
		if ( curr_states[ ii ] == 0 ) return;
	}

	for (int ii = 0; ii < 2; ++ii ) {
		int const other = ! ii;
		if ( store_rpes_[ ii ] ) {
			stored_rpes_[ ii ]( curr_states[ other ], recent_history_ids[ ii ] ) = curr_state_energy_;
		}
	}

}

void
LinearMemEdge::wipe( int node )
{
	stored_rpes_[ node ] = NOT_YET_COMPUTED_ENERGY;
}

//-------------------------------------------------------------------//

LinearMemoryInteractionGraph::LinearMemoryInteractionGraph(
	int numNodes
) : OnTheFlyInteractionGraph( numNodes ),
	first_time_prepping_for_simA_( true ),
	num_commits_since_last_update_( 0 ),
	total_energy_current_state_assignment_( 0.0 ),
	total_energy_alternate_state_assignment_( 0.0 ),
	node_considering_alt_state_( 0 ),
	recent_history_size_( 10 ),
	have_not_committed_last_substitution_( false )
{
}


LinearMemoryInteractionGraph::~LinearMemoryInteractionGraph()
{}

void
LinearMemoryInteractionGraph::blanket_assign_state_0()
{
	have_not_committed_last_substitution_ = false;
	for (int ii = 1; ii <= get_num_nodes(); ++ii)
	{
		get_linmem_node( ii )->assign_zero_state();
	}
	total_energy_current_state_assignment_ = 0;
}


core::PackerEnergy
LinearMemoryInteractionGraph::set_state_for_node(int node_ind, int new_state)
{
	have_not_committed_last_substitution_ = false;
	get_linmem_node( node_ind )->assign_state( new_state );
	update_internal_energy_totals();
	return total_energy_current_state_assignment_;
}


core::PackerEnergy
LinearMemoryInteractionGraph::set_network_state(
	ObjexxFCL::FArray1_int & node_states
)
{
	have_not_committed_last_substitution_ = false;
	for (int ii = 1; ii <= get_num_nodes(); ++ii)
	{
		get_linmem_node( ii )->partial_assign_state( node_states( ii ) );
	}
	for (int ii = 1; ii <= get_num_nodes(); ++ii)
	{
		get_linmem_node( ii )->complete_state_assignment();
	}
	update_internal_energy_totals();
	//T << "Set Network State Finished" << std::endl;
	//print_current_state_assignment();
	return total_energy_current_state_assignment_;
}


void
LinearMemoryInteractionGraph::consider_substitution(
	int node_ind,
	int new_state,
	core::PackerEnergy & delta_energy,
	core::PackerEnergy & prev_energy_for_node
)
{
	if (have_not_committed_last_substitution_)
	{
		get_linmem_node( node_considering_alt_state_ )->
			acknowledge_last_substititon_not_committed();
	}

	node_considering_alt_state_ = node_ind;

	delta_energy = get_linmem_node( node_ind )->
		project_deltaE_for_substitution( new_state, prev_energy_for_node );

	total_energy_alternate_state_assignment_ =
		total_energy_current_state_assignment_ + delta_energy;
	have_not_committed_last_substitution_ = true;
}

core::PackerEnergy
LinearMemoryInteractionGraph::commit_considered_substitution()
{
	have_not_committed_last_substitution_ = false;
	get_linmem_node( node_considering_alt_state_ )->commit_considered_substitution();

	total_energy_current_state_assignment_ =
		total_energy_alternate_state_assignment_;

	++num_commits_since_last_update_;
	if (num_commits_since_last_update_ == COMMIT_LIMIT_BETWEEN_UPDATES)
	{
		update_internal_energy_totals();
	}

	return total_energy_alternate_state_assignment_;
}


core::PackerEnergy
LinearMemoryInteractionGraph::get_energy_current_state_assignment()
{
	//T << "Num rotamer pair energy calculations performed: " << LinearMemNode::num_rpe_calcs << std::endl;
	update_internal_energy_totals();
	return total_energy_current_state_assignment_;
}

///@brief O(1) total energy report.  Protected read access for derived classes.
core::PackerEnergy
LinearMemoryInteractionGraph::get_energy_PD_current_state_assignment()
{
	return total_energy_current_state_assignment_;
}

int
LinearMemoryInteractionGraph::get_edge_memory_usage() const
{
	int sum = 0;
	for (std::list< EdgeBase* >::const_iterator iter = get_edge_list_begin();
		iter != get_edge_list_end(); ++iter)
	{
		sum += ((LinearMemEdge*) *iter)->get_two_body_table_size();
	}
	return sum;
}

void
LinearMemoryInteractionGraph::print_current_state_assignment() const
{
	T << "State Assignment: " << std::endl;
	for (int ii = 1; ii <= get_num_nodes(); ++ii)
	{
		T << "Node " << ii << " state " << get_linmem_node(ii)->get_current_state() << std::endl;
		get_linmem_node(ii)->print();
	}

	for (std::list< EdgeBase* >::const_iterator iter = get_edge_list_begin();
		iter != get_edge_list_end(); ++iter)
	{
		((LinearMemEdge*) (*iter))->print_current_energy();
	}
	T << "Energy: " << total_energy_current_state_assignment_ << std::endl;
}


void
LinearMemoryInteractionGraph::set_errorfull_deltaE_threshold( core::PackerEnergy )
{}

core::PackerEnergy
LinearMemoryInteractionGraph::get_energy_sum_for_vertex_group( int group_id )
{
	core::PackerEnergy esum = 0;
	for (int ii = 1; ii <= get_num_nodes(); ++ii) {
		if ( get_vertex_member_of_energy_sum_group( ii, group_id ) ) {
			esum += get_linmem_node( ii )->get_one_body_energy_current_state();
		}
	}

	for ( std::list< EdgeBase* >::iterator edge_iter = get_edge_list_begin();
			edge_iter != get_edge_list_end(); ++edge_iter) {
		int first_node_ind = (*edge_iter)->get_first_node_ind();
		int second_node_ind = (*edge_iter)->get_second_node_ind();

		if ( get_vertex_member_of_energy_sum_group( first_node_ind, group_id )
				&& get_vertex_member_of_energy_sum_group( second_node_ind, group_id )) {
			esum += ((LinearMemEdge*) (*edge_iter))->get_current_two_body_energy();
		}
	}

	return esum;
}

void
LinearMemoryInteractionGraph::prepare_for_simulated_annealing()
{
	if ( first_time_prepping_for_simA_ ) {
		set_recent_history_sizes();
		first_time_prepping_for_simA_ = false;
	}
	InteractionGraphBase::prepare_for_simulated_annealing();

}

void
LinearMemoryInteractionGraph::set_recent_history_size( Size recent_history_size ) {
	recent_history_size_ = recent_history_size;
}

Size
LinearMemoryInteractionGraph::get_recent_history_size() const {
	return recent_history_size_;
}

/*
bool
LinearMemoryInteractionGraph::build_sc_only_rotamer() const
{
	return true;
}
*/

unsigned int
LinearMemoryInteractionGraph::count_static_memory() const
{
	return sizeof( LinearMemoryInteractionGraph );
}

unsigned int
LinearMemoryInteractionGraph::count_dynamic_memory() const
{
	unsigned int total_memory = OnTheFlyInteractionGraph::count_dynamic_memory();
	return total_memory;
}


NodeBase*
LinearMemoryInteractionGraph::create_new_node( int node_index, int num_states)
{
	return new LinearMemNode( this, node_index, num_states );
}


EdgeBase*
LinearMemoryInteractionGraph::create_new_edge( int index1, int index2)
{
	return new LinearMemEdge( this, index1, index2 );
}

void
LinearMemoryInteractionGraph::update_internal_energy_totals()
{
	total_energy_current_state_assignment_ = 0;

	for (int ii = 1; ii <= get_num_nodes(); ++ii) {
		total_energy_current_state_assignment_ += get_linmem_node( ii )->
			get_one_body_energy_current_state();
	}

	for (std::list<EdgeBase*>::iterator iter = get_edge_list_begin();
		iter != get_edge_list_end(); ++iter) {
		total_energy_current_state_assignment_ +=
			((LinearMemEdge*) *iter)->get_current_two_body_energy();
	}

	num_commits_since_last_update_ = 0;
	return;
}

void
LinearMemoryInteractionGraph::set_recent_history_sizes()
{
	for (int ii = 1; ii <= get_num_nodes(); ++ii) {
		get_linmem_node( ii )->set_recent_history_size( recent_history_size_ );
	}
}

/*
int
LinearMemoryInteractionGraph::get_cmdline_history_size()
{
	int const default_history_size = 20;
	int LinMemIG_history_size;
	if ( linmem_ig ) {
		optional_positive_intafteroption( "linmem_ig", default_history_size, LinMemIG_history_size );
	}
	return LinMemIG_history_size;
}
*/

} // namespace interaction_graph
} // namespace pack
} // namespace core
