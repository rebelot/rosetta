// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/disulfides/FullatomDisulfideEnergy.cc
/// @brief  Disulfide Energy class implementation
/// @author Andrew Leaver-Fay

// Unit headers
#include <core/scoring/disulfides/FullatomDisulfideEnergy.hh>
#include <core/scoring/disulfides/FullatomDisulfideEnergyCreator.hh>

// Package headers
#include <core/scoring/DerivVectorPair.hh>
#include <core/scoring/disulfides/FullatomDisulfidePotential.hh>
#include <core/scoring/disulfides/FullatomDisulfideEnergyContainer.hh>

// Project headers
#include <core/pose/Pose.hh>
#include <core/chemical/VariantType.hh>
#include <core/conformation/Conformation.hh>
#include <core/conformation/Residue.hh>
#include <core/scoring/EnergyMap.hh>
#include <core/scoring/Energies.hh>
// AUTO-REMOVED #include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/MinimizationData.hh>
#include <core/scoring/methods/Methods.hh>
#include <basic/datacache/CacheableData.hh>

#include <core/scoring/disulfides/DisulfideAtomIndices.hh>
#include <utility/vector1.hh>


namespace core {
namespace scoring {
namespace disulfides {

class DisulfMinData : public basic::datacache::CacheableData
{
public:
	typedef basic::datacache::CacheableDataOP CacheableDataOP;
public:
	DisulfMinData( conformation::Residue const & res1, conformation::Residue const & res2 );
	virtual ~DisulfMinData();
	CacheableDataOP clone() const;

