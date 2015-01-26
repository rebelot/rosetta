// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    mpfolding.cc
/// @details last Modified: 4/4/14
/// @author  JKLeman (julia.koehler1982@gmail.com)

// App headers
#include <devel/init.hh>

// Project Headers
#include <protocols/moves/Mover.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>


// Package Headers
#include <apps/benchmark/performance/init_util.hh>
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/conformation/Conformation.hh>
#include <core/conformation/membrane/MembraneInfo.hh>

#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/Job.hh>
#include <basic/Tracer.hh>

#include <protocols/viewer/viewers.hh>

// C++ Headers
#include <cstdlib>
#include <string>
#include <cmath>


///////////////////////////////////////////////////////////////////////////////
// HEADERS FROM PROTOCOL

// Unit Headers
#include <protocols/moves/Mover.hh>

// Project Headers
#include <protocols/membrane/AddMembraneMover.hh>
#include <protocols/membrane/TranslationRotationMover.hh>
#include <protocols/docking/DockMCMProtocol.hh>
#include <protocols/docking/DockingProtocol.hh>
#include <protocols/moves/MoverContainer.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoreFunction.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/PyMolMover.hh>
#include <core/conformation/membrane/Span.hh>
#include <core/conformation/membrane/SpanningTopology.hh>
#include <protocols/membrane/geometry/EmbeddingDef.hh>
#include <protocols/membrane/geometry/Embedding.hh>

// Package Headers
#include <core/kinematics/FoldTree.hh>
#include <core/pose/Pose.hh>
#include <core/conformation/membrane/util.hh>
#include <protocols/membrane/geometry/util.hh>
#include <core/sequence/Sequence.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/sequence/util.hh>
#include <core/types.hh>

// Utility Headers
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/membrane_new.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

// C++ Headers
#include <cstdlib>

using basic::Error;
using basic::Warning;

using namespace core;
using namespace core::pose;
using namespace core::conformation;
using namespace core::conformation::membrane;

static thread_local basic::Tracer TR( "apps.pilot.jkleman.MPFolding" );

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// HEADER ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class MPFoldingMover : public protocols::moves::Mover {
	
public:
	
	/////////////////////
	/// Constructors  ///
	/////////////////////

	/// @brief Default Constructor
	MPFoldingMover();
	
	/// @brief Copy Constructor
	MPFoldingMover( MPFoldingMover const & src );
	
	/// @brief Destructor
	virtual ~MPFoldingMover();
	
public: // methods
	
	/// @brief Create a Clone of this mover
	virtual protocols::moves::MoverOP clone() const;
	
	/// @brief Create a Fresh Instance of this Mover
	virtual protocols::moves::MoverOP fresh_instance() const;
	
	/////////////////////
	/// Mover Methods ///
	/////////////////////

	/// @brief Get the name of this Mover (MPFoldingMover)
	virtual std::string get_name() const;
	
	/// @brief Fold MP
	virtual void apply( Pose & pose );
	
private: // methods
	
	// setup the foldtree with cutpoints according to topology
//	void setup_foldtree();
	
	
	
private: // data
	
	// topology object storing SSEs
	SpanningTopologyOP SSE_topo_;
	
	// topology object storing loops
//	SpanningTopologyOP loops_;
	
	// foldtree
	FoldTreeOP foldtree_;
	
	// add membrane mover
//	protocols::membrane::AddMembraneMoverOP add_membrane_mover_;
	
	// docking protocols protocol
//	protocols::docking::DockingProtocolOP docking_protocol_;
	
	// sequence mover
//	protocols::moves::RandomMoverOP random_mover_;
	
	// scorefunction
	core::scoring::ScoreFunctionOP lowres_scorefxn_;
	core::scoring::ScoreFunctionOP highres_scorefxn_;
	
	// kT for MCM protocol
	// KAB - below line commented out by warnings removal script (-Wunused-private-field) on 2014-09-11
	// Real kT_;
	
};

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IMPLEMENTATION/////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


/////////////////////
/// Constructors  ///
/////////////////////

/// @brief Default Constructor
/// @details Create a membrane pose setting the membrane center
/// at center=(0, 0, 0), normal=(0, 0, 1) and loads in spans
/// and lips from the command line interface.
MPFoldingMover::MPFoldingMover() :
protocols::moves::Mover()
{}

/// @brief Copy Constructor
/// @details Create a deep copy of this mover
MPFoldingMover::MPFoldingMover( MPFoldingMover const & src ) :
protocols::moves::Mover( src )
{}

/// @brief Destructor
MPFoldingMover::~MPFoldingMover() {}

/// @brief Create a Clone of this mover
protocols::moves::MoverOP
MPFoldingMover::clone() const {
	return ( protocols::moves::MoverOP( new MPFoldingMover( *this ) ) );
}

