// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/toolbox/task_operations/RestrictIdentitiesAtAlignedPositionsOperation.cc
/// @brief
/// @author Sarelf Fleishman sarelf@uw.edu

// Unit Headers
#include <protocols/toolbox/task_operations/RestrictIdentitiesAtAlignedPositions.hh>
#include <protocols/toolbox/task_operations/RestrictIdentitiesAtAlignedPositionsCreator.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <core/pose/selection.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pose/Pose.hh>
#include <core/conformation/Conformation.hh>
#include <protocols/toolbox/task_operations/DesignAroundOperation.hh>
#include <core/pack/task/TaskFactory.hh>
// Project Headers
#include <core/pose/Pose.hh>
#include <utility/string_util.hh>

// AUTO-REMOVED #include <core/pack/task/PackerTask.hh>
// AUTO-REMOVED #include <core/pack/task/operation/TaskOperations.hh>

// Utility Headers
#include <core/types.hh>
#include <basic/Tracer.hh>
#include <utility/exit.hh>
#include <utility/vector1.hh>
#include <utility/tag/Tag.hh>
#include <core/pack/task/operation/ResLvlTaskOperations.hh>
#include <core/pack/task/operation/OperateOnCertainResidues.hh>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

// C++ Headers

using basic::Error;
using basic::Warning;
static basic::Tracer TR( "protocols.toolbox.TaskOperations.RestrictIdentitiesAtAlignedPositionsOperation" );

namespace protocols {
namespace toolbox {
namespace task_operations {

using namespace core::pack::task::operation;
using namespace std;

RestrictIdentitiesAtAlignedPositionsOperation::RestrictIdentitiesAtAlignedPositionsOperation() :
	chain_( 1 ),
	design_only_target_residues_( false )
{
	source_pose_ = new core::pose::Pose;
	res_ids_.clear();
}

RestrictIdentitiesAtAlignedPositionsOperation::~RestrictIdentitiesAtAlignedPositionsOperation() {}

core::pack::task::operation::TaskOperationOP
RestrictIdentitiesAtAlignedPositionsOperationCreator::create_task_operation() const
{
	return new RestrictIdentitiesAtAlignedPositionsOperation;
}

core::pack::task::operation::TaskOperationOP RestrictIdentitiesAtAlignedPositionsOperation::clone() const
{
	return new RestrictIdentitiesAtAlignedPositionsOperation( *this );
}

void
RestrictIdentitiesAtAlignedPositionsOperation::apply( core::pose::Pose const & pose, core::pack::task::PackerTask & task ) const
{
	using namespace protocols::rosetta_scripts;
	using namespace core::pack::task::operation;

	DesignAroundOperation dao;
	dao.design_shell( 0.01 );
	dao.repack_shell( 6.0 );
	foreach( core::Size const resid, res_ids_ ){
		core::Size const nearest_to_res = find_nearest_res( pose, *source_pose_, resid, chain() );
		if( nearest_to_res == 0 ){
			TR<<"WARNING: could not find a residue near to "<<resid<<std::endl;
			continue;
		}//fi
		RestrictAbsentCanonicalAASRLTOP racaas = new RestrictAbsentCanonicalAASRLT;
		char const residue_id( source_pose_->residue( resid ).name1() );
		std::string residues_to_keep("");
		residues_to_keep += residue_id;
		racaas->aas_to_keep( residues_to_keep );
		OperateOnCertainResidues oocr;
		oocr.op( racaas );
		utility::vector1< core::Size > temp_vec;
		temp_vec.clear();
		temp_vec.push_back( nearest_to_res );
		oocr.residue_indices( temp_vec );
		oocr.apply( pose, task );
		dao.include_residue( nearest_to_res );
	}//foreach resid
	if( design_only_target_residues() )
		dao.apply( pose, task );
}

void
RestrictIdentitiesAtAlignedPositionsOperation::source_pose( std::string const s ) {
	core::import_pose::pose_from_pdb( *source_pose_, s );
}

void
RestrictIdentitiesAtAlignedPositionsOperation::parse_tag( TagPtr tag )
{
	using namespace protocols::rosetta_scripts;
	utility::vector1< std::string > pdb_names, start_res, stop_res;
  source_pose( tag->getOption< std::string >( "source_pdb" ) );
	std::string const res_list( tag->getOption< std::string >( "resnums" ) );
	utility::vector1< std::string > const split_reslist( utility::string_split( res_list,',' ) );
	chain( tag->getOption< core::Size >( "chain", 1 ) );
	TR<<"source_pdb: "<<tag->getOption< std::string >( "source_pdb" )<<" restricting residues: ";
	foreach( std::string const res_str, split_reslist ){
		res_ids_.push_back( core::pose::parse_resnum( res_str, *source_pose_ ) );
		TR<<res_str<<",";
	}
	design_only_target_residues( tag->getOption< bool >( "design_only_target_residues", false ) );
	TR<<std::endl;
}

} //namespace protocols
} //namespace toolbox
} //namespace task_operations
