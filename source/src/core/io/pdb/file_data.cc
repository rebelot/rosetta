// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/io/pdb/file_data.cc
/// @brief
/// @author Sergey Lyskov

// Unit headers
#include <core/io/pdb/Field.hh>
#include <core/io/pdb/HeaderInformation.hh>
#include <core/io/pdb/file_data.hh>

#include <core/io/pdb/pose_io.hh>
#include <core/types.hh>

#include <core/io/pdb/file_data_options.hh>
#include <core/io/pdb/pdb_dynamic_reader.hh>
#include <core/io/pdb/pdb_dynamic_reader_options.hh>
#include <core/pose/PDBInfo.hh>

#include <core/chemical/AA.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/Patch.hh>
#include <core/chemical/carbohydrates/CarbohydrateInfo.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/AtomTypeSet.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/AtomType.hh>

#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>

#include <core/io/raw_data/DisulfideFile.hh>

#include <core/scoring/dssp/Dssp.hh>
#include <core/scoring/cryst/util.hh>

#include <core/pose/util.hh>

// Basic headers
#include <basic/options/option.hh>
#include <basic/Tracer.hh>

// option key includes
#include <basic/options/option.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/inout.OptionKeys.gen.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>

#include <numeric/random/random.hh>

#include <utility/string_util.hh>
#include <utility/io/ozstream.hh>
#include <utility/io/izstream.hh>
#include <utility/exit.hh>

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <utility>
#include <ObjexxFCL/format.hh>

// option key includes
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/inout.OptionKeys.gen.hh>

#include <core/pose/util.hh>
#include <utility/vector1.hh>

//Auto Headers
#include <core/pose/util.tmpl.hh>