/// @brief Create a Fresh Instance of this Mover
protocols::moves::MoverOP
MPFoldingMover::fresh_instance() const {
	return protocols::moves::MoverOP( new MPFoldingMover() );
}

/////////////////////
/// Mover Methods ///
/////////////////////

/// @brief Get the name of this Mover (MPFoldingMover)
std::string
MPFoldingMover::get_name() const {
	return "MPFoldingMover";
}


/// @brief Add Membrane Components to Pose
void
MPFoldingMover::apply( Pose & pose ) {
	
	using namespace basic;
	using namespace basic::options;
	using namespace core;
	using namespace core::pose;
	using namespace core::sequence;
	using namespace core::conformation::membrane;
	using namespace protocols::membrane::geometry;
	using namespace protocols::membrane;

	// register options
	option.add_relevant( OptionKeys::in::file::fasta );

	// check if fasta there
	if ( ! option[OptionKeys::in::file::fasta].user() ){
		utility_exit_with_message("Please provide a fasta file!");
	}

	// read fasta
	SequenceOP sequence = read_fasta_file(
					option[ OptionKeys::in::file::fasta ]()[1] )[1];

	// get number of residues
	std::string seq = sequence->to_string();
	//Size nres = sequence->length();

	TR << "" << std::endl;

	// create pose from sequence
	make_pose_from_sequence( pose, "MNGTEGPNFYVPFSNKTGVVRSPFEAPQYYLAEPWQFSMLAAYMFLLIMLGFPINFLTLYVTVQHKKLRTPLNYILLNLAVADLFMVFGGFTTTLYTSLHGYFVFGPTGCNLEGFFATLGGEIALWSLVVLAIERYVVVCKPMSNFRFGENHAIMGVAFTWVMALACAAPPLVGWSRYIPEGMQCSCGIDYYTPHEETNNESFVIYMFVVHFIIPLIVIFFCYGQLVFTVKEAAAQQQESATTQKAEKEVTRMVIIMVIAFLICWLPYAGVAFYIFTHQGSDFGPIFMTIPAFFAKTSAVYNPVIYIMMNKQFRNCMVTTLCCGKNPLGDDEASTTVSKTETSQVAPA", "centroid" );
	
	// create topology from spanfile
	SpanningTopologyOP topo( new SpanningTopology( spanfile_name() ) );

	pose.dump_pdb("1_ideal_helices.pdb");

	TR << "1" << std::endl;
	
	// create topology object holding loops = 'inverse' of spans
	SpanningTopologyOP loops = new SpanningTopology();
	Size prev_end = 1;
	for ( Size i = 1; i <= topo->nspans(); ++i ){
		loops->add_span( prev_end, topo->span(i)->start() - 1 );
		prev_end = topo->span(i)->end() + 1;
	}
	loops->add_span( prev_end, pose.total_residue() );

	TR << "2" << std::endl;

	// create ideal helices from SSEs
	for ( Size i = 1; i <= pose.total_residue(); ++i ){
		if ( topo->in_span( i ) ){
			pose.set_phi(   i, -62 );
			pose.set_psi(   i, -41 );
			pose.set_omega( i, 180 );
		}
	}
	pose.dump_pdb("2_ideal_helices.pdb");

	TR << "3" << std::endl;
	
	// default center
	EmbeddingDefOP membrane = new EmbeddingDef();

	TR << "4" << std::endl;
	
	// define center and normal
	Vector center(0, 0, 0);
	Vector normal(0, 0, 1);
	
	// add MEM to root of foldtree
	AddMembraneMoverOP add_mem = new AddMembraneMover( center, normal, topo );
	add_mem->apply( pose );

	TR << "5" << std::endl;
	
	// foldtree stuff, get membrane jump, etc
	Size memjump = pose.conformation().membrane_info()->membrane_jump();
	Size memrsd = pose.conformation().membrane_info()->membrane_rsd_num();

	TR << "6" << std::endl;
	
	// more foldtree
	core::kinematics::FoldTree foldtree = pose.fold_tree();

	TR << "7" << std::endl;
	
	// get jump anchor residue in first SSE
	Size anchor1 = topo->span(1)->center();

	TR << "8" << std::endl;

	// slide: jumpnumber, newres1, newres2
	foldtree.slide_jump( 1, memrsd, anchor1 );

	TR << "9" << std::endl;
	
	// add jumps to residues at centers of SSEs
	// these are not the COMs, these are only defined from the topo object
	for ( Size i = 2; i <= topo->nspans(); ++i ){
		Size anchor = topo->span(i)->center();
		Size cut = loops->span(i)->center();
		foldtree.new_jump( memrsd, anchor, cut );
	}

	TR << "10" << std::endl;
	
	// set foldtree
	pose.fold_tree( foldtree );
	
	// reorder foldtree to set MEM to root
	reorder_membrane_foldtree( pose );
	pose.fold_tree().show( std::cout );
	
	TR << "11" << std::endl;

	PoseOP mypose = new Pose( pose );

	// create embedding object from topology and pose
	// this describes how the normals currently are
	EmbeddingOP embedding_old = new Embedding( topo, mypose );

	TR << "12" << std::endl;
	TR << "embedding_old: " << std::endl;
	embedding_old->show();
	
	// move apart all SSEs into a big circle
	// create embedding object from topology that contains parallel/antiparallel
	// orientations of normals around a circle of radius 200
	// this object describes how the normals should be
	Real radius( 50 );
	EmbeddingOP embedding_new = new Embedding( topo, radius );
	TR << "embedding_new: " << std::endl;
	embedding_new->show();

	TR << "13" << std::endl;
	
	// move the SSEs out to their desired position using both embedding objects
	for ( Size i = 1; i <= topo->nspans(); ++i ){
		Vector old_center = embedding_old->embedding(i)->center();
		Vector old_normal = embedding_old->embedding(i)->normal();
		Vector new_center = embedding_new->embedding(i)->center();
		Vector new_normal = embedding_new->embedding(i)->normal();
		Size jumpnum = i;
		
		Vector trans_vec = new_center - old_center;
		
		TR << "old_center: " << old_center.to_string() << std::endl;
		TR << "old_normal: " << old_normal.to_string() << std::endl;
		TR << "new_center: " << new_center.to_string() << std::endl;
		TR << "new_normal: " << new_normal.to_string() << std::endl;
		TR << "jumpnum: " << jumpnum << std::endl;

//		TranslationMoverOP trans = new TranslationMover( trans_vec, jumpnum );
//		trans->apply( pose );
//
//		RotationMoverOP rot = new RotationMover( old_normal, new_normal, new_center, jumpnum );
//		rot->apply( pose );

				
		TranslationRotationMoverOP transrot = new TranslationRotationMover(
		old_center, old_normal, new_center, new_normal, jumpnum );
		transrot->apply( pose );
	}

	TR << "14" << std::endl;

	pose.dump_pdb("14.pdb");

	// get center SSE number to start docking from
	Size sse_number;
	if ( topo->nspans() % 2 == 0 ){
		sse_number = topo->nspans() / 2;
	}
	else{
		sse_number = ( topo->nspans() + 1 ) / 2;
	}

	TR << "15" << std::endl;
	
	// start in middle: if odd, start in middle, go up, down, up, etc
	// if even, start below middle, go up, down, up, etc
	Size sse_raw = 1;
	Size sse_signed = 1;
	Size sign = 1;
//	while ( sse_number <= topo->nspans() ){
//		
//		Size jumpnum = sse
//	
//		// move first SSE into center
//		if ( sse == 1 ){
//			TranslationRotationMoverOP rt1 = new TranslationRotationMover(
//			   old_center, old_normal, new_center, new_normal, jumpnum );
//			rt1->apply( pose );
//			
//		}
//		
//		// slide together next one
//		
//		
//		
//		// dock these two
//		// dock each SSE starting from center of sequence
//		// filter via loop length constraint
//		// consider other regular constraints
//		// switch fragments and minimize
//		
//		
//
//		
//		
//		// counter and sign for getting the correct span
//		sse_raw += 1;
//		sign    *= ( -1 );
//		Size addition = sign * sse_raw;
//		Size sse_signed =
//	}
	
	































	
}

