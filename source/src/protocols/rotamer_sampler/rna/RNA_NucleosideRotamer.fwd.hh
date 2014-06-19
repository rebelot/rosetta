// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/rotamer_sampler/rna/RNA_NucleosideRotamer.fwd.hh
/// @brief Generate rotamers for one RNA nucleoside (pucker + glycosidic chi).
/// @author Fang-Chieh Chou

#ifndef INCLUDED_protocols_rotamer_sampler_rna_RNA_NucleosideRotamer_fwd_HH
#define INCLUDED_protocols_rotamer_sampler_rna_RNA_NucleosideRotamer_fwd_HH


#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace rotamer_sampler {
namespace rna {

class RNA_NucleosideRotamer;
typedef utility::pointer::owning_ptr< RNA_NucleosideRotamer > RNA_NucleosideRotamerOP;

} //rna
} //rotamer_sampler
} //protocols

#endif
