// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/antibody2/AntibodyInfo.fwd.hh
/// @brief  AntibodyInfo class forward declarations header
/// @author Jianqing Xu (xubest@gmail.com)


#ifndef INCLUDED_protocols_antibody2_AntibodyInfo_fwd_hh
#define INCLUDED_protocols_antibody2_AntibodyInfo_fwd_hh


// Utility headers
#include <utility/pointer/owning_ptr.hh>


// C++ Headers
namespace protocols{
namespace antibody2{

// Forward
class AntibodyInfo;

typedef utility::pointer::owning_ptr< AntibodyInfo > AntibodyInfoOP;
typedef utility::pointer::owning_ptr< AntibodyInfo const > AntibodyInfoCOP;



} //namespace antibody2
} //namespace protocols

#endif //INCLUDED_protocols_AntibodyInfo_FWD_HH
