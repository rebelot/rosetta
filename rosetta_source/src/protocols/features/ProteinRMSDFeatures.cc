// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/features/ProteinRMSDFeatures.cc
/// @brief  report comments stored with each pose
/// @author Matthew O'Meara

// Unit Headers
#include <protocols/features/ProteinRMSDFeatures.hh>

// Project Headers
#include <core/pose/util.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>


// Platform Headers
#include <basic/Tracer.hh>
#include <core/chemical/AA.hh>
#include <core/conformation/Residue.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pose/Pose.hh>
#include <core/types.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/rms_util.tmpl.hh>
#include <protocols/rosetta_scripts/util.hh>

// Utility Headers
#include <numeric/xyzVector.hh>
#include <utility/vector1.hh>
#include <utility/tag/Tag.hh>
#include <utility/sql_database/DatabaseSessionManager.hh>

// Basic Headers
#include <basic/options/keys/inout.OptionKeys.gen.hh>
#include <basic/database/sql_utils.hh>

// External Headers
#include <cppdb/frontend.h>

// Boost Headers
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

// C++ Headers
#include <string>
#include <map>
#include <list>
#include <sstream>

#include <utility/vector0.hh>


namespace protocols{
namespace features{

using std::string;
using std::list;
using std::endl;
using core::Size;
using core::import_pose::pose_from_pdb;
using core::pose::Pose;
using core::pose::PoseCOP;
using core::pose::PoseOP;
using protocols::filters::Filters_map;
using protocols::moves::DataMap;
using protocols::moves::Movers_map;
using protocols::rosetta_scripts::saved_reference_pose;
using utility::vector1;
using utility::sql_database::sessionOP;
using utility::tag::TagPtr;
using cppdb::statement;

static basic::Tracer tr("protocols.features.ProteinRMSDFeatures");

string
ProteinRMSDFeatures::type_name() const { return "ProteinRMSDFeatures"; }


PoseCOP
ProteinRMSDFeatures::reference_pose() const {
	return reference_pose_;
}

void
ProteinRMSDFeatures::reference_pose(
	PoseCOP pose
) {
	reference_pose_ = pose;
}

ProteinRMSDFeatures::ProteinRMSDFeatures(
	PoseCOP reference_pose ) :
	reference_pose_(reference_pose)
{}

string
ProteinRMSDFeatures::schema() const {
	string db_mode(basic::options::option[basic::options::OptionKeys::inout::database_mode]);

	if(db_mode == "sqlite3") {
		return
			"CREATE TABLE IF NOT EXISTS protein_rmsd (\n"
			"	struct_id INTEGER,\n"
			"	reference_tag TEXT,\n"
			"	protein_CA REAL,\n"
			"	protein_CA_or_CB REAL,\n"
			"	protein_backbone REAL,\n"
			"	protein_backbone_including_O REAL,\n"
			"	protein_backbone_sidechain_heavyatom REAL,\n"
			"	heavyatom REAL,\n"
			"	nbr_atom REAL,\n"
			"	all_atom REAL,\n"
			"	FOREIGN KEY (struct_id)\n"
			"		REFERENCES structures (struct_id)\n"
			"		DEFERRABLE INITIALLY DEFERRED,\n"
			"	PRIMARY KEY(struct_id, reference_tag));";
	}else if(db_mode == "mysql") {
		return
			"CREATE TABLE IF NOT EXISTS protein_rmsd (\n"
			"	struct_id INTEGER,\n"
			"	reference_tag VARCHAR(255),\n"
			"	protein_CA DOUBLE,\n"
			"	protein_CA_or_CB DOUBLE,\n"
			"	protein_backbone DOUBLE,\n"
			"	protein_backbone_including_O DOUBLE,\n"
			"	protein_backbone_sidechain_heavyatom DOUBLE,\n"
			"	heavyatom DOUBLE,\n"
			"	nbr_atom DOUBLE,\n"
			"	all_atom DOUBLE,\n"
			"	FOREIGN KEY (struct_id)\n"
			"		REFERENCES structures (struct_id),\n"
			"	PRIMARY KEY(struct_id, reference_tag));";
	}else {
		return "";
	}
}

utility::vector1<std::string>
ProteinRMSDFeatures::features_reporter_dependencies() const {
	utility::vector1<std::string> dependencies;
	dependencies.push_back("StructureFeatures");
	return dependencies;
}

void
ProteinRMSDFeatures::parse_my_tag(
	TagPtr const tag,
	DataMap & data,
	Filters_map const & /*filters*/,
	Movers_map const & /*movers*/,
	Pose const & pose
) {
	runtime_assert(tag->getOption<string>("name") == type_name());

	if(tag->hasOption("reference_name")){
		// Use with SavePoseMover
		// WARNING! reference_pose is not initialized until apply time
		reference_pose(saved_reference_pose(tag, data));
	} else {
		using namespace basic::options;
		if (option[OptionKeys::in::file::native].user()) {
			PoseOP ref_pose;
			string native_pdb_fname(option[OptionKeys::in::file::native]());
			pose_from_pdb(*ref_pose, native_pdb_fname);
			tr << "Adding features reporter '" << type_name() << "' referencing '"
				<< " the -in:file:native='" << native_pdb_fname << "'" << endl;
			reference_pose(ref_pose);
		} else {
			tr << "Setting '" << type_name() << "' to reference the starting structure." << endl;
			reference_pose(new Pose(pose));
		}
	}
}

Size
ProteinRMSDFeatures::report_features(
	Pose const & pose,
	vector1< bool > const & relevant_residues,
	Size struct_id,
	sessionOP db_session
){
	using namespace core::scoring;

	list< Size > subset_residues;
	for(Size i = 1; i <= relevant_residues.size(); ++i){
		if(relevant_residues[i]) subset_residues.push_back(i);
	}

	std::string statement_string = "INSERT INTO protein_rmsd VALUES (?,?,?,?,?,?,?,?,?,?);";
	statement stmt(basic::database::safely_prepare_statement(statement_string,db_session));

	stmt.bind(1,struct_id);
	stmt.bind(2,find_tag(*reference_pose_));
	stmt.bind(3,rmsd_with_super(*reference_pose_, pose, subset_residues, is_protein_CA));
	stmt.bind(4,rmsd_with_super(*reference_pose_, pose, subset_residues, is_protein_CA_or_CB));
	stmt.bind(5,rmsd_with_super(*reference_pose_, pose, subset_residues, is_protein_backbone));
	stmt.bind(6,rmsd_with_super(*reference_pose_, pose, subset_residues, is_protein_backbone_including_O));
	stmt.bind(7,rmsd_with_super(*reference_pose_, pose, subset_residues, is_protein_sidechain_heavyatom));
	stmt.bind(8,rmsd_with_super(*reference_pose_, pose, subset_residues, is_heavyatom));
	stmt.bind(9,rmsd_with_super(*reference_pose_, pose, subset_residues, is_nbr_atom));
	stmt.bind(10,all_atom_rmsd(*reference_pose_, pose, subset_residues));

	basic::database::safely_write_to_database(stmt);

	return 0;
}

} //namesapce
} //namespace
