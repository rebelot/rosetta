// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/pose/util.hh
/// @brief  Pose utilities
/// @author Phil Bradley
/// @author Modified by Sergey Lyskov, Vikram K. Mulligan, Jared Adolf-Bryfogle

#ifndef INCLUDED_core_pose_util_hh
#define INCLUDED_core_pose_util_hh

// Package headers
#include <core/pose/Pose.fwd.hh>
#include <core/pose/util.tmpl.hh>
#include <core/pose/MiniPose.fwd.hh>

// Project headers
#include <core/types.hh>
#include <core/conformation/Residue.hh>
#include <core/chemical/ResidueType.fwd.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/rings/AxEqDesignation.hh>
#include <core/id/AtomID.fwd.hh>
#include <core/id/AtomID_Map.fwd.hh>
#include <core/id/DOF_ID_Mask.fwd.hh>
#include <core/id/NamedAtomID.fwd.hh>
#include <core/id/NamedStubID.fwd.hh>
#include <core/id/TorsionID.fwd.hh>
#include <core/id/SequenceMapping.fwd.hh>
#include <core/io/StructFileRep.fwd.hh>
#include <core/kinematics/FoldTree.fwd.hh>
#include <core/kinematics/Jump.hh>
#include <core/kinematics/MoveMap.fwd.hh>
#include <core/kinematics/RT.fwd.hh>
#include <core/kinematics/tree/Atom.fwd.hh>
#include <core/scoring/ScoreType.hh>

// Utility headers
#include <numeric/xyzVector.hh>
#include <utility/vector1.hh>

// C/C++ headers
#include <map>
#include <tuple>
#include <set>