////////////////////////////////////////////////////////////////////////////////

// read spanfiles
std::string read_spanfile(){
	
	using namespace basic;
	using namespace basic::options;

	// cry if spanfiles not given
	if ( ! option[OptionKeys::membrane_new::setup::spanfiles].user() ){
		utility_exit_with_message("Please provide a single spanfiles!");
	}
	
	// get filenames from Optionsystem
	std::string spanfile = option[OptionKeys::membrane_new::setup::spanfiles]()[1];
	
	return spanfile;
}// read spanfile


////////////////////////////////////////////////////////////////////////////////

typedef utility::pointer::shared_ptr< MPFoldingMover > MPFoldingMoverOP;

/////////////////////////////////////// MAIN ///////////////////////////////////

int
main( int argc, char * argv [] )
{
	try {
//		using namespace protocols::docking::membrane;
		using namespace protocols::jd2;
		
		// initialize option system, random number generators, and all factory-registrators
		devel::init(argc, argv);
		//protocols::init(argc, argv);
		
		MPFoldingMoverOP mpfm( new MPFoldingMover() );
		JobDistributor::get_instance()->go(mpfm);
	}
	catch ( utility::excn::EXCN_Base const & e ) {
		std::cout << "caught exception " << e.msg() << std::endl;
		return -1;
	}
}
