// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   src/core/chemical/residue_dbio.hh
/// @author Sam DeLuca
/// @author Matt O'Meara

#ifndef INCLUDED_core_chemical_ResidueDatabaseIO_hh
#define INCLUDED_core_chemical_ResidueDatabaseIO_hh

#include <utility/pointer/ReferenceCount.hh>
#include <core/chemical/ResidueType.fwd.hh>
#include <core/chemical/ResidueDatabaseIO.fwd.hh>
#include <utility/sql_database/DatabaseSessionManager.fwd.hh>
#include <core/chemical/AtomTypeSet.fwd.hh>
#include <core/chemical/MMAtomTypeSet.fwd.hh>
#include <core/chemical/orbitals/OrbitalTypeSet.fwd.hh>
#include <core/chemical/ElementSet.fwd.hh>
#include <core/chemical/ResidueTypeSet.fwd.hh>


namespace core {
namespace chemical {


class ResidueDatabaseIO : public utility::pointer::ReferenceCount {
public:

	ResidueDatabaseIO();

	virtual ~ResidueDatabaseIO();

	///@brief return sql statements that setup the right tables
	std::string
	schema() const;

	///@brief write the schema
	void initialize(utility::sql_database::sessionOP db_session);

	core::Real get_version();

	void write_residuetype_to_database(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	core::chemical::ResidueTypeOP read_residuetype_from_database(
		chemical::AtomTypeSetCAP atom_types,
		chemical::ElementSetCAP elements,
		chemical::MMAtomTypeSetCAP mm_atom_types,
		chemical::orbitals::OrbitalTypeSetCAP orbital_atom_types,
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		utility::sql_database::sessionOP db_session) const;

	utility::vector1<std::string> get_all_residues_in_database(utility::sql_database::sessionOP db_session) const;

private:

	std::string
	get_atom_name_from_database_atom_index(
		std::string residue_name,
		core::Size atom_index,
		utility::sql_database::sessionOP db_session) const;


	void
	report_residue_type(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_atom(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_atom(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_bond(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_bond(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;
	void
	report_residue_type_cut_bond(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_cut_bond(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_chi(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_chi(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_chi_rotamer(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_chi_rotamer(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_proton_chi(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_proton_chi(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_properties(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_properties(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;


	void
	report_residue_type_variant(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_variant(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	report_residue_type_icoor(
		std::string const & residue_type_set_name,
		core::chemical::ResidueType const & res_type,
		utility::sql_database::sessionOP db_session) const;

	void
	read_residue_type_icoor(
		std::string const & residue_type_set_name,
		std::string const & residue_type_name,
		core::chemical::ResidueType & res_type,
		utility::sql_database::sessionOP db_session) const;


private:
	core::Real version_;


};



}
}


#endif /* RESIDUE_DBIO_HH_ */