namespace core {
namespace pose {

typedef std::set< int > Jumps;

/// @brief Append residues of pose2 to pose1.
void
append_pose_to_pose(
	core::pose::Pose & pose1,
	core::pose::Pose const & pose2,
	bool new_chain = true
);

/// @brief Append specified residues of pose2 to pose1.
void
append_subpose_to_pose(
	core::pose::Pose & pose1,
	core::pose::Pose const & pose2,
	core::Size start_res,
	core::Size end_res,
	bool new_chain = true
);

/// @brief Retrieves jump information from <pose>, storing the result in <jumps>.
/// Jumps are keyed by their jump id.
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
void jumps_from_pose(core::pose::Pose const & pose, Jumps & jumps);

/// @brief Updates the rigid-body transform of the specified jump in <pose>
void swap_transform(Size jump_num, kinematics::RT const & xform, Pose & pose);

/// @brief Returns true if <residue> is positionally conserved, false otherwise
/// Based on the POSITION_CONSERVED_RESIDUES annotation stored in the Pose DataCache
bool is_position_conserved_residue(Pose const & pose, core::Size residue);

/// @brief Create a subpose of the src pose.  PDBInfo is set as NULL.
void
create_subpose(
	Pose const & src,
	utility::vector1< Size > const & positions,
	kinematics::FoldTree const & f,
	Pose & pose
);

/// @brief Create a subpose of the src pose -- figures out a reasonable fold tree.
void
pdbslice( pose::Pose & new_pose,
	pose::Pose const & pose,
	utility::vector1< Size > const & slice_res );

/// @brief Create a subpose of the src pose -- figures out a reasonable fold tree.
void
pdbslice( pose::Pose & pose,
	utility::vector1< Size > const & slice_res );


void set_reasonable_fold_tree( core::pose::Pose & pose );


// for partition_by_jump: both new poses start residue numbering from 1 and don't keep the original numbering!
void
partition_pose_by_jump(
	pose::Pose const & src,
	int const jump_number,
	pose::Pose & partner1, // partner upstream in foldtree
	pose::Pose & partner2  // partner downstream in foldtree
);

/// @brief Analyzes  <pose>  residue phi/psi sets and guesses the secondary
/// structure, ideally dssp should be used for that
void
set_ss_from_phipsi(
	pose::Pose & pose
);

// /// @brief Analyses the pose in terms of phi/psi and guesses at the secondary
// /// structure - ideally dssp should be used for that
// void
// set_ss_from_phipsi_dssp(
//  pose::Pose &pose
// );

utility::vector1< char > read_psipred_ss2_file( pose::Pose const & pose, std::string const & filename );
utility::vector1< char > read_psipred_ss2_file( pose::Pose const & pose );

/// getters/setters for things in the Pose DataCache

/// @brief return bool is T/F for whether the requested datum exists.  "value" is the data, pass-by-ref.
bool getPoseExtraScore(
	core::pose::Pose const & pose,
	std::string const & name,
	core::Real & value
);

/// @brief return value is ExtraScore if exist, runtime_assert if it doesn't exist
Real getPoseExtraScore(
	core::pose::Pose const & pose,
	std::string const & name );

/// @brief does this ExtraScore exist?
bool
hasPoseExtraScore(
	core::pose::Pose const & pose,
	std::string const & name );

void setPoseExtraScore(
	core::pose::Pose & pose,
	std::string const & name,
	core::Real value
);

void clearPoseExtraScore(
	core::pose::Pose & pose,
	std::string const & name
);

void clearPoseExtraScores(
	core::pose::Pose & pose
);

/// @brief return bool is T/F for whether the requested datum exists.  "value" is the data, pass-by-ref.
bool getPoseExtraScore(
	core::pose::Pose const & pose,
	std::string const & name,
	std::string & value
);

void setPoseExtraScore(
	core::pose::Pose & pose,
	std::string const & name,
	std::string const & value
);

/// @brief Return the appropritate ResidueType for the virtual residue for the i
/// "mode" (fullatom, centroid ...) the pose is in.
core::chemical::ResidueTypeCOP
virtual_type_for_pose(core::pose::Pose const & pose);

/// @brief Adds a VRT res to the end of the pose at the center of mass.
/// Reroots the structure on this res.
void addVirtualResAsRoot(core::pose::Pose & pose);

/// @brief Adds a virtual residue to the end of the pose at the specified location.
/// Roots the structure on this residue.
void addVirtualResAsRoot(const numeric::xyzVector<core::Real>& xyz, core::pose::Pose& pose);

/// @brief Removes all virtual residues from <pose>
void remove_virtual_residues(core::pose::Pose & pose);

/// @brief Get center of mass of a pose.
///
/// This computes an equally-weighted, all-(non-virtual)-heavy atom center.
numeric::xyzVector< core::Real >
get_center_of_mass( core::pose::Pose const & pose );

/// @brief Adds a key-value pair to the STRING_MAP in the Pose DataCache. If
/// there is no STRING_MAP in the DataCache, one is created.
void add_comment(
	core::pose::Pose & pose,
	std::string const & key,
	std::string const & val
);

/// @brief Attempts to access the entry in the STRING_MAP associated with the
/// given key. If an entry for the key exists, the value associated with the key
/// is put into val, and this function returns true. Otherwise, this function
/// returns false and val left unmodified.
bool get_comment(
	core::pose::Pose const & pose,
	std::string const & key,
	std::string & val
);

/// @brief Deletes the entry in the STRING_MAP associated with the
/// given key.
void delete_comment(
	core::pose::Pose & pose,
	std::string const & key
);

/// @brief Gets a map< string, string > representing comments about the Pose in
/// the form of key-value pairs.
std::map< std::string, std::string > get_all_comments(
	core::pose::Pose const & pose
);

/// @brief Dumps a pdb with comments at end of file


/// @brief Sets a PDB-style REMARK entry in the Pose.
/// @details This is different from a comment in its interpretation by the
/// silent-file output machinery. A REMARK is written on its own separate line
/// in the output silent-file, while a comment is written as part of the Pose
/// SCORE: lines.
void add_score_line_string(
	core::pose::Pose & pose,
	std::string const & key,
	std::string const & val
);

bool get_score_line_string(
	core::pose::Pose const & pose,
	std::string const & key,
	std::string & val
);

/// @brief Gets a map< string, string > representing score_line_strings about the Pose in
/// the form of key-value pairs.
std::map< std::string, std::string > get_all_score_line_strings(
	core::pose::Pose const & pose
);

/// @brief get Conformation chain number -> PDBInfo chain mapping
/// @remarks Any chains whose PDBInfo chain records are marked entirely as
///  PDBInfo::empty_record() will be mapped to that character.
/// @return the mapping if PDBInfo available and chains appear consistent,
///  otherwise prints a warning and returns a default mapping (1=A, 2=B_
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
std::map< core::Size, char > conf2pdb_chain( core::pose::Pose const & pose );

/// @brief Get all the chain numbers from conformation
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// @details This is a rather silly function, as it will just return a vector
/// with entries from 1 to pose->num_chains() (as chains numbers are sequential starting from 1
utility::vector1< core::Size > get_chains( core::pose::Pose const & pose );

/// @brief compute last residue number of a chain
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
//// @details This is mostly indirection to Conformation::chain_end(), but with better error checking
core::Size chain_end_res( Pose const & pose, core::Size const chain );

/// @brief compute last residue numbers of all chains
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
//// @details This is mostly an indirection to Conformation::chain_endings(), though with better handling of the last residue
utility::vector1< core::Size > chain_end_res( Pose const & pose );

/// @brief Compute uniq chains in a complex, based on sequence identity
/// @details Returns a vector of pose length with true/false of uniq chain
///    true is unique, false is not
utility::vector1< bool > compute_unique_chains( Pose & pose );

/// @brief Repair pdbinfo of inserted residues that may have blank chain and zero
/// seqpos. Assumes insertions only occur _after_ a residue.
void fix_pdbinfo_damaged_by_insertion(
	core::pose::Pose & pose
);

/// @brief renumber PDBInfo based on Conformation chains; each chain starts from 1
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// @param[in,out] pose The Pose to modify.
/// @param[in] fix_chains If true, the procedure will attempt to fix any empty record
///  characters it finds in the PDBInfo. (default true)
/// @param[in] start_from_existing_numbering If true, will attempt to start each
///  chain from the existing numbering in the PDBInfo.  E.g. if the first residue
///  of chain 2 in the Conformation is 27, then the renumbering of the chain in
///  PDBInfo will start from 27. (default true)
/// @param[in] keep_insertion_codes If true, will maintain insertion codes and
///  will not increment the pdb residue numbering for those residues.  This means
///  new numbering with insertion codes will only reflect properly if the
///  old numbering included the base numbering of the insertion code residues,
///  i.e. 100 100A 100B and not just 100A 100B (with 100 never appearing).
///  (default false)
/// @param[in] rotate_chain_ids If true, allows support for more than 26 pdb chains
///  by rotating [A,Z] continuously.  WARNING: This will break the assumption
///  made by the PDBPoseMap that each pdb chain id is unique, so make sure you
///  are not using the PDBPoseMap feature downstream in your code path without
///  corrections! (default false)
/// @remarks If fixing chains and there is only one chain and the PDBInfo exists
///  but all records are marked as empty, will renumber and set the PDBInfo chain
///  to 'A'.
/// @return true if renumbering successful, false otherwise
bool renumber_pdbinfo_based_on_conf_chains(
	core::pose::Pose & pose,
	bool fix_chains = true,
	bool const start_from_existing_numbering = true,
	bool const keep_insertion_codes = false,
	bool const rotate_chain_ids = false
);

/// @brief Returns true if the  <pose>  geometry is ideal
/// @param[in] pose The Pose to check.
/// @return true if all pose positions have ideal bond lengths and angles
///  up to some very small epsilon
bool is_ideal_pose(
	core::pose::Pose const & pose
);

/// @brief Returns true if the  <pose> geometry is ideal in position  <seqpos>
/// @param[in] pose The Pose to check.
/// @return true if position seqpos has ideal bond lengths and angles
///  up to some very small epsilon
bool is_ideal_position(
	Size seqpos,
	core::pose::Pose const & pose
);

/// @brief this function removes all residues from the pose which are not protein residues.  This removal includes, but is not limited to, metals, DNA, RNA, and ligands.  It will NOT remove ligands which are canonical residues (for example, if a protein binds an alanine monomer, the monomer will be untouched).
void remove_nonprotein_residues( core::pose::Pose & pose );

/// @brief this function removes all residues with both UPPER and LOWER terminus types.  This is intended for removing ligands that are canonical residues.
void remove_ligand_canonical_residues( core::pose::Pose & pose );

/// @brief this function compares pose atom coordinates for equality; it is not the == operator because it does not compare all pose data.
/// @author Steven Lewis smlewi@gmail.com
/// @param[in] lhs one pose to compare
/// @param[in] rhs one pose to compare
/// @param[in] n_dec_places number of decimal places to compare for the coordinates (remember == doesn't work for float); defaults to 3 which is PDB accuracy
bool compare_atom_coordinates(
	Pose const & lhs,
	Pose const & rhs,
	Size const n_dec_places = 3);

/// @brief this function compares poses for equality up to the
///information stored in the binary protein silent struct format.
bool compare_binary_protein_silent_struct(
	Pose const & lhs,
	Pose const & rhs);


/// @brief  Reads the comments from the pdb file and adds it into comments
void read_comment_pdb(
	std::string const &file_name,
	core::pose::Pose  & pose
);
/// @brief  dumps pose+ comments to pdb file
void dump_comment_pdb(
	std::string const &file_name,
	core::pose::Pose const& pose
);

id::NamedAtomID
atom_id_to_named_atom_id(
	core::id::AtomID const & atom_id,
	Pose const & pose
);

id::AtomID
named_atom_id_to_atom_id(
	core::id::NamedAtomID const & named_atom_id,
	Pose const & pose,
	bool raise_exception = true
);

id::NamedStubID
stub_id_to_named_stub_id(
	core::id::StubID const & stub_id,
	Pose const & pose
);

id::StubID
named_stub_id_to_stub_id(
	core::id::NamedStubID const & named_stub_id,
	Pose const & pose
);

///////////////////////////////////////////////////////////////////
std::string tag_from_pose( core::pose::Pose const & pose );
void tag_into_pose( core::pose::Pose & pose, std::string const & tag );

// criterion for sorting.
bool sort_pose_by_score( core::pose::PoseOP const & pose1, core::pose::PoseOP const & pose2 );

core::Real energy_from_pose(
	core::pose::Pose const & pose, core::scoring::ScoreType const & sc_type
);

core::Real energy_from_pose(
	core::pose::Pose const & pose, std::string const & sc_type
);

core::Real total_energy_from_pose( core::pose::Pose const & pose );

void
transfer_phi_psi( const core::pose::Pose& srcpose, core::pose::Pose& tgtpose, core::Size ir, core::Size jr );

void
transfer_phi_psi( const core::pose::Pose& srcpose, core::pose::Pose& tgtpose );

void
transfer_jumps( const core::pose::Pose& srcpose, core::pose::Pose& tgtpose);

void
replace_pose_residue_copying_existing_coordinates(
	pose::Pose & pose,
	Size const seqpos,
	core::chemical::ResidueType const & new_rsd_type
);

/// @brief Return the residue type in the correct
/// "mode" (fullatom, centroid ...) the pose is in.
core::chemical::ResidueTypeCOP
get_restype_for_pose(core::pose::Pose const & pose, std::string const & name);

/// @brief Return the residue type in the passed mode,
/// respecting any modification that pose may make.
core::chemical::ResidueTypeCOP
get_restype_for_pose(core::pose::Pose const & pose, std::string const & name, core::chemical::TypeSetMode mode);

/// @brief Remove variant from an existing residue.
conformation::ResidueOP remove_variant_type_from_residue(
	conformation::Residue const & old_rsd,
	core::chemical::VariantType const variant_type,
	pose::Pose const & pose );

/// @brief Construct a variant of an existing residue.
conformation::ResidueOP add_variant_type_to_residue(
	conformation::Residue const & old_rsd,
	core::chemical::VariantType const variant_type,
	pose::Pose const & pose );

/// @brief Construct a variant of an existing pose residue.
void add_variant_type_to_pose_residue(
	pose::Pose & pose,
	chemical::VariantType const variant_type,
	Size const seqpos );

/// @brief Construct a non-variant of an existing pose residue.
void remove_variant_type_from_pose_residue(
	pose::Pose & pose,
	chemical::VariantType const variant_type,
	Size const seqpos );


void
add_lower_terminus_type_to_pose_residue(
	pose::Pose & pose,
	Size const seqpos
);


void
add_upper_terminus_type_to_pose_residue(
	pose::Pose & pose,
	Size const seqpos
);


void
remove_lower_terminus_type_from_pose_residue(
	pose::Pose & pose,
	Size const seqpos
);


void
remove_upper_terminus_type_from_pose_residue(
	pose::Pose & pose,
	Size const seqpos
);

/// @brief set up a map to look up TORSION_ID by DOF_ID (Map[DOF_ID] = TORISION_ID)
void
setup_dof_to_torsion_map(
	pose::Pose const & pose,
	id::DOF_ID_Map< id::TorsionID > & dof_map
);


/// @brief convert from allow-bb/allow-chi MoveMap to simple DOF_ID boolean mask needed by the minimizer
void
setup_dof_mask_from_move_map(
	core::kinematics::MoveMap const & mm,
	pose::Pose const & pose,
	id::DOF_ID_Mask & dof_mask
);

/// @brief Does the pose have a residue with the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
bool
has_chain(std::string const & chain, core::pose::Pose const & pose);

/// @brief Does the pose have a residue with the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
bool
has_chain(char const & chain, core::pose::Pose const & pose);

/// @brief Does the pose have a residue with the given chain number
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
bool
has_chain(core::Size chain_id, core::pose::Pose const & pose);

/// @brief Get all chain numbers for the residues with the given chain letters
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The returned chain numbers are in sorted order
utility::vector1<core::Size>
get_chain_ids_from_chains(utility::vector1<std::string> const & chains, core::pose::Pose const & pose);

/// @brief Get all chain numbers for the residues with the given chain letters
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The returned chain numbers are in sorted order
utility::vector1<core::Size>
get_chain_ids_from_chains(utility::vector1<char> const & chains, core::pose::Pose const & pose);

/// @brief Get all chain numbers for the residues with the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The returned chain numbers are in sorted order
utility::vector1<core::Size>
get_chain_ids_from_chain(std::string const & chain, core::pose::Pose const & pose);

/// @brief Get all chain numbers for the residues with the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The returned chain numbers are in sorted order
utility::vector1<core::Size>
get_chain_ids_from_chain(char const & chain, core::pose::Pose const & pose);

/// @brief Attempt to get the chain number which correspond to the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// If the chain letter corresponds to more than one chain letter, raise an error
core::Size
get_chain_id_from_chain(std::string const & chain, core::pose::Pose const & pose);

/// @brief Attempt to get the chain number which correspond to the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// If the chain letter corresponds to more than one chain letter, raise an error
core::Size
get_chain_id_from_chain(char const & chain, core::pose::Pose const & pose);

/// @brief Get the chain letter for the first residue in a given chain number
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// Keep in mind that not all residues with the given chain number will necessarily have the returned chain letter
char
get_chain_from_chain_id(core::Size const & chain_id, core::pose::Pose const & pose);

/// @brief Attempt to get jump IDs which correspond to the given chain number
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The jumps here are the jumps which are *directly* upstream of a residue in the given chains,
/// (i.e. a residue on the given chain number is built directly from the jump) rather than logically upstream.
/// Return all jumps which build a given chain.
/// If no jumps directly builds the given chains (unlikely), return an empty set.
std::set<core::Size>
get_jump_ids_from_chain_ids(std::set<core::Size> const & chain_ids, core::pose::Pose const & pose);

/// @brief Attempt to get the jump number which correspond to the given chain number
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The jump here is the jump which is *directly* upstream of a residue in the given chain,
/// (i.e. a residue on the given chain number is built directly from the jump) rather than logically upstream.
/// If there's more than one jump which builds the given chain, return the smallest numbered jump
/// (even if it's not the jump which best partions the chain on the FoldTree).
/// If no jump directly builds the chain (unlikely), hard exit.
core::Size
get_jump_id_from_chain_id(core::Size const & chain_id, core::pose::Pose const & pose);

/// @brief Get all the jump numbers for the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The jumps here are the jumps which are *directly* upstream of a residue with a given chain letter,
/// (i.e. a residue with the given chain letter is built directly from the jump) rather than logically upstream.
/// Return all jumps which build residues with the given chain letter.
/// If no jumps directly builds residues with the given chain letters, return an empty vector.
///
/// The returned jump numbers are in sorted order
utility::vector1<core::Size>
get_jump_ids_from_chain(char const & chain, core::pose::Pose const & pose);

/// @brief Get all the jump numbers for the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The jumps here are the jumps which are *directly* upstream of a residue with a given chain letter,
/// (i.e. a residue with the given chain letter is built directly from the jump) rather than logically upstream.
/// Return all jumps which build residues with the given chain letter.
/// If no jumps directly builds residues with the given chain letters, return an empty vector.
///
/// The returned jump numbers are in sorted order
utility::vector1<core::Size>
get_jump_ids_from_chain(std::string const & chain, core::pose::Pose const & pose);

/// @brief Get the jump number for the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The jump here is the jumps which is *directly* upstream of a residue with a given chain letter,
/// (i.e. a residue with the given chain letter is built directly from the jump) rather than logically upstream.
/// If there's more than one jump which builds residues with the given chain letter, return the smallest numbered jump
/// (even if it's not the jump which best partions the chain on the FoldTree).
/// If no jump directly builds the chain, hard exit.
core::Size
get_jump_id_from_chain(std::string const & chain, core::pose::Pose const & pose);

/// @brief Get the jump number for the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The jump here is the jumps which is *directly* upstream of a residue with a given chain letter,
/// (i.e. a residue with the given chain letter is built directly from the jump) rather than logically upstream.
/// If there's more than one jump which builds residues with the given chain letter, return the smallest numbered jump
/// (even if it's not the jump which best partions the chain on the FoldTree).
/// If no jump directly builds the chain, hard exit.
core::Size
get_jump_id_from_chain(char const & chain, core::pose::Pose const & pose);

/// @brief Get the chain ID of the residue directly built by the given jump.
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// Keep in mind that not every residue with the returned chain ID will be downstream of this jump.
core::Size
get_chain_id_from_jump_id(core::Size const & jump_id, core::pose::Pose const & pose);

/// @brief Get the chain letter of the chain built by the given jump.
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// Keep in mind that not every residue with the returned chain ID will be downstream of this jump.
char
get_chain_from_jump_id(core::Size const & jump_id, core::pose::Pose const & pose);

/// @brief Get a vector of all residues numbers which are represented by this chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The returned residue numbers are in sorted order
utility::vector1<core::Size>
get_resnums_for_chain( core::pose::Pose const & pose, char chain );

/// @brief Get a vector of all residues numbers which are represented by this chain number
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
/// The returned residue numbers are in sorted order
utility::vector1<core::Size>
get_resnums_for_chain_id( core::pose::Pose const & pose, core::Size chain_id );

/// @brief Get all residues which correspond to the given chain number
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
core::conformation::ResidueCOPs
get_chain_residues(core::pose::Pose const & pose, core::Size chain_id);

/// @brief Get all residues which correspond to the given chain numbers
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
core::conformation::ResidueCOPs
get_residues_from_chains(core::pose::Pose const & pose, utility::vector1<core::Size> const & chain_ids);

/// @brief Does this residue number have this chain letter?
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
bool res_in_chain( core::pose::Pose const & pose, core::Size resnum, std::string const & chain );

core::Size num_heavy_atoms(
	core::Size begin,
	core::Size const end,
	core::pose::Pose const & pose
);

core::Size num_atoms(
	core::Size begin,
	core::Size const end,
	core::pose::Pose const & pose
);

core::Size num_hbond_acceptors(
	core::Size begin,
	core::Size const end,
	core::pose::Pose const & pose
);

core::Size num_hbond_donors(
	core::Size begin,
	core::Size const end,
	core::pose::Pose const & pose
);
core::Size
num_chi_angles(
	core::Size begin,
	core::Size const end,
	core::pose::Pose const & pose
);

core::Real
mass(
	core::Size begin,
	core::Size const end,
	core::pose::Pose const & pose
);

/// @brief Get a value representing the position of all the atoms for residues with the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
core::Size
get_hash_from_chain( char const & chain, core::pose::Pose const & pose, std::string const & extra_label="" );

/// @brief Get a value representing the position of all the atoms for residues which don't have the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
core::Size
get_hash_excluding_chain( char const & chain, core::pose::Pose const & pose, std::string const & extra_label="" );

/// @brief Get a value representing the position of all the atoms for residues with the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
std::string
get_sha1_hash_from_chain(char const & chain, core::pose::Pose const & pose, std::string const & extra_label="");

/// @brief Get a value representing the position of all the atoms for residues with the given chain letters
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
std::string
get_sha1_hash_from_chains(utility::vector1< std::string > const & chains, core::pose::Pose const & pose, std::string const & extra_label="");

/// @brief Get a value representing the position of all the atoms for residues which don't have the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
std::string
get_sha1_hash_excluding_chain(char const & chain, core::pose::Pose const & pose, std::string const & extra_label="");

/// @brief Get a value representing the position of all the atoms for residues which don't have the given chain letter
///
/// See the documentation of Pose::num_chains() for details about chain numbers, chain letters and jumps.
///
std::string
get_sha1_hash_excluding_chains(utility::vector1< std::string > const & chains, core::pose::Pose const & pose, std::string const & extra_label="");

/// @brief Initialize a DOF_ID_Map for a given Pose using the DOF_ID_Map's current default fill values
template< typename T >
void
initialize_dof_id_map( id::DOF_ID_Map< T > & dof_map, Pose const & pose );

/// @brief Initialize a DOF_ID_Map for a given Pose using a specified fill value
template< typename T >
void
initialize_dof_id_map( id::DOF_ID_Map< T > & dof_map, Pose const & pose, T const & value );

/// @brief returns a Distance
core::Real
pose_max_nbr_radius( pose::Pose const & pose );

/// @brief Initialize an AtomID_Map for a given Pose using the AtomID_Map's current default fill values
template< typename T >
void
initialize_atomid_map( id::AtomID_Map< T > & atom_map, pose::Pose const & pose );

/// @brief Initialize an AtomID_Map for a given Pose using a specified fill value
template< typename T >
void
initialize_atomid_map( id::AtomID_Map< T > & atom_map, pose::Pose const & pose, T const & value );

/// @brief Initialize an AtomID_Map for a given Conformation using the AtomID_Map's current default fill values
template< typename T >
void
initialize_atomid_map( id::AtomID_Map< T > & atom_map, conformation::Conformation const & conformation );

/// @brief Initialize an AtomID_Map for a given Conformation using a specified fill value
template< typename T >
void
initialize_atomid_map( id::AtomID_Map< T > & atom_map, conformation::Conformation const & conformation, T const & value );

/// @brief Initialize an AtomID_Map for a given Pose using the AtomID_Map's current default fill values
template< typename T >
void
initialize_atomid_map_heavy_only( id::AtomID_Map< T > & atom_map, pose::Pose const & pose );

/// @brief Initialize an AtomID_Map for a given Pose using a specified fill value
template< typename T >
void
initialize_atomid_map_heavy_only( id::AtomID_Map< T > & atom_map, pose::Pose const & pose, T const & value );

/// @brief Initialize an AtomID_Map for a given Conformation using the AtomID_Map's current default fill values
template< typename T >
void
initialize_atomid_map_heavy_only( id::AtomID_Map< T > & atom_map, conformation::Conformation const & conformation );

/// @brief Initialize an AtomID_Map for a given Conformation using a specified fill value
template< typename T >
void
initialize_atomid_map_heavy_only( id::AtomID_Map< T > & atom_map, conformation::Conformation const & conformation, T const & value );

/// @brief detect and fix disulfide bonds
void
initialize_disulfide_bonds(
	Pose & pose
);

/// @brief detect and fix disulfide bonds
void
initialize_disulfide_bonds(
	Pose & pose,
	io::StructFileRep const & fd
);

/// @brief Returns a string giving the pose's tag if there is such a thing or "UnknownTag" otherwise.
std::string extract_tag_from_pose( core::pose::Pose &pose );

/// @brief Create a sequence map of first pose onto the second, matching the PDBInfo
///    If the PDBInfo of either Pose is missing or invalid, do a simple sequence alignment matching.
core::id::SequenceMapping sequence_map_from_pdbinfo( Pose const & first, Pose const & second );

/// @brief count the number of canonical residues in the pose
core::Size canonical_residue_count(core::pose::Pose const & pose);

/// @brief count the number of non-canonical residues in the pose
core::Size noncanonical_residue_count(core::pose::Pose const & pose);

/// @brief count the number of canonical amino acid atoms in the pose
core::Size canonical_atom_count(core::pose::Pose const & pose);

/// @brief count the number of non-canonical amino acids in thepose
core::Size noncanonical_atom_count(core::pose::Pose const & pose);

/// @brief count the number of non-canonical chi angles in the pose
core::Size noncanonical_chi_count(core::pose::Pose const & pose);

/// @brief Number of protein residues in the pose
/// @details No virtuals, membrane residues or embedding residues counted
core::Size nres_protein( core::pose::Pose const & pose );

/// @brief Get the center of the indicated residues
///
/// WARNING: Despite the name, this function only calculates with a single coordinate per residue
/// (the Calpha/neighbor atom)
numeric::xyzVector< core::Real>
center_of_mass(
	core::pose::Pose const & pose,
	utility::vector1< bool > const & residues
);

/// @brief Get the center of the indicated residues
///
/// WARNING: Despite the name, this function only calculates with a single coordinate per residue
/// (the Calpha/neighbor atom)
numeric::xyzVector< core::Real>
center_of_mass(
	core::pose::Pose const & pose,
	int const start,
	int const stop
);

/// @brief Get the center of the indicated residues
///
/// This computes an equally-weighted, all-atom (including virtuals and hydrogens) center
core::Vector
all_atom_center(
	core::pose::Pose const & pose,
	utility::vector1< core::Size > const & residues
);

int
residue_center_of_mass(
	pose::Pose const & pose,
	utility::vector1< bool > residues
);

int
return_nearest_residue(
	pose::Pose const & pose,
	utility::vector1< bool > const & residues,
	Vector center
);

int
residue_center_of_mass(
	core::pose::Pose const & pose,
	int const start,
	int const stop
);

int
return_nearest_residue(
	core::pose::Pose const & pose,
	int const begin,
	int const end,
	core::Vector center
);

id::AtomID_Map< id::AtomID >
convert_from_std_map( std::map< id::AtomID, id::AtomID > const & atom_map, core::pose::Pose const & pose );

/// @brief Create std::map from PDBPoseMap
std::map< std::string, core::Size > get_pdb2pose_numbering_as_stdmap ( core::pose::Pose const & pose );

/// @brief Add cutpoint variants to all residues annotated as cutpoints in the FoldTree in the Pose.
void
correctly_add_cutpoint_variants( core::pose::Pose & pose );

/// @brief Add CUTPOINT_LOWER and CUTPOINT_UPPER types to two residues, remove incompatible types, and declare
/// a chemical bond between them.
/// @param[in] pose The pose to modify.
/// @param[in] cutpoint_res The index of the CUTPOINT_LOWER residue.
/// @param[in] check_fold_tree If true, a check is performed to confirm that the residues in question represent a
/// cutpoint in the foldtree in the pose.
/// @param[in] next_res_in The index of the CUTPOINT_UPPER residue.  If not provided, or if set to 0, this defaults
/// to the cutpoint_res + 1 residue.  Must be specified for cyclic geometry.
void
correctly_add_cutpoint_variants(
	core::pose::Pose & pose,
	Size const cutpoint_res,
	bool const check_fold_tree = true,
	Size const next_res_in = 0 );

/// @brief Remove variant types incompatible with CUTPOINT_LOWER from a position in a pose.
/// @author Vikram K. Mulligan (vmullig@uw.edu).
/// @param[in,out] pose The pose on which to operate.
/// @param[in] res_index The index of the residue on which to operate.
void
correctly_remove_variants_incompatible_with_lower_cutpoint_variant( core::pose::Pose & pose, Size const res_index );

/// @brief Remove variant types incompatible with CUTPOINT_UPPER from a position in a pose.
/// @author Vikram K. Mulligan (vmullig@uw.edu).
/// @param[in,out] pose The pose on which to operate.
/// @param[in] res_index The index of the residue on which to operate.
void
correctly_remove_variants_incompatible_with_upper_cutpoint_variant( core::pose::Pose & pose, Size const res_index );

/// @brief Create a chemical bond from lower to upper residue across CUTPOINT_LOWER/CUTPOINT_UPPER.
/// @details This will prevent steric repulsion.
/// @param[in] pose The pose to modify.
/// @param[in] cutpoint_res The index of the CUTPOINT_LOWER residue.
/// @param[in] next_res_in The index of the CUTPOINT_UPPER residue.  If not provided, or if set to 0, this defaults
/// to the cutpoint_res + 1 residue.  Must be specified for cyclic geometry.
void
declare_cutpoint_chemical_bond( core::pose::Pose & pose, Size const cutpoint_res, Size const next_res_in = 0 );

/// @brief Given a pose and a position that may or may not be CUTPOINT_UPPER or CUTPOINT_LOWER, determine whether this
/// position has either of these variant types, and if it does, determine whether it's connected to anything.  If it is,
/// update the C-OVL1-OVL2 bond lengths and bond angle (for CUTPOINT_LOWER) or OVU1-N bond length (for CUTPOINT_UPPER) to
/// match any potentially non-ideal geometry in the residue to which it's bonded.
/// @details Requires a little bit of special-casing for gamma-amino acids.  Throws an exception if the residue to which
/// a CUTPOINT_LOWER is bonded does not have an "N" and a "CA" or "C4".  Safe to call repeatedly, or if cutpoint variant
/// types are absent; in these cases, the function does nothing.
/// @note By default, this function calls itself again once on residues to which this residue is connected, to update their
/// geometry.  Set recurse=false to disable this.
/// @author Vikram K. Mulligan (vmullig@uw.edu).
void
update_cutpoint_virtual_atoms_if_connected( core::pose::Pose & pose, core::Size const cutpoint_res, bool recurse = true );

void
get_constraints_from_link_records( core::pose::Pose & pose, io::StructFileRep const & sfr );

/// @brief Convert PDB numbering to pose numbering. Must exist somewhere else, but I couldn't find it. -- rhiju
utility::vector1< Size > pdb_to_pose( pose::Pose const & pose, utility::vector1< int > const & pdb_res );

/// @brief Convert PDB numbering/chain to pose numbering. Must exist somewhere else, but I couldn't find it. -- rhiju
utility::vector1< Size > pdb_to_pose( pose::Pose const & pose, std::tuple< utility::vector1< int >, utility::vector1<char>, utility::vector1<std::string> > const & pdb_res );

/// @brief Convert PDB numbering to pose numbering. Must exist somewhere else, but I couldn't find it. -- rhiju
Size pdb_to_pose( pose::Pose const & pose, int const res_num, char const chain = ' ' );

/// @brief Convert pose numbering to pdb numbering. Must exist somewhere else, but I couldn't find it. -- rhiju
utility::vector1< Size > pose_to_pdb( pose::Pose const & pose, utility::vector1< Size > const & pose_res );

/// @brief returns true if the given residue in the pose is a chain ending or has upper/lower terminal variants
bool
pose_residue_is_terminal( Pose const & pose, Size const resid );

/// @brief checks to see if this is a lower chain ending more intelligently than just checking residue variants
bool
is_lower_terminus( pose::Pose const & pose, Size const resid );

/// @brief checks to see if this is a lower chain ending more intelligently than just checking residue variants
bool
is_upper_terminus( pose::Pose const & pose, Size const resid );


/// @brief  Is the query atom in this pose residue axial or equatorial to the given ring or neither?
chemical::rings::AxEqDesignation is_atom_axial_or_equatorial_to_ring(
	Pose const & pose,
	uint seqpos,
	uint query_atom,
	utility::vector1< uint > const & ring_atoms );

/// @brief  Is the query atom in this pose axial or equatorial to the given ring or neither?
/*chemical::rings::AxEqDesignation is_atom_axial_or_equatorial_to_ring(
Pose const & pose,
id::AtomID const & query_atom,
utility::vector1< id::AtomID > const & ring_atoms );*/


/// @brief  Is the query atom in this pose residue axial or equatorial or neither?
//chemical::rings::AxEqDesignation is_atom_axial_or_equatorial( Pose const & pose, uint seqpos, uint query_atom );

/// @brief  Is the query atom in this pose axial or equatorial or neither?
//chemical::rings::AxEqDesignation is_atom_axial_or_equatorial( Pose const & pose, id::AtomID const & query_atom );

/// @brief Set bfactors in a pose PDBInfo
void
set_bfactors_from_atom_id_map(Pose & pose, id::AtomID_Map< Real > const & bfactors);



///Set the BB torsion, phi, psi, omega (see core::types).
/// Works with carbohydrates.
/// Think about moving this to pose itself.
void
set_bb_torsion( uint torsion_id, Pose & pose, core::Size sequence_position, core::Angle new_angle);

///@brief Get a particular backbone torsion, phi, psi, omega (see core::types)
/// Works with carbohydrates.
/// Think about moving this to pose itself.
core::Angle
get_bb_torsion( uint torsion_id, Pose const & pose, core::Size sequence_position );


// Stepwise

void
fix_up_residue_type_variants_at_strand_beginning( core::pose::Pose & pose, core::Size const res );

void
fix_up_residue_type_variants_at_strand_end( core::pose::Pose & pose, core::Size const res );

void
fix_up_residue_type_variants( core::pose::Pose & pose );


bool
just_modeling_RNA( std::string const & sequence );

bool
stepwise_addable_pose_residue( core::Size const n /*in pose numbering*/,
	core::pose::Pose const & pose );

bool
stepwise_addable_residue( core::Size const n /*in full model numbering*/,
	std::map< core::Size, std::string > const & non_standard_residue_map );


////////////////////////////////////////////////////////////////////////////////////////////////
bool
effective_lower_terminus_based_on_working_res( Size const i,
	utility::vector1< Size > const & working_res,
	utility::vector1< Size > const & res_list,
	utility::vector1< Size > const & cutpoint_open_in_full_model );

bool
effective_upper_terminus_based_on_working_res( Size const i,
	utility::vector1< Size > const & working_res,
	utility::vector1< Size > const & res_list,
	utility::vector1< Size > const & cutpoint_open_in_full_model,
	Size const nres_full);

bool
definite_terminal_root( utility::vector1< Size > const & cutpoint_open_in_full_model,
	utility::vector1< Size > const & working_res,
	utility::vector1< Size > const & res_list,
	Size const nres,
	Size const i );

bool
definite_terminal_root( pose::Pose const & pose, Size const i );

Size
get_definite_terminal_root( pose::Pose const & pose,
	utility::vector1< Size > const & partition_res /* should not be empty */,
	utility::vector1< Size > const & res_list,
	utility::vector1< Size > const & fixed_domain_map /* 0 in free; 1,2,... for separate fixed domains */,
	utility::vector1< Size > const & cutpoint_open_in_full_model,
	utility::vector1< Size > const & working_res );

Size
get_definite_terminal_root( pose::Pose const & pose,
	utility::vector1< Size > const & partition_res /* should not be empty */ );

utility::vector1< Size >
reorder_root_partition_res(
	utility::vector1< Size > const & root_partition_res /* should not be empty */,
	utility::vector1< Size > const & res_list,
	utility::vector1< Size > const & fixed_domain_map /* 0 in free; 1,2,... for separate fixed domains */ );

void
reroot( pose::Pose & pose,
	utility::vector1< Size > const & root_partition_res /* should not be empty */,
	utility::vector1< Size > const & res_list,
	utility::vector1< Size > const & preferred_root_res /* can be empty */,
	utility::vector1< Size > const & fixed_domain_map /* 0 in free; 1,2,... for separate fixed domains */,
	utility::vector1< Size > const & cutpoint_open_in_full_model,
	utility::vector1< Size > const & working_res );

} // pose
} // core

#endif // INCLUDED_core_pose_util_HH