	/// @brief which_res should be 1 or 2
	void set_res_inds( Size which_res, DisulfideAtomIndices const & dais );
	DisulfideAtomIndices const & res_inds( Size which_res ) const;

private:
	DisulfideAtomIndices res1_inds_;
	DisulfideAtomIndices res2_inds_;
};

typedef utility::pointer::owning_ptr< DisulfMinData > DisulfMinDataOP;
typedef utility::pointer::owning_ptr< DisulfMinData const > DisulfMinDataCOP;


DisulfMinData::DisulfMinData(
	conformation::Residue const & res1,
	conformation::Residue const & res2
) :
	res1_inds_( res1 ),
	res2_inds_( res2 )
{}

DisulfMinData::~DisulfMinData() {}

DisulfMinData::CacheableDataOP DisulfMinData::clone() const { return new DisulfMinData( *this ); }

/// @brief which_res should be 1 or 2
void DisulfMinData::set_res_inds( Size which_res, DisulfideAtomIndices const & dais )
{
	assert( which_res == 1 || which_res == 2 );
	if ( which_res == 1 ) res1_inds_ = dais;
	else res2_inds_ = dais;
}

DisulfideAtomIndices const &
DisulfMinData::res_inds( Size which_res ) const
{
	assert( which_res == 1 || which_res == 2 );
	return which_res == 1 ? res1_inds_ : res2_inds_;
}


/// @details This must return a fresh instance of the FullatomDisulfideEnergy class,
/// never an instance already in use
methods::EnergyMethodOP
FullatomDisulfideEnergyCreator::create_energy_method(
	methods::EnergyMethodOptions const &
) const {
	return new FullatomDisulfideEnergy( ScoringManager::get_instance()->get_FullatomDisulfidePotential() );
}

ScoreTypes
FullatomDisulfideEnergyCreator::score_types_for_method() const {
	ScoreTypes sts;
	sts.push_back( dslf_ss_dst );
	sts.push_back( dslf_cs_ang );
	sts.push_back( dslf_ss_dih );
	sts.push_back( dslf_ca_dih );
	sts.push_back( dslf_cbs_ds );
	return sts;
}



FullatomDisulfideEnergy::FullatomDisulfideEnergy( FullatomDisulfidePotential const & potential )
:
	parent( new FullatomDisulfideEnergyCreator ),
	potential_( potential )
{}

FullatomDisulfideEnergy::~FullatomDisulfideEnergy()
{}

// EnergyMethod Methods:

methods:: EnergyMethodOP
FullatomDisulfideEnergy::clone() const
{
	return new FullatomDisulfideEnergy( potential_ );
}

void
FullatomDisulfideEnergy::ensure_lrenergy_container_is_up_to_date(
	pose::Pose & pose
) const
{
	using namespace methods;

	if ( pose.energies().long_range_container( fa_disulfide_energy ) == 0 ) {
		FullatomDisulfideEnergyContainerOP dec = new FullatomDisulfideEnergyContainer( pose );
		pose.energies().set_long_range_container( fa_disulfide_energy, dec );
	} else {
		FullatomDisulfideEnergyContainerOP dec = FullatomDisulfideEnergyContainerOP (
			static_cast< FullatomDisulfideEnergyContainer * > (
			pose.energies().nonconst_long_range_container( fa_disulfide_energy ).get() ));
		dec->update( pose );
		if ( dec->num_residues() != pose.conformation().size() ) {
			FullatomDisulfideEnergyContainerOP dec = new FullatomDisulfideEnergyContainer( pose );
			pose.energies().set_long_range_container( fa_disulfide_energy, dec );
		}
	}
}

void
FullatomDisulfideEnergy::setup_for_scoring(
	pose::Pose & pose,
	ScoreFunction const & ) const
{
	ensure_lrenergy_container_is_up_to_date( pose );
}

void
FullatomDisulfideEnergy::setup_for_packing(
	pose::Pose & pose,
	utility::vector1< bool > const & ,
	utility::vector1< bool > const &
) const
{
	ensure_lrenergy_container_is_up_to_date( pose );
}

/// @details returns true if both residues are cys, if both are disulfide-cys, and then
/// if all of these conditions have been satisfied, if residue1's SG atom connects to residue 2,
/// and if residue 2's SG atom connects to residue 1
bool
FullatomDisulfideEnergy::defines_score_for_residue_pair(
	conformation::Residue const & res1,
	conformation::Residue const & res2,
	bool res_moving_wrt_eachother
) const
{
	using namespace chemical;
	return res_moving_wrt_eachother && res1.aa() == aa_cys && res2.aa() == aa_cys &&
		res1.has_variant_type( DISULFIDE ) && res2.has_variant_type( DISULFIDE ) &&
		res1.type().has_atom_name( "SG" ) &&
		res1.connect_map( res1.type().residue_connection_id_for_atom( res1.atom_index( "SG" ) ) ).resid() == res2.seqpos() &&
		res2.type().has_atom_name( "SG" ) &&
		res2.connect_map( res2.type().residue_connection_id_for_atom( res2.atom_index( "SG" ) ) ).resid() == res1.seqpos();
}

bool
FullatomDisulfideEnergy::use_extended_residue_pair_energy_interface() const { return true; }

bool
FullatomDisulfideEnergy::minimize_in_whole_structure_context( pose::Pose const & ) const { return false; }

void
FullatomDisulfideEnergy::residue_pair_energy_ext(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	ResPairMinimizationData const & min_data,
	pose::Pose const &,
	ScoreFunction const &,
	EnergyMap & emap
) const
{
	// ignore scoring residues which have been marked as "REPLONLY" residues (only the repulsive energy will be calculated)
	if ( rsd1.has_variant_type( core::chemical::REPLONLY ) || rsd2.has_variant_type( core::chemical::REPLONLY ) ){
		return;
	}

	conformation::Residue const & rsdl( rsd1.seqpos() < rsd2.seqpos() ? rsd1 : rsd2 );
	conformation::Residue const & rsdu( rsd1.seqpos() < rsd2.seqpos() ? rsd2 : rsd1 );

	assert( dynamic_cast< DisulfMinData const * > ( min_data.get_data( fa_dslf_respair_data )() ) );
	DisulfMinData const & disulf_inds = static_cast< DisulfMinData const & > ( *min_data.get_data( fa_dslf_respair_data ) );

	Energy distance_score_this_disulfide;
	Energy csangles_score_this_disulfide;
	Energy dihedral_score_this_disulfide;
	Energy ca_dihedral_sc_this_disulf;
	bool truefalse_fa_disulf;

	potential_.score_this_disulfide(
		rsdl, rsdu,
		disulf_inds.res_inds( 1 ),
		disulf_inds.res_inds( 2 ),
		distance_score_this_disulfide,
		csangles_score_this_disulfide,
		dihedral_score_this_disulfide,
		ca_dihedral_sc_this_disulf,
		truefalse_fa_disulf
	);

	emap[ dslf_ss_dst ] += distance_score_this_disulfide;
	emap[ dslf_cs_ang ] += csangles_score_this_disulfide;
	emap[ dslf_ss_dih ] += dihedral_score_this_disulfide;
	emap[ dslf_ca_dih ] += ca_dihedral_sc_this_disulf;

}

void
FullatomDisulfideEnergy::setup_for_minimizing_for_residue_pair(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	pose::Pose const &,
	ScoreFunction const &,
	kinematics::MinimizerMapBase const &,
	ResSingleMinimizationData const &,
	ResSingleMinimizationData const &,
	ResPairMinimizationData & data_cache
) const
{
	// ignore scoring residues which have been marked as "REPLONLY" residues (only the repulsive energy will be calculated)
	if ( rsd1.has_variant_type( core::chemical::REPLONLY ) || rsd2.has_variant_type( core::chemical::REPLONLY ) ){
		return;
	}

	conformation::Residue const & rsdl( rsd1.seqpos() < rsd2.seqpos() ? rsd1 : rsd2 );
	conformation::Residue const & rsdu( rsd1.seqpos() < rsd2.seqpos() ? rsd2 : rsd1 );

	DisulfMinDataOP disulf_inds = new DisulfMinData( rsdl, rsdu );

	data_cache.set_data( fa_dslf_respair_data, disulf_inds );
}

void
FullatomDisulfideEnergy::eval_residue_pair_derivatives(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	ResSingleMinimizationData const &,
	ResSingleMinimizationData const &,
	ResPairMinimizationData const & min_data,
	pose::Pose const &,
	EnergyMap const & weights,
	utility::vector1< DerivVectorPair > & r1_atom_derivs,
	utility::vector1< DerivVectorPair > & r2_atom_derivs
) const
{
	// ignore scoring residues which have been marked as "REPLONLY" residues (only the repulsive energy will be calculated)
	if ( rsd1.has_variant_type( core::chemical::REPLONLY ) || rsd2.has_variant_type( core::chemical::REPLONLY ) ){
		return;
	}

	assert( dynamic_cast< DisulfMinData const * > ( min_data.get_data( fa_dslf_respair_data )() ) );
	DisulfMinData const & disulf_inds = static_cast< DisulfMinData const & > ( *min_data.get_data( fa_dslf_respair_data ) );

	/// this could be substantially more efficient, but there are only ever a handful of disulfides in proteins,
	/// so there's basically no point in spending time making this code faster
	potential_.get_disulfide_derivatives(
		rsd1, rsd2, disulf_inds.res_inds( 1 ), disulf_inds.res_inds( 2 ),
		disulf_inds.res_inds(1).c_alpha_index(), weights,
		r1_atom_derivs[ disulf_inds.res_inds(1).c_alpha_index() ].f1(),
		r1_atom_derivs[ disulf_inds.res_inds(1).c_alpha_index() ].f2() );

	potential_.get_disulfide_derivatives(
		rsd1, rsd2, disulf_inds.res_inds( 1 ), disulf_inds.res_inds( 2 ),
		disulf_inds.res_inds(1).c_beta_index(), weights,
		r1_atom_derivs[ disulf_inds.res_inds(1).c_beta_index() ].f1(),
		r1_atom_derivs[ disulf_inds.res_inds(1).c_beta_index() ].f2() );

	potential_.get_disulfide_derivatives(
		rsd1, rsd2, disulf_inds.res_inds( 1 ), disulf_inds.res_inds( 2 ),
		disulf_inds.res_inds(1).disulf_atom_index(), weights,
		r1_atom_derivs[ disulf_inds.res_inds(1).disulf_atom_index() ].f1(),
		r1_atom_derivs[ disulf_inds.res_inds(1).disulf_atom_index() ].f2() );

	potential_.get_disulfide_derivatives(
		rsd2, rsd1, disulf_inds.res_inds( 2 ), disulf_inds.res_inds( 1 ),
		disulf_inds.res_inds(2).c_alpha_index(), weights,
		r2_atom_derivs[ disulf_inds.res_inds(2).c_alpha_index() ].f1(),
		r2_atom_derivs[ disulf_inds.res_inds(2).c_alpha_index() ].f2() );

	potential_.get_disulfide_derivatives(
		rsd2, rsd1, disulf_inds.res_inds( 2 ), disulf_inds.res_inds( 1 ),
		disulf_inds.res_inds(2).c_beta_index(), weights,
		r2_atom_derivs[ disulf_inds.res_inds(2).c_beta_index() ].f1(),
		r2_atom_derivs[ disulf_inds.res_inds(2).c_beta_index() ].f2() );

	potential_.get_disulfide_derivatives(
		rsd2, rsd1, disulf_inds.res_inds( 2 ), disulf_inds.res_inds( 1 ),
		disulf_inds.res_inds(2).disulf_atom_index(), weights,
		r2_atom_derivs[ disulf_inds.res_inds(2).disulf_atom_index() ].f1(),
		r2_atom_derivs[ disulf_inds.res_inds(2).disulf_atom_index() ].f2() );

}

/*void
FullatomDisulfideEnergy::eval_atom_derivative_for_residue_pair(
	Size const atom_index,
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	ResSingleMinimizationData const &,
	ResSingleMinimizationData const &,
	ResPairMinimizationData const & minpair_data,
	pose::Pose const &, // provides context
	kinematics::DomainMap const &,
	ScoreFunction const &,
	EnergyMap const & weights,
	Vector & F1,
	Vector & F2
) const
{
	Size my_ind( rsd1.seqpos() < rsd2.seqpos()    ? 1 : 2 );
	Size other_ind( rsd1.seqpos() < rsd2.seqpos() ? 2 : 1 );

	assert( dynamic_cast< DisulfMinData const * > ( minpair_data.get_data( fa_dslf_respair_data )() ) );
	DisulfMinData const & disulf_inds = static_cast< DisulfMinData const & > ( *minpair_data.get_data( fa_dslf_respair_data ) );

	Vector f1( 0.0 ), f2( 0.0 );
	potential_.get_disulfide_derivatives(
		rsd1, rsd2,
		disulf_inds.res_inds( my_ind ),
		disulf_inds.res_inds( other_ind ),
		atom_index,
		weights,
		f1, f2 );

	F1 += f1;
	F2 += f2;
}*/

void
FullatomDisulfideEnergy::old_eval_atom_derivative(
	id::AtomID const & atomid,
	pose::Pose const & pose,
	kinematics::DomainMap const &,
	ScoreFunction const &,
	EnergyMap const & weights,
	Vector & F1,
	Vector & F2
) const
{
	// ignore scoring residues which have been marked as "REPLONLY" residues (only the repulsive energy will be calculated)
	if ( pose.residue( atomid.rsd() ).has_variant_type( core::chemical::REPLONLY )){
		return;
	}

	FullatomDisulfideEnergyContainerCOP dec = FullatomDisulfideEnergyContainerCOP (
		static_cast< FullatomDisulfideEnergyContainer const * > (
		pose.energies().long_range_container( methods::fa_disulfide_energy ).get() ));
	if ( ! dec->residue_forms_disulfide( atomid.rsd() ) ) return;

	if ( dec->disulfide_atom_indices( atomid.rsd() ).atom_gets_derivatives( atomid.atomno() ) ) {
		conformation::Residue const & res( pose.residue( atomid.rsd() ));
	//if ( res.atom_name( atomid.atomno() ) == " CA " ||
	//		res.atom_name( atomid.atomno() ) == " CB "  ||
	//		res.atom_name( atomid.atomno() ) == " SG " ) {
		Vector f1( 0.0 ), f2( 0.0 );
		potential_.get_disulfide_derivatives(
			res,
			pose.residue( dec->other_neighbor_id( atomid.rsd()) ),
			dec->disulfide_atom_indices( atomid.rsd() ),
			dec->other_neighbor_atom_indices( atomid.rsd() ),
			atomid.atomno(),
			weights,
			f1, f2 );
		F1 += f1;
		F2 += f2;
	}

}



Real
FullatomDisulfideEnergy::eval_dof_derivative(
	id::DOF_ID const &,
	id::TorsionID const &,
	pose::Pose const &,
	ScoreFunction const &,
	EnergyMap const &
) const
{
	return 0.0;
}

void
FullatomDisulfideEnergy::indicate_required_context_graphs( utility::vector1< bool > & ) const
{}


// TwoBodyEnergy Methods:

void
FullatomDisulfideEnergy::residue_pair_energy(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	pose::Pose const & pose,
	ScoreFunction const &,
	EnergyMap & emap
) const
{
	// ignore scoring residues which have been marked as "REPLONLY" residues (only the repulsive energy will be calculated)
	if ( rsd1.has_variant_type( core::chemical::REPLONLY ) || rsd2.has_variant_type( core::chemical::REPLONLY ) ){
		return;
	}

	if ( rsd1.aa() != chemical::aa_cys || rsd2.aa() != chemical::aa_cys ) return;

	Energy distance_score_this_disulfide;
	Energy csangles_score_this_disulfide;
	Energy dihedral_score_this_disulfide;
	Energy ca_dihedral_sc_this_disulf;
	bool truefalse_fa_disulf;

	FullatomDisulfideEnergyContainerCOP dec = FullatomDisulfideEnergyContainerCOP (
		static_cast< FullatomDisulfideEnergyContainer const * > (
		pose.energies().long_range_container( methods::fa_disulfide_energy ).get() ));

	if ( ! dec->residue_forms_disulfide( rsd1.seqpos() ) ||
			dec->other_neighbor_id( rsd1.seqpos() ) != (Size) rsd2.seqpos() ){
		return;
	}

	potential_.score_this_disulfide(
		rsd1, rsd2,
		dec->disulfide_atom_indices( rsd1.seqpos() ),
		dec->other_neighbor_atom_indices( rsd1.seqpos() ), //The function change from the above line changes which index we get; if we also change which rsd we use then it turns everything upside down twice and the disulfide_atom_indices inappropriately match
		distance_score_this_disulfide,
		csangles_score_this_disulfide,
		dihedral_score_this_disulfide,
		ca_dihedral_sc_this_disulf,
		truefalse_fa_disulf
	);

	/*
	Energy cbs_sc_this_disulf( 0.0 );

	if ( ! sfxn.has_zero_weight( dslf_cbs_ds ) ) {
		potential_.get_cbs_sc_this_disulf(
			rsd1, rsd2, cbs_sc_this_disulf
		);
	}
	*/

	//if ( truefalse_fa_disulf ) { // this just allows the packer to unwittingly break a disulfide bond, what is its point?
	emap[ dslf_ss_dst ] += distance_score_this_disulfide;
	emap[ dslf_cs_ang ] += csangles_score_this_disulfide;
	emap[ dslf_ss_dih ] += dihedral_score_this_disulfide;
	emap[ dslf_ca_dih ] += ca_dihedral_sc_this_disulf;

	//emap[ dslf_cbs_ds ] = cbs_sc_this_disulf;
	//}
}


bool
FullatomDisulfideEnergy::defines_intrares_energy( EnergyMap const & ) const
{
	return false;
}


void
FullatomDisulfideEnergy::eval_intrares_energy(
	conformation::Residue const &,
	pose::Pose const &,
	ScoreFunction const &,
	EnergyMap &
) const
{
}

// LongRangeTwoBodyEnergy methods
methods::LongRangeEnergyType
FullatomDisulfideEnergy::long_range_type() const
{
	return methods::fa_disulfide_energy;
}


bool
FullatomDisulfideEnergy::defines_residue_pair_energy(
	pose::Pose const & pose,
	Size res1,
	Size res2
) const
{

	using namespace methods;
	if ( ! pose.energies().long_range_container( fa_disulfide_energy )) return false;

	FullatomDisulfideEnergyContainerCOP dec = FullatomDisulfideEnergyContainerCOP (
		static_cast< FullatomDisulfideEnergyContainer const * > (
		pose.energies().long_range_container( fa_disulfide_energy ).get() ));
	return dec->disulfide_bonded( res1, res2 );
} core::Size
FullatomDisulfideEnergy::version() const
{
	return 1; // Initial versioning
}


} // namespace disulfides
} // namespace scoring
} // namespace core