namespace core {
namespace io {
namespace pdb {

using core::Size;
using core::SSize;

using basic::T;
using basic::Error;
using basic::Warning;

using std::string;
using std::iostream;

using namespace ObjexxFCL;
using namespace ObjexxFCL::fmt;

// Tracer instance for this file
static basic::Tracer TR("core.io.pdb.file_data");

// random number generator for randomizing missing density coordinates
static numeric::random::RandomGenerator RG(231411);  // <- Magic number, do not change it!

// TODO: move this to core/chemical/types.hh
// TODO: Confirm that not allowing ' ' as a chain id is intended--as this is inconsistent with the PDB spec
static string const chr_chains( "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz" );

ResidueInformation::ResidueInformation() :
	resid( "" ),
	resName( "" ),
	chainID( ' ' ),
	resSeq( 0 ),
	iCode( ' ' ),
	terCount( 0 ),
	atoms(),
	xyz(),
	temps()
{}

ResidueInformation::ResidueInformation(
	AtomInformation const & ai) :
	resid( "" ),
	resName( ai.resName ),
	chainID( ai.chainID ),
	resSeq( ai.resSeq ),
	iCode( ai.iCode ),
	terCount( ai.terCount ),
	atoms(),
	xyz(),
	temps()
{}

bool
ResidueInformation::operator==(
	ResidueInformation const & that) const {
	return
		resName == that.resName &&
		chainID == that.chainID &&
		resSeq == that.resSeq &&
		iCode == that.iCode &&
		terCount == that.terCount;
}

bool
ResidueInformation::operator!=(
	ResidueInformation const & that) const {
	return !(*this == that);
}


/////////////////////////////////////////////

FileData::~FileData()
{
}

void
FileData::append_residue(
	core::conformation::Residue const & rsd,
	core::Size & atom_index,
	core::pose::Pose const & pose, // for pdb numbering and chains, could change to PDBInfo if necessary (but casting here is perhaps best)
	bool preserve_crystinfo
)
{
	using namespace core;

	//extract PDBInfo pointer
	pose::PDBInfoCOP pdb_info = pose.pdb_info();

	bool use_PDB(false);
	if (
			pdb_info
			&& !(pdb_info->obsolete())
			&& !(basic::options::option[ basic::options::OptionKeys::out::file::renumber_pdb ].value()) ) {
		use_PDB = true;
	}

	bool renumber_chains(false);
	if ( basic::options::option[ basic::options::OptionKeys::out::file::per_chain_renumbering ].value() ) {
		renumber_chains = true;
	}


	for ( Size j=1; j<= rsd.natoms(); ++j ) {
		conformation::Atom const & atom( rsd.atom(j) );

		//skip outputting virtual atom unless specified
		if ( !basic::options::option[ basic::options::OptionKeys::out::file::output_virtual ]() &&
				rsd.atom_type(j).is_virtual() ) continue;

		//fpd optionally don't output centroids
		if ( basic::options::option[ basic::options::OptionKeys::out::file::no_output_cen ]() &&
				rsd.atom_name(j) == " CEN" ) continue;

		// skip outputting zero occupancy atoms if specified
		if ( use_PDB && basic::options::option[ basic::options::OptionKeys::out::file::suppress_zero_occ_pdb_output ]() &&
			( rsd.seqpos() <= pdb_info->nres() ) ) {
			if ( pdb_info->occupancy( rsd.seqpos(), j ) < 0.0001 ) continue;
		}

		++atom_index;

		AtomInformation ai;
		AtomInformation orb;//have to initialize this out here.

		ai.isHet = (!rsd.is_polymer() || rsd.is_ligand());
		ai.serial = atom_index;
		ai.name = rsd.atom_name(j);
		ai.resName = rsd.name3();
		ai.x = atom.xyz()(1);
		ai.y = atom.xyz()(2);
		ai.z = atom.xyz()(3);
		ai.occupancy = 1.0; // dummy occupancy, can be overridden by PDBInfo

		// output with pdb specific info if possible
		if ( use_PDB && rsd.seqpos() <= pdb_info->nres() ) {
			// residue
			ai.chainID = pdb_info->chain( rsd.seqpos() );
			if ( ai.chainID == pose::PDBInfo::empty_record() ) { // safety
				TR.Warning << "PDBInfo chain id was left as character '" << pose::PDBInfo::empty_record()
					<< "' denoting empty record, for convenience replacing with space" << std::endl;
				ai.chainID = ' ';
			}
			ai.resSeq = pdb_info->number( rsd.seqpos() );
			ai.iCode = pdb_info->icode( rsd.seqpos() );

			// atom
			if ( pdb_info->is_het( rsd.seqpos(), j ) ) { // override standard het only if .is_het() is true
				ai.isHet = true;
			}
			ai.altLoc = pdb_info->alt_loc( rsd.seqpos(), j );
			ai.occupancy = pdb_info->occupancy( rsd.seqpos(), j );
			ai.temperature = pdb_info->temperature( rsd.seqpos(), j );

			//fpd: element
			if (preserve_crystinfo) {
				core::chemical::AtomTypeSet const &ats = rsd.type().atom_type_set();
				ai.element = ats[atom.type()].element();
				if (ai.element.length() == 1) ai.element = " "+ai.element;
			}
		} else {
			// residue
			runtime_assert( rsd.chain() > 0 );
			ai.chainID = chr_chains[ ( rsd.chain() - 1 ) % chr_chains.size() ];
			ai.resSeq = rsd.seqpos();

			// if option is specified, renumber per-chain
			if ( renumber_chains ) {
				utility::vector1< Size > const &chn_ends = pose.conformation().chain_endings();
				//for(int i=1; i<=chn_ends.size(); ++i) {
				for ( Size i=1; i<=chn_ends.size(); ++i ) {
					if (chn_ends[i] < rsd.seqpos()) ai.resSeq = rsd.seqpos() - chn_ends[i];
				}
			}

			// fix for >10k residues
			ai.resSeq = ai.resSeq % 10000;
		}

		// 'chains' is member data
		if ( chains.size() < Size(rsd.chain() + 1) ) chains.resize( rsd.chain() + 1 );
		AtomChain & AC(chains[rsd.chain()]);
		AC.push_back(ai);

	}
}
/// @details
/// init FileData structure from pose object.
/// read atoms/residue information from Pose object and put it in FileData object.
void FileData::init_from_pose(core::pose::Pose const & pose)
{
	FileDataOptions options;
	init_from_pose( pose, options );
}


///@details prepare the HeaderInformation data structure;
void
FileData::initialize_header_information() {
	header = new HeaderInformation();
}

HeaderInformationOP
FileData::header_information() const {
	return header;
}

///@details Store information in the header record into the HeaderInformation
///@remarks HeaderInformation must be created explicitly before it can be filled!
void
FileData::store_header_record(Record & R) {
	header->store_record(R);
}

///@details Populate the header records from the data in the HeaderInformation
///@remarks HeaderInformation must be created explicitly before it can be filled!
void
FileData::fill_header_records(
	std::vector<Record> & VR
) const {
	header->fill_records(VR);
}

///@details finalize storing records from the data in the HeaderInformation
///@remarks HeaderInformation must be created explicitly before it can be filled!
void
FileData::finalize_header_information() {
	header->finalize_parse();

}


// Store (non-standard) polymer linkages in a map.
void
FileData::store_link_record(Record & record)
{
	using namespace std;
	using namespace utility;

	LinkInformation link;
	vector1<LinkInformation> links;

	// Extract values from record fields.
	link.name1_ = record["name1"].value;  // 1st atom name
	link.resName1_ = record["resName1"].value;
	link.resID1_ = record["resSeq1"].value + record["iCode1"].value + record["chainID1"].value;

	link.name2_ = record["name2"].value;  // 2nd atom name
	link.resName2_ = record["resName2"].value;
	link.resID2_ = record["resSeq2"].value + record["iCode2"].value + record["chainID2"].value;

	link.length_ = atof(record["length"].value.c_str());  // bond length

	// If key is found in the links map, add this new linkage information to the links already keyed to this residue.
	if (link_map.count(link.resID1_)) {
		links = link_map[link.resID1_];
	}
	links.push_back(link);

	link_map[link.resID1_] = links;

	TR.Debug << "LINK record information stored successfully." << std::endl;
}


// Store heterogen name information in map.
/// @remarks  heterogen "names" for carbohydrates (from "Rosetta-ready" PDB files) instead have the name field parsed
/// to extract the base (non-variant) ResidueType needed for a particular residue.
void
FileData::store_heterogen_names(std::string const & hetID, std::string const & text)
{
	using namespace std;
	using namespace core::chemical::carbohydrates;

	if (hetID.empty()) {
		TR.Warning << "PDB HETNAM record is missing an heterogen ID field." << endl;
		return;
	}
	if (text.empty()) {
		TR.Warning << "PDB HETNAM chemical name field is an empty string." << endl;
		return;
	}

	string name;
	if (heterogen_names.count(hetID)) {
		name = heterogen_names[hetID];
		name.append(rstripped_whitespace(text));
	} else {
		name = text;
		strip_whitespace(name);
	}

	// If the hetID is found in the map of Rosetta-allowed carbohydrate 3-letter codes....
	if (CarbohydrateInfo::CODE_TO_ROOT_MAP.count(hetID)) {
		parse_heterogen_name_for_carbohydrate_residues(name);
	} else {
		heterogen_names[hetID] = name;  // Non-carbohydrate heterogen names are simply stored in the standard PDB way.
	}
}

// Parse heterogen name data for a given carbohydrate and save the particular base (non-variant) ResidueType needed in
// a map.
/// @details  The standard PDB HETNAM record is insufficient for indicating the type of carbohydrate residue found at
/// each position of the sequence.  The PDB format only allows one heterogen name per 3-letter code.  To work around
/// this, a PDB file containing carbohydrates will need to be made "Rosetta-ready".  3-letter codes will need to be
/// converted from, e.g., GLC, which implies the vague "ALPHA-D-GLUCOSE", to Glc.  When Rosetta reads in a "sentence
/// case" code such as this, it will check a resID-to-ResidueType map to determine which type of alpha-D-glucose to
/// use, e.g., ->4)-alpha-D-glucopyranosyl or ->6)-alpha-D-glucopyranosyl or ->4)-alpha-D-glucofuranosyl, etc.  This
/// function fills that map from a "Rosetta-ready" HETNAM text field, which includes the resID information (in the
/// same order as in an ATOM or HETATOM record) followed by a space and the base (non-variant) ResidueType.
void
FileData::parse_heterogen_name_for_carbohydrate_residues(std::string const & text)
{
	using namespace std;

	string chainID = string(text.begin(), text.begin() + 1);  // 1 character for chainID
	string resSeq = string(text.begin() + 1, text.begin() + 5);  // 4 characters for resSeq
	string iCode = string(text.begin() + 5, text.begin() + 6);  // 1 character for iCode
	string key = resSeq + iCode + chainID;  // a resID, as defined elsewhere in FileData

	string needed_residue_type_base_name = string(text.begin() + 7, text.end());  // name starts after 7th character

	carbohydrate_residue_type_base_names[key] = needed_residue_type_base_name;
}


/// @details
/// init FileData structure from pose object.
/// read atoms/residue information from Pose object and put it in FileData object using options defined in FileDataOptions.
void FileData::init_from_pose(core::pose::Pose const & pose, FileDataOptions const & options)
{
	using namespace core;
	core::Size const nres( pose.total_residue() );
	core::Size atom_index(0);

	//get OP to PDBInfo object for remarks header
	using core::pose::PDBInfo;
	if( (options.preserve_header() == true || options.preserve_crystinfo() == true ) && pose.pdb_info() ) {
		*remarks = pose.pdb_info()->remarks();
		if(pose.pdb_info()->header_information()){
			header = new HeaderInformation(*(pose.pdb_info()->header_information()));
		} else {
			header = new HeaderInformation();
		}
	}

	chains.resize(0);
	if ( options.preserve_crystinfo() && pose.pdb_info() ) {
		crystinfo = pose.pdb_info()->crystinfo();
	}

	for ( Size i=1; i<= nres; ++i ) {
		conformation::Residue const & rsd( pose.residue(i) );
		append_residue( rsd, atom_index, pose, options.preserve_crystinfo() );
	}
}

/// @details
/// a lightweight, direct way of limiting pose pdb output to a subset of residues
/// the alternative of constructing new subposes for output only would be unnecessary/less efficient (?)
void FileData::init_from_pose(
	core::pose::Pose const & pose,
	utility::vector1< core::Size > const & residue_indices
)
{
	using namespace core;
	core::Size const nres( pose.total_residue() );
	core::Size atom_index(0);

	chains.resize(0); // 'chains' is member data
	for ( utility::vector1< Size >::const_iterator index( residue_indices.begin() ),
			end( residue_indices.end() ); index != end; ++index ) {
		if ( *index < 1 || *index > nres ) { runtime_assert(false); continue; }
		append_residue( pose.residue( *index ), atom_index, pose );
	}
}


/// @details Convert given Pose object in to PDB format and send it to the given stream.
void FileData::dump_pdb(
	core::pose::Pose const & pose,
	std::ostream & out,
	string const & /* tag */,
	bool write_fold_tree
)
{
	string data;
	FileData fd;
	fd.init_from_pose(pose);

	data = PDB_DReader::createPDBData(fd);
	out.write( data.c_str(), data.size() );

	write_additional_pdb_data( out, pose, fd, write_fold_tree );
}

/// @details Convert given Pose object in to PDB format and save it to 'file_name' file.
/// return: true if operation was completed without error, false other wise.
bool FileData::dump_pdb(
	core::pose::Pose const & pose,
	string const & file_name,
	string const & tag,
	bool write_fold_tree)
{
	utility::io::ozstream file(file_name.c_str(), std::ios::out | std::ios::binary);
	if(!file) {
		Error() << "FileData::dump_pdb: Unable to open file:" << file_name << " for writing!!!" << std::endl;
		return false;
	}
	dump_pdb(pose, file, tag, write_fold_tree);

	file.close();

	return true;
}

/// @details Convert given Pose object in to PDB format and send it to the given stream.
/// only the residues corresponding to indices in 'residue_indices' will be output
void
FileData::dump_pdb(
	core::pose::Pose const & pose,
	std::ostream & out,
	utility::vector1< core::Size > const & residue_indices,
	string const & /* tag */
)
{
	FileData fd;
	string data;
	fd.init_from_pose( pose, residue_indices );
//	data = "MODEL     " + tag + "\n";
//	out.write( data.c_str(), data.size() );

	data = PDB_DReader::createPDBData(fd);
	out.write( data.c_str(), data.size() );

//	data = "ENDMDL\n";
//	out.write( data.c_str(), data.size() );

	write_additional_pdb_data( out, pose, fd );
}


/// @details Debug/Info function.
/// Output FileData object to TR like stream in human redable format.
std::ostream& operator <<(std::ostream &os, FileData const & fd)
{
	os << "<FileData>{";
	for(Size i=0; i<fd.chains.size(); i++) {
		os << "Chain<" << i << ">";
		for(Size j=0; j<fd.chains[i].size(); j++) {
			os << "[" << j << ":" << fd.chains[i][j] << "]" << "\n";
		}
	}
	os << "}";
	return os;
}

/// @details  Temporary hacky hack
/// Need better mechanism for this
std::string
convert_res_name( std::string const & name )
{
	if      ( name == " DA" ) return "  A";
	else if ( name == " DC" ) return "  C";
	else if ( name == " DG" ) return "  G";
	else if ( name == " DT" ) return "  T";
	else if ( name == " Ad" ) return "  A";
	else if ( name == " Cd" ) return "  C";
	else if ( name == " Gd" ) return "  G";
	else if ( name == " Td" ) return "  T";
	else if ( name == "MSE" ) {
		TR << "Reading MSE as MET!" << std::endl;
		return "MET";
	}
	return name;
}

std::string
convert_atom_name( std::string const & res_name, std::string atom_name )
{
	if( atom_name.size() != 4 ){
		std::string message= res_name+" has atom "+ atom_name+", with size!=4";
		utility_exit_with_message(message);
	};
	//atom_name = strip_whitespace( atom_name );
	if ( res_name == "5MC" ||
			res_name == "  A" ||
			res_name == "  C" ||
			res_name == "  G" ||
			res_name == "  T" ||
			res_name == "  U" ) {
		/// DNA or RNA
		if ( atom_name == " OP1" ) return " O1P";
		if ( atom_name == " OP2" ) return " O2P";
		if ( atom_name[3] == '\'' ) return atom_name.substr(0,3)+"*";
		if ( res_name == "  T" && atom_name == " C7 " ) return " C5M";
	} else if ( res_name == "MET" && atom_name == " S  " ) {
		return " SD ";
	} else if ( res_name == "MET" && atom_name == "SE  " ) {
		TR << "Reading Selenium SE from MSE as SD from MET" << std::endl;
		return " SD ";
	}
	return atom_name;
}

/// @details Convert FileData in to set of residues, sequences, coordinates.
/// this is a convenience function, no magic done here.
/// Well, maybe a little.
void FileData::create_working_data(
	utility::vector1< ResidueInformation > & rinfo
)
{
	FileDataOptions options;
	create_working_data( rinfo, options );
}

/// @details Convert FileData in to set of residues, sequences, coordinates.
/// this is a convenience function, no magic done here.
/// Well, maybe a little.
void FileData::create_working_data(
	utility::vector1< ResidueInformation > & rinfo,
	FileDataOptions const & options
)
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	rinfo.clear();
	std::string buf;  buf.resize(1024);

