// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/outputter/ResFileOutputter.hh
/// @brief Saves a resfile that is generated by applying the input task operations to the pose
/// @author Ken Jung

#ifndef INCLUDED_protocols_outputter_ResFileOutputter_hh
#define INCLUDED_protocols_outputter_ResFileOutputter_hh

// Unit Headers
#include <protocols/outputter/ResFileOutputter.fwd.hh>
#include <protocols/outputter/FormatStringOutputter.hh>

#include <core/pack/task/TaskFactory.fwd.hh>

namespace protocols {
namespace outputter {


#ifdef USELUA
		void lregister_ResFileOutputter( lua_State * lstate );
#endif

class ResFileOutputter : public FormatStringOutputter {

	public:
		ResFileOutputter();
		virtual ~ResFileOutputter();

		virtual void write( Pose & p );

#ifdef USELUA
		void parse_def( utility::lua::LuaObject const & def,
						utility::lua::LuaObject const & tasks );
		virtual void lregister( lua_State * lstate );
#endif

		// factory functions
		OutputterSP create();
		static std::string name() {
			return "ResFileOutputter";
		}

	private:
 	core::pack::task::TaskFactoryCOP task_factory_;
		bool designable_only_;
		std::string resfile_general_property_;


}; // end 

} // outputter
} // protocols


#endif //INCLUDED_protocols_outputter_ResFileOutputter_hh