	for(Size ch=0; ch<chains.size(); ch++) {
		for(Size i=0; i<chains[ch].size(); i++) {
			AtomInformation & ai( chains[ch][i] );
			// we should make a copy instead of taking a reference if "fixing" the names causes problems
			std::string const  res_name( convert_res_name( ai.resName ) );
			std::string const atom_name( convert_atom_name( res_name, ai.name ) );
			ai.resName = res_name;
			ai.name = atom_name;

			sprintf(&buf[0], "%4d%c%c", ai.resSeq, ai.iCode, ai.chainID);
			std::string resid( buf ); // include chain ID
			resid.resize(6);

			//chu modify the logic how atoms are treated with zero or negative occupancy field.
			if ( ai.occupancy == 0.0 ) {
				if( options.randomize_missing_coords() ) {
					randomize_missing_coords( ai );
				} else if ( !options.ignore_zero_occupancy() ) {
					// do nothing and keep this atom as it is
				} else {
					//When flag default changes from true to false, change to TR.Debug and remove second line
					TR.Warning << "PDB reader is ignoring atom " << atom_name << " in residue " << resid
										 << ".  Pass flag -ignore_zero_occupancy false to change this behavior" << std::endl;
					continue; // skip this atom with zero occ by default
				}
			} else if ( ai.occupancy < 0.0 ) { // always randomize coords for atoms with negative occ
				randomize_missing_coords( ai );
			} else {
				// do nothing for normal atoms with positive occ
			}

			ResidueInformation new_res( ai );
			new_res.resid = resid;
			if( rinfo.size() == 0 || rinfo.back() != new_res ) rinfo.push_back(new_res);
			ResidueInformation & curr_res = rinfo.back();
			// Only insert atoms once, so we capture just the first alt conf.
			// Would be nice in the future to take the highest occupancy instead...
			if( curr_res.xyz.count(ai.name) == 0 ) {
				curr_res.atoms.push_back(ai); // this *does* make a copy
				Vector coords( ai.x, ai.y, ai.z );
				curr_res.xyz[ ai.name ] = coords;
				curr_res.temps[ ai.name ] = ai.temperature;
			}
		}
	}
}


// Helper Functions
/// @details Remove spaces from given string.
inline std::string local_strip_whitespace( std::string const & name )
{
	std::string trimmed_name( name );
	left_justify( trimmed_name ); trim( trimmed_name ); // simpler way to dothis?
	return trimmed_name;
}


/// @brief The missing density regions in the input pdb should have 0.000 in the placeholders
/// this routine puts random coordinates wherever there is 0.000 for mainchain atoms.
/// tex - that's a stupid way of defining missing density, as atoms can be at the origin for other
/// reasons. This has been updated to check for occupancy to define missing density rather than atoms
/// located at the origin.
void FileData::randomize_missing_coords( AtomInformation & ai ) {
	//	if( ai.resSeq == 1 && ai.name == " N  ") return;//ignore first atom. Rosetta pdbs start with 0.000
	if ( ai.x == 0.000 && ai.y == 0.000 && ai.z == 0.000 && ai.occupancy <= 0.0 ){
		TR << "Randomized: " << ai.name << " " << ai.resName << "  " << ai.resSeq << std::endl;
		//v		if ( ai.name == " N  " || ai.name == " CA " || ai.name == " C  " ||
		//v			ai.name == " O  " || ai.name == " CB " ) {
		ai.x = ai.x + 900.000 + RG.uniform()*100.000;
		ai.y = ai.y + 900.000 + RG.uniform()*100.000;
		ai.z = ai.z + 900.000 + RG.uniform()*100.000;
		//v		}
	}
	return;
}

/// @brief Writes
void
write_additional_pdb_data(
	std::ostream & out,
	pose::Pose const & pose,
	io::pdb::FileData const &,
	bool write_fold_tree
)
{

	using namespace basic::options;

	// added by rhiju --> "CONECT" lines. Useful for coarse-grained/centroid poses, so that
	//  rasmol/pymol draws bonds between atoms 'bonded' in Rosetta that are far apart.
	//  perhaps turn on with a flag?
	if ( (pose.total_residue() >= 1 && pose.residue(1).is_coarse()) || option[ OptionKeys::inout::dump_connect_info]() )  dump_connect_info( pose, out );

	if ( write_fold_tree || option[ OptionKeys::inout::fold_tree_io ].user() ) {
		out << "REMARK " << pose.fold_tree();
	}
	if ( basic::options::option[ OptionKeys::out::file::pdb_parents]() ) {
		std::string value;
		bool has_parents = core::pose::get_comment( pose, "parents", value );
		if( has_parents ){
			out << "REMARK PARENT    " << value.substr(0,5) << std::endl;
		}
	}
		if ( basic::options::option[ OptionKeys::out::file::pdb_comments]() ) {
	 	                        out << "##Begin comments##" << std::endl;
	 	                        using namespace std;
	 	                        map< string, string > const comments = core::pose::get_all_comments(pose);
	 	                        for( std::map< string, string >::const_iterator i = comments.begin(); i != comments.end(); ++i ){
	 	                                out << i->first<<" "<<i->second << std::endl;
	 	                        }
	 	                        out << "##End comments##" << std::endl;
	 }

	if(basic::options::option[ basic::options::OptionKeys::out::file::output_orbitals]){
		static std::string const chains( " ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890" );
		for(core::Size i=1; i <=pose.n_residue(); ++i){
			core::conformation::Residue rsd(pose.residue(i));
			core::Size number(0);
			char const chain( chains[ rsd.chain() ] );
			for(core::Size j=1; j<=rsd.natoms(); ++j){
				if(rsd.atom_type(j).atom_has_orbital()){
					utility::vector1<core::Size> const & orbital_indices(rsd.bonded_orbitals(j));
					for(
							utility::vector1<core::Size>::const_iterator
							orbital_index = orbital_indices.begin(),
							orbital_index_end = orbital_indices.end();
							orbital_index != orbital_index_end; ++orbital_index
					){
						++number;
						Vector orbital_xyz(rsd.orbital_xyz(*orbital_index));
						out << "ATOM  " << I(5,number) << ' ' << rsd.orbital_name(*orbital_index) << ' ' <<
								rsd.name3() << ' ' << chain << I(4,rsd.seqpos() ) << "    " <<
								F(8,3,orbital_xyz.x()) <<
								F(8,3,orbital_xyz.y()) <<
								F(8,3,orbital_xyz.z()) <<
								F(6,2,1.0) << F(6,2,1.0) << '\n';
					}
				}
			}
		}
	}
	if (basic::options::option[ basic::options::OptionKeys::out::file::output_torsions ]){
		if ( !core::pose::is_ideal_pose(pose) ) {
			TR << "Ignoring out::file::output_torsions option because pose is non-ideal!" << std::endl;
		} else {
			ObjexxFCL::FArray1D_char dssp_reduced_secstruct(pose.n_residue());
			scoring::dssp::Dssp(pose).dssp_reduced(dssp_reduced_secstruct);
			out << "REMARK torsions: res pdbres pdbchain seq dssp phi psi omega" << std::endl;
			for (core::Size i=1; i<=pose.n_residue(); ++i) {
				out << "REMARK " << I( 4, i ) << " " << I( 4, pose.pdb_info()->number(i)) << " " << pose.pdb_info()->chain(i) << " " << pose.residue( i ).name1() << " " <<
					dssp_reduced_secstruct(i) << " " << F( 9, 3, pose.phi(i)) << " " << F( 9, 3, pose.psi(i)) << " " << F( 9, 3, pose.omega(i)) << std::endl;
			}
		}
	}
}

void
build_pose_from_pdb_as_is(
	pose::Pose & pose,
	std::string const & filename
)
{
	PDB_DReaderOptions options;
	build_pose_from_pdb_as_is( pose, filename, options );
}

void
build_pose_from_pdb_as_is(
	pose::Pose & pose,
	std::string const & filename,
	PDB_DReaderOptions const & pdr_options
)
{
	using namespace chemical;
	build_pose_from_pdb_as_is( pose, * ChemicalManager::get_instance()->residue_type_set( FA_STANDARD ), filename, pdr_options );
}

void
build_pose_from_pdb_as_is(
	pose::Pose & pose,
	chemical::ResidueTypeSet const & residue_set,
	std::string const & filename
)
{
	PDB_DReaderOptions options;
	build_pose_from_pdb_as_is( pose, residue_set, filename, options );
}

void
build_pose_from_pdb_as_is(
	pose::Pose & pose,
	chemical::ResidueTypeSet const & residue_set,
	std::string const & filename,
	PDB_DReaderOptions const & pdr_options
)
{
	std::string all_lines, sub_lines;

	utility::io::izstream file( filename );
	if (!file) {
		TR.Error << "File:" << filename << " not found!" << std::endl;
		utility_exit_with_message( "Cannot open file " + filename );
	} else {
		TR.Debug << "read file: " << filename << std::endl;
	}

	utility::slurp( file, all_lines );
	FileData fd = PDB_DReader::createFileData( all_lines, pdr_options );
	if ( fd.filename == "" ) {
		fd.filename = filename;
	}
	id::AtomID_Mask missing( false );
	build_pose_as_is1( fd, pose, residue_set, missing, pdr_options);
}

void
build_pose_as_is1(
	io::pdb::FileData & fd,
	pose::Pose & pose,
	chemical::ResidueTypeSet const & residue_set,
	id::AtomID_Mask & missing,
	FileDataOptions const & options
)
{
	typedef std::map< std::string, double > ResidueTemps;
	typedef std::map< std::string, ResidueTemps > Temps;
	typedef std::map< std::string, Vector > ResidueCoords;
	typedef std::map< std::string, ResidueCoords > Coords;
	typedef utility::vector1< std::string > Strings;
	typedef numeric::xyzVector< double > Vector;

	using namespace chemical;
	using namespace conformation;

	// reset current data
	pose.clear();

	utility::vector1< ResidueInformation > rinfos;
	// Map pose residue numbers to indices into rinfos.
	// Some residues in the input file may be discarded (missing atoms, unrecognized, etc)
	utility::vector1< Size > pose_to_rinfo;
	fd.create_working_data( rinfos, options );
	Strings pose_resids;
	utility::vector1<ResidueTemps> pose_temps;

	Strings branch_lower_termini;
	Strings branch_points;

	Size const nres_pdb( rinfos.size() );

	utility::vector1<Size> UA_res_nums;
	utility::vector1<std::string> UA_res_names, UA_atom_names;
	utility::vector1<numeric::xyzVector<Real> > UA_coords;
	utility::vector1<core::Real> UA_temps;

	std::string chains_whose_residues_are_separate_chemical_entities =
			options.chains_whose_residues_are_separate_chemical_entities();
	std::string::const_iterator const entities_begin = chains_whose_residues_are_separate_chemical_entities.begin();
	std::string::const_iterator const entities_end = chains_whose_residues_are_separate_chemical_entities.end();

	std::string chains_to_check_if_Ntermini= options.check_if_residues_are_Ntermini() ;
	std::string::const_iterator const check_Ntermini_begin = chains_to_check_if_Ntermini.begin();
	std::string::const_iterator const check_Ntermini_end = chains_to_check_if_Ntermini.end();
	std::string chains_to_check_if_Ctermini= options.check_if_residues_are_Ctermini() ;
	std::string::const_iterator const check_Ctermini_begin = chains_to_check_if_Ctermini.begin();
	std::string::const_iterator const check_Ctermini_end = chains_to_check_if_Ctermini.end();

	//mjo do not add residue by bond if the last residue was not recognized
	bool last_residue_was_recognized(true);

	// Loop over every residue in the FileData extracted from the PDB file, select appropriate ResidueTypes,
	// create Residues, and build the Pose.
	for ( Size i=1; i<= nres_pdb; ++i ) {
		ResidueInformation const & rinfo = rinfos[i];
		std::string const & pdb_name = rinfo.resName;
		std::string const & resid = rinfo.resid;
		char chainID = rinfo.chainID;

		runtime_assert( resid.size() == 6 );
		bool const separate_chemical_entity = find(entities_begin, entities_end, chainID ) !=  entities_end;
		bool const same_chain_prev = ( i > 1        && chainID == rinfos[i-1].chainID &&
				rinfo.terCount == rinfos[i-1].terCount && !separate_chemical_entity);
		bool const same_chain_next = ( i < nres_pdb && chainID == rinfos[i+1].chainID &&
				rinfo.terCount == rinfos[i+1].terCount && !separate_chemical_entity);
		bool const check_Ntermini_for_this_chain = ("ALL" == chains_to_check_if_Ntermini) ?
				true : find(check_Ntermini_begin, check_Ntermini_end, chainID ) ==  check_Ntermini_end;
		bool const check_Ctermini_for_this_chain = ("ALL" == chains_to_check_if_Ctermini) ?
				true : find(check_Ctermini_begin, check_Ctermini_end, chainID ) ==  check_Ctermini_end;

		// Determine polymer information: termini, branch points, etc.
		bool const is_branch_point = fd.link_map.count(resid);  // if found in the linkage map
		if (is_branch_point) {
			// Find and store to access later:
			//     associated 1st residue of all branches off this residue (determines branch lower termini)
			//     positions of branch points
			for (Size branch = 1, n_branches = fd.link_map[resid].size(); branch <= n_branches; ++branch) {
				branch_lower_termini.push_back(fd.link_map[resid][branch].resID2_);
				branch_points.push_back(fd.link_map[resid][branch].name1_);  // This only matters for saccharides.
			}
		}
		bool const is_branch_lower_terminus = branch_lower_termini.contains(resid);
		bool const is_lower_terminus( ( i == 1 || rinfos.empty() || (!same_chain_prev && !is_branch_lower_terminus) )
				&& check_Ntermini_for_this_chain );
		bool const is_upper_terminus( ( i == nres_pdb || !same_chain_next ) && check_Ctermini_for_this_chain );

		TR.Debug << "Residue " << i << std::endl;
		if (is_lower_terminus) {
			TR.Debug << "...is a lower terminus." << std::endl;
		}
		if (is_upper_terminus) {
			TR.Debug << "...is an upper terminus." << std::endl;
		}
		if (is_branch_point) {
			TR.Debug << "...is a branch point." << std::endl;
		}
		if (is_branch_lower_terminus) {
			TR.Debug << "...is the lower terminus of a branch." << std::endl;
		}

		ResidueCoords const & xyz = rinfo.xyz;
		ResidueTemps  const & rtemp = rinfo.temps;

		// Get a list of ResidueTypes that could apply for this particular 3-letter PDB residue name.
		ResidueTypeCOPs const & rsd_type_list( residue_set.name3_map( pdb_name ) );
		if(!is_residue_type_recognized(
				i, pdb_name, rsd_type_list, xyz, rtemp,
				UA_res_nums, UA_res_names, UA_atom_names, UA_coords, UA_temps, options)) {
			last_residue_was_recognized = false;
			continue;
		}

		// look for best match:
		// rsd_type should have all the atoms present in xyz
		// try to minimize atoms missing from xyz
		Size best_index(0), best_rsd_missing( 99999 ), best_xyz_missing( 99999 );

		for ( Size j=1; j<= rsd_type_list.size(); ++j ) {
			ResidueType const & rsd_type( *(rsd_type_list[j]) );
			bool const is_polymer( rsd_type.is_polymer() ); // need an example residue type, though this will
			// remain fixed for all residue_types with the same name3

			//TR.Debug << rsd_type.name() << " is_polymer " << is_polymer << std::endl;
			//TR.Debug << rsd_type.name() << " is_lower_terminus " << rsd_type.has_variant_type( LOWER_TERMINUS ) << std::endl;
			//TR.Debug << rsd_type.name() << " is_upper_terminus " << rsd_type.has_variant_type( UPPER_TERMINUS ) << std::endl;

			// only take the desired variants
			bool lower_term_type = rsd_type.has_variant_type( LOWER_TERMINUS ) ||
					rsd_type.has_variant_type( LOWERTERM_TRUNC );
			bool upper_term_type = rsd_type.has_variant_type( UPPER_TERMINUS ) ||
					rsd_type.has_variant_type( UPPERTERM_TRUNC );
			if ( is_polymer && (
				(is_lower_terminus != lower_term_type ) || (is_upper_terminus != upper_term_type ) ) ) {
				TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
				//TR.Debug << "because of the terminus state" << std::endl;
				continue;
			}
			if (is_polymer && (is_branch_point != rsd_type.has_variant_type(BRANCH_POINT))) {
				TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
				//TR.Debug << "because of the branch state" << std::endl;
				continue;
			}
			if (is_polymer && (is_branch_lower_terminus != rsd_type.has_variant_type(BRANCH_LOWER_TERMINUS))) {
				TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
				//TR.Debug << "because of the branch lower terminus state" << std::endl;
				continue;
			}
			if ( rsd_type.aa() == aa_cys && rsd_type.has_variant_type( DISULFIDE ) && pdb_name != "CYD" ) {
				TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
				//TR.Debug << "because of the disulfide state" << std::endl;
				continue;
			}
			if ( !options.keep_input_protonation_state() &&
				( rsd_type.has_variant_type( PROTONATED ) || rsd_type.has_variant_type( DEPROTONATED ) )){
				TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
				//TR.Debug << "because of the protonation state" << std::endl;
				continue;
			}

			// special checks to ensure selecting the proper carbohydrate ResidueType
			if (rsd_type.is_carbohydrate() && residue_type_base_name(rsd_type) !=
					fd.carbohydrate_residue_type_base_names[resid]) {
				TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
				//TR.Debug << "because the residue is not a carbohydrate" << std::endl;
				continue;
			}
			if (rsd_type.is_carbohydrate() && rsd_type.has_variant_type(BRANCH_POINT)) {
				// The below assumes that ResidueTypes with fewer patches are selected 1st, that is, that an
				// _p:->2-branch ResidueType will be checked as a possible match before an _p:->2-branch_p:->6-branch
				// ResidueType.  If this were not the case, Rosetta could misassign an _p:->2-branch_p:->6-branch
				// ResidueType to a residue that actually only has a single branch at the 2 or 6 position.
				char branch_point;
				bool branch_point_is_missing = false;
				for (Size k = 1, n_branch_points = branch_points.size();
						k <= n_branch_points; ++k) {
					branch_point = branch_points[k][2];  // 3rd column (index 2) is the atom number in all saccharides
					TR.Debug << "Checking '" << rsd_type.name() <<
							"' for branch at position " << branch_point << std::endl;
					if (residue_type_all_patches_name(rsd_type).find(string(1, branch_point) + ")-branch") ==
							string::npos) {
						branch_point_is_missing = true;
						break;
					}
				}
				if (branch_point_is_missing) {
					TR.Debug << "Discarding '" << rsd_type.name() << "' ResidueType" << std::endl;
					//TR.Debug << "because of a missing branch point" << std::endl;
					continue;
				}
			}

			TR.Debug << "Trying '" << rsd_type.name() << "' ResidueType" << std::endl;

			Size rsd_missing(0), xyz_missing(0);

			for ( Size k=1; k<= rsd_type.natoms(); ++k ) {
				if ( xyz.count( rsd_type.atom_name(k) ) == 0 ) ++xyz_missing;
			}

			for ( ResidueCoords::const_iterator iter=xyz.begin(), iter_end=xyz.end(); iter!= iter_end; ++iter ) {
				if ( !rsd_type.has( local_strip_whitespace(iter->first) ) &&
						!( iter->first == " H  " && is_lower_terminus ) ) { // don't worry about missing BB H if Nterm
					++rsd_missing;
				}
			}

			if ( ( rsd_missing < best_rsd_missing ) ||
					( rsd_missing == best_rsd_missing && xyz_missing < best_xyz_missing ) ) {
				best_rsd_missing = rsd_missing;
				best_xyz_missing = xyz_missing;
				best_index = j;
			}
		} // j=1,rsd_type_list.size()

		if(!best_index){
			utility_exit_with_message( "Unrecognized residue: " + pdb_name );
		}

		ResidueType const & rsd_type( *(rsd_type_list[ best_index ]) );
		TR.Debug << "Match: '" << rsd_type.name() << "'; missing " << best_xyz_missing << " coordinates" << std::endl;

		if ( best_rsd_missing ) {
			TR << "[ WARNING ] discarding " << best_rsd_missing << " atoms at position " << i <<
				" in file " << fd.filename << ". Best match rsd_type:  " << rsd_type.name() << std::endl;
		}

		// check for missing mainchain atoms:
		if ( rsd_type.is_polymer() ) {
			AtomIndices const & mainchain( rsd_type.mainchain_atoms() );
			Size const nbb( mainchain.size() );
			if ( nbb >= 3 ) {
				bool mainchain_core_present( false );
				for ( Size k=1; k<= nbb-2; ++k ) {
					if ( xyz.count( rsd_type.atom_name(mainchain[k  ])) &&
						xyz.count( rsd_type.atom_name(mainchain[k+1])) &&
						xyz.count( rsd_type.atom_name(mainchain[k+2])) ) {
						mainchain_core_present = true;
						break;
					}
				}
				if ( !mainchain_core_present ) {
					TR << "[ WARNING ] skipping pdb residue b/c its missing too many mainchain atoms: " << resid <<
						' ' << pdb_name << ' ' << rsd_type.name() << std::endl;
					for ( Size k=1; k<= nbb; ++k ) {
						if ( !xyz.count( rsd_type.atom_name(mainchain[k] ) ) ) {
							TR << "missing: " << rsd_type.atom_name( mainchain[k] ) << std::endl;
						}
					}
					if( options.exit_if_missing_heavy_atoms() == true ) {
						utility_exit_with_message("quitting due to missing heavy atoms");
					}
					continue;
				}
			}
		}

		// found a match, now fill in the coords
		ResidueOP new_rsd( ResidueFactory::create_residue( rsd_type ) );

		for ( ResidueCoords::const_iterator iter=xyz.begin(), iter_end=xyz.end(); iter!= iter_end; ++iter ) {
			if ( new_rsd->has( local_strip_whitespace(iter->first) ) ) {
				// offsetting all coordinates by a small constant prevents problems with atoms located
				// at position (0,0,0).
				// This is a bit of a dirty hack but it fixes the major problem of reading in rosetta
				// pdbs which usually start at 0,0,0. However the magnitude of this offset is so small
				// that the output pdbs should still match input pdbs. hopefully. yes. aehm.
				double offset = 1e-250; // coordinates now double, so we can use _really_ small offset.
				new_rsd->atom( local_strip_whitespace(iter->first) ).xyz( iter->second + offset );
			}
			//else runtime_assert( iter->first == " H  " && rsd_type.is_terminus() ); // special casee
		}

		// Add this new residue to the pose by appending.
		Size const old_nres( pose.total_residue() );
		if ( !old_nres ) {
			pose.append_residue_by_bond( *new_rsd );
		} else if ( ( is_lower_terminus || !check_Ntermini_for_this_chain ) ||
						is_branch_lower_terminus ||
						!new_rsd->is_polymer() ||
						!pose.residue_type(old_nres).is_polymer() ||
						!last_residue_was_recognized ) {
			pose.append_residue_by_jump( *new_rsd, 1 /*pose.total_residue()*/ );
		} else {
			if (!options.missing_dens_as_jump()) {
				pose.append_residue_by_bond( *new_rsd );
			} else {
				//fpd look for missing density in the input PDB
				//fpd if there is a discontinuity in numbering AND bondlength > 3A
				//fpd we will consider this missing density
				Residue const &last_rsd( pose.residue( old_nres ) );
				core::Real bondlength = ( last_rsd.atom( last_rsd.upper_connect_atom() ).xyz() -
																	new_rsd->atom( new_rsd->lower_connect_atom() ).xyz() ).length();

				// uncomment to look at PDB ids
				//int last_resid = utility::string2int( rinfos[pose_to_rinfo[old_nres]].resid.substr(0,4) );
				//int resid = utility::string2int( rinfo.resid.substr(0,4) );

				if ( bondlength > 3.0 ) {
					TR << "[ WARNING ] missing density found at residue (rosetta number) " << old_nres << std::endl;
					pose.append_residue_by_jump( *new_rsd, old_nres );
					core::pose::add_variant_type_to_pose_residue( pose, chemical::UPPERTERM_TRUNC, old_nres );
					core::pose::add_variant_type_to_pose_residue( pose, chemical::LOWERTERM_TRUNC, old_nres+1 );
				} else {
					pose.append_residue_by_bond( *new_rsd );
				}
			}
		}
		pose_to_rinfo.push_back( Size(i) );
		pose_resids.push_back( rinfo.resid );
		pose_temps.push_back( rinfo.temps );

		// update the pose-internal chain label if necessary
		if ( ( ( is_lower_terminus || !check_Ntermini_for_this_chain ) || is_branch_lower_terminus) && pose.total_residue() > 1 ) {
			pose.conformation().insert_chain_ending( pose.total_residue() - 1 );
		}

		last_residue_was_recognized = true;
	} // i=1,nres_pdb


	// Check termini status of newly created pose residues.
	// Will this ever happen? ~ Labonte

	Size const nres( pose.total_residue() );
	for ( Size i=1; i<= nres; ++i ) {
		ResidueInformation const & rinfo = rinfos[i];
		char chainID = rinfo.chainID;

		bool const check_Ntermini_for_this_chain = ("ALL" == chains_to_check_if_Ntermini) ?
					true : find(check_Ntermini_begin, check_Ntermini_end, chainID ) ==  check_Ntermini_end;
		bool const check_Ctermini_for_this_chain = ("ALL" == chains_to_check_if_Ctermini) ?
					true : find(check_Ctermini_begin, check_Ctermini_end, chainID ) ==  check_Ctermini_end;

		if ( !check_Ntermini_for_this_chain ) continue;
		if ( !check_Ctermini_for_this_chain ) continue;

		//Residue const & rsd( pose.residue( i ) ); // THIS WAS A BAD BUG
		if ( !pose.residue_type(i).is_polymer() ) continue;
		if ( !pose.residue_type(i).is_lower_terminus() &&
				( i == 1 ||
				!pose.residue_type( i-1 ).is_polymer() ||
				(pose.residue_type( i-1 ).is_upper_terminus() &&
						!pose.residue_type( i ).has_variant_type(BRANCH_LOWER_TERMINUS)) ) ) {
			TR << "Adding undetected lower terminus type to residue " << i << std::endl;
			core::pose::add_lower_terminus_type_to_pose_residue( pose, i );
		}
		if ( !pose.residue_type(i).is_upper_terminus() &&
				( i == nres ||
				!pose.residue_type(i+1).is_polymer() ||
				pose.residue_type(i+1).is_lower_terminus() ||
				pose.residue_type(i+1).has_variant_type(BRANCH_LOWER_TERMINUS)) ) {
			TR << "Adding undetected upper terminus type to residue " << i << std::endl;
			core::pose::add_upper_terminus_type_to_pose_residue( pose, i );
		}
	}

	//make_upper_terminus( pose, residue_set, pose.total_residue() );

	// now handle missing atoms
	//id::AtomID_Mask missing( false );
	Size num_heavy_missing = 0;

	core::pose::initialize_atomid_map( missing, pose ); // dimension the missing-atom mask
	if ( pose.total_residue() == 0 ) {

		// PDBInfo setup
		core::pose::PDBInfoOP pdb_info( new core::pose::PDBInfo( pose.total_residue() ) );
		for( Size i = 1; i <= UA_res_nums.size(); ++i ) {
			pdb_info->add_unrecognized_atom( UA_res_nums[i], UA_res_names[i], UA_atom_names[i], UA_coords[i], UA_temps[i] );
		}
		// store pdb info
		pose.pdb_info( pdb_info );
		return;

		utility_exit_with_message("ERROR: No residues in pose, empty file ? " );
		// if unchecked it segfaults further down...
	}
	for ( Size i=1; i<= pose.total_residue(); ++i ) {
		ResidueCoords const & xyz( rinfos[pose_to_rinfo[i]].xyz );

		Residue const & rsd( pose.residue(i) );
		for ( Size j=1; j<= rsd.natoms(); ++j ) {
			if ( xyz.count( rsd.atom_name(j) ) == 0 ) {
				missing[ id::AtomID( j, i ) ] = true;
				if( !rsd.atom_is_hydrogen(j) ) num_heavy_missing++;
			}
		}
	}


	//ja save the pdb residue indices in the Pose //well, PDBInfo
	//ja pdb residue indices can be negative
	utility::vector1< int > pdb_numbering;
	//sml chain char
	utility::vector1< char > pdb_chains, insertion_codes;
	//Size const nres( pose.total_residue() );
	for ( Size i(1); i <= nres; ++i ) {
		ResidueInformation const & rinfo = rinfos[pose_to_rinfo[i]];
		std::string resid( rinfo.resid.substr(0,4) );
		// pdb residue numbers can be negative
		int resid_num;
		std::istringstream ss( resid );
		ss >> resid_num;
		pdb_numbering.push_back( resid_num );

		char const chain( rinfo.resid[5] );
		pdb_chains.push_back( chain );

		char const icode( rinfo.resid[4] );
		insertion_codes.push_back( icode );
	}

	// PDBInfo setup
	core::pose::PDBInfoOP pdb_info( new core::pose::PDBInfo( pose.total_residue() ) );

	// set pdb-wide information
	pdb_info->name( fd.filename );
	if(fd.modeltag=="") {
		pdb_info->modeltag( fd.filename );
	} else {
		pdb_info->modeltag( fd.modeltag );
	}

	if( options.preserve_header() == true ) {
		pdb_info->remarks( *fd.remarks );
		pdb_info->header_information( fd.header_information()() );
	}

	// set residue level pdb information
	pdb_info->set_numbering( pdb_numbering );
	pdb_info->set_chains( pdb_chains );
	pdb_info->set_icodes( insertion_codes );
	if ( options.preserve_crystinfo() )
		pdb_info->set_crystinfo( fd.crystinfo );

	pose.conformation().fill_missing_atoms( missing );

	// most DNA structures lack 5' phosphate groups. 5' phosphates must be built to serve as part of the backbone for
	// atom/fold tree purposes. Here they are made virtual so as not to affect physical calculations.
	for ( uint seqpos(1), nres( pose.total_residue() ); seqpos <= nres; ++seqpos ) {
		Residue const & rsd( pose.residue( seqpos ) );
		if ( ! rsd.type().is_DNA() ) continue;
		for ( uint atomi(1), natoms( rsd.natoms() ); atomi <= natoms; ++atomi ) {
			id::AtomID const id( atomi, seqpos );
			if ( missing[ id ] && rsd.atom_name(atomi) == " P  " ) {
				TR << "Virtualizing missing phosphate that was built in at seqpos " << seqpos << std::endl;
				core::pose::add_variant_type_to_pose_residue( pose, "VIRTUAL_DNA_PHOSPHATE", seqpos );
				break;
			}
		}
	}

	// Look for and create any remaining non-mainchain (Edge::CHEMICAL) bonds based on a specified radius from any
	// unsatisfied residue connections.  This is used for such things as branched polymers, ubiquitination, or covalent
	// intermediates.  Note: The fold tree will remain with a jump between each such bond until import_pose::
	// set_reasonable_fold_tree() is called later, which actually adds the CHEMICAL edges to fold tree; this method
	// simply makes the bonds.
	pose.conformation().detect_bonds();

	//mjo TODO: this can try to access pose->pdb_info() which is not yet
	//initialized. Moving it after the pose->pdb_info has been
	//initialized causes integration test changes
	core::pose::initialize_disulfide_bonds(pose);

	if(pose.n_residue()>1){// 1 residue fragments for ligand design.
		pose.conformation().detect_pseudobonds();
	}

	// ensure enough space for atom level pdb information
	pdb_info->resize_atom_records( pose );

	// add unrecognized atoms to PDBInfo
	for( Size i = 1; i <= UA_res_nums.size(); ++i ) {
		pdb_info->add_unrecognized_atom( UA_res_nums[i], UA_res_names[i], UA_atom_names[i], UA_coords[i], UA_temps[i] );
	}

	// add temps to PDBInfo
	for( core::Size ir = 1; ir <= pose.total_residue(); ir++ ) {
		// fill in b-factor from pdb file
		ResidueTemps & res_temps( rinfos[pose_to_rinfo[ir]].temps );
		for( ResidueTemps::const_iterator iter=res_temps.begin(); iter != res_temps.end(); ++iter ) {
			if( pose.residue(ir).type().has( local_strip_whitespace(iter->first) ) ) {
				// printf("setting temp: res %d atom %s temp %f\n",ir,iter->first.c_str(),iter->second);
				core::Size ia = pose.residue(ir).type().atom_index(local_strip_whitespace(iter->first)) ;
				pdb_info->temperature( ir, ia, iter->second );
			} else {
				if( (iter->first)[0] == 'H' || ((iter->first)[0] == ' ' && (iter->first)[1] == 'H') ) {
					;// don't warn if H
				} else {
					TR << "[ WARNING ] can't find atom for res " << ir << " atom " << iter->first << " (trying to set temp)" << std::endl;
				}
			}
		}
	}

	// mark PDBInfo as ok and store in Pose
	pdb_info->obsolete( false );
	pose.pdb_info( pdb_info );

	//fpd fix bfactors of missing atoms using neighbors
	//fpd set hydrogen Bfactors as 1.2x attached atom
	if (options.preserve_crystinfo()) {
		core::scoring::cryst::fix_bfactorsMissing( pose );
		core::scoring::cryst::fix_bfactorsH( pose );
	}
	if ( basic::options::option[ basic::options::OptionKeys::out::file::pdb_comments]() ) {
		utility::io::izstream data(fd.filename);

		std::string line;
		while( getline( data, line ) ) {
		if( line != "##Begin comments##")
		continue;
		getline( data, line );
		while (line != "##End comments##"){
		//TR<<"Testing read comments! :"<<line<<std::endl;
		std::string const key;
		std::string const value;
		utility::vector1<std::string> comment_line(utility::string_split(line,' '));
		core::pose::add_comment(pose,comment_line[1],comment_line[2]);
		getline( data, line );
	}
	}
	}
}

///@details The input rsd_type_list are all the residue types that have
///the same 3 letter code as pdb_name. Return true if the list is
///non-empty and false otherwise.  If no residue types match, then
///either exit, ignore or remember the residue based on the following
///options in the option system:
///
/// -in:ignore_waters
/// -in:ignore_unrecognized_res
/// -in:remember_unrecognized_waters
/// -in:remember_unrecognized_res
bool is_residue_type_recognized(
	Size const pdb_residue_index,
	std::string const & pdb_name,
	core::chemical::ResidueTypeCOPs const & rsd_type_list,
	std::map< std::string, Vector > const & xyz,
	std::map< std::string, double > const & rtemp,
	utility::vector1<Size> & UA_res_nums,
	utility::vector1<std::string> & UA_res_names,
	utility::vector1<std::string> & UA_atom_names,
	utility::vector1<numeric::xyzVector<Real> > & UA_coords,
	utility::vector1<core::Real> & UA_temps){

	FileDataOptions options;
	return is_residue_type_recognized( pdb_residue_index, pdb_name, rsd_type_list, xyz, rtemp, UA_res_nums, UA_res_names, UA_atom_names, UA_coords, UA_temps, options );
}

///@details The input rsd_type_list are all the residue types that have
///the same 3 letter code as pdb_name. Return true if the list is
///non-empty and false otherwise.  If no residue types match, then
///either exit, ignore or remember the residue based on the following
///options in a FileDataOptions instance:
///
/// -ignore_waters
/// -ignore_unrecognized_res
/// -remember_unrecognized_waters
/// -remember_unrecognized_res
bool is_residue_type_recognized(
	Size const pdb_residue_index,
	std::string const & pdb_name,
	core::chemical::ResidueTypeCOPs const & rsd_type_list,
	std::map< std::string, Vector > const & xyz,
	std::map< std::string, double > const & rtemp,
	utility::vector1<Size> & UA_res_nums,
	utility::vector1<std::string> & UA_res_names,
	utility::vector1<std::string> & UA_atom_names,
	utility::vector1<numeric::xyzVector<Real> > & UA_coords,
	utility::vector1<core::Real> & UA_temps,
	FileDataOptions const & options){

	if(!rsd_type_list.empty()){
		return true;
	}

	using namespace basic::options;
	if( !(options.ignore_unrecognized_res() ||
			options.remember_unrecognized_res() ||
			(pdb_name == "HOH" && options.ignore_waters())) ) {
		// We should fail fast on unrecognized input rather than produce bad results!
		utility_exit_with_message(" unrecognized aa " + pdb_name );
	}

	if( !options.remember_unrecognized_water() ) {
		// don't bother with water
		if( pdb_name == "HOH" ){
			return false;
		}
	}

	if( options.remember_unrecognized_res() ) {
		for(std::map<std::string, Vector>::const_iterator iter=xyz.begin(), iter_end=xyz.end(); iter!= iter_end; ++iter ) {
			if( UA_res_nums.size() > 5000 ) {
				utility_exit_with_message("can't handle more than 5000 atoms worth of unknown residues\n");
			}
			TR << "remember unrecognized atom " << pdb_residue_index << " " << pdb_name << " " << local_strip_whitespace(iter->first)
			<< " temp " << rtemp.find(iter->first)->second << std::endl;
			UA_res_nums.push_back( pdb_residue_index );
			UA_res_names.push_back( pdb_name );
			UA_atom_names.push_back( local_strip_whitespace(iter->first) );
			UA_coords.push_back( iter->second );
			UA_temps.push_back( rtemp.find(iter->first)->second );
		}
	}
	return false;
}

void
pose_from_pose(
	pose::Pose & new_pose,
	pose::Pose const & old_pose,
	utility::vector1< core::Size > const & residue_indices
){
	FileDataOptions options;
	pose_from_pose( new_pose, old_pose, residue_indices, options );
}

void
pose_from_pose(
	pose::Pose & new_pose,
	pose::Pose const & old_pose,
	utility::vector1< core::Size > const & residue_indices,
	FileDataOptions const & options
){
	using namespace chemical;
	ResidueTypeSetCAP residue_set(
		ChemicalManager::get_instance()->residue_type_set( FA_STANDARD )
	);
	pose_from_pose( new_pose, old_pose, *residue_set,  residue_indices, options);
}

void
pose_from_pose(
	pose::Pose & new_pose,
	pose::Pose const & old_pose,
	chemical::ResidueTypeSet const & residue_set,
	utility::vector1< core::Size > const & residue_indices
){
	FileDataOptions options;
	pose_from_pose( new_pose, old_pose, residue_set, residue_indices, options );
}

void
pose_from_pose(
	pose::Pose & new_pose,
	pose::Pose const & old_pose,
	chemical::ResidueTypeSet const & residue_set,
	utility::vector1< core::Size > const & residue_indices,
	FileDataOptions const & options
){
	FileData fd;
	std::string data;
	fd.init_from_pose( old_pose, residue_indices );
	id::AtomID_Mask missing( false );
	build_pose_as_is1( fd, new_pose, residue_set, missing, options );
}

} // namespace pdb
} // namespace io
} // namespace core
