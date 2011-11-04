// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   basic/options/keys/corrections.OptionKeys.gen.hh
/// @brief  basic::options::OptionKeys collection
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)
/// @author James M. Thompson (tex@u.washington.edu)

#ifndef INCLUDED_basic_options_keys_corrections_OptionKeys_gen_HH
#define INCLUDED_basic_options_keys_corrections_OptionKeys_gen_HH

// Unit headers
#include <basic/options/keys/OptionKeys.hh>

namespace basic {
namespace options {
namespace OptionKeys {

namespace corrections { extern BooleanOptionKey const corrections; }
namespace corrections { extern BooleanOptionKey const correct; }
namespace corrections { namespace score { extern BooleanOptionKey const score; } }
namespace corrections { namespace score { extern BooleanOptionKey const no_his_his_pairE; } }
namespace corrections { namespace score { extern BooleanOptionKey const hbond_His_Phil_fix; } }
namespace corrections { namespace score { extern BooleanOptionKey const helix_hb_06_2009; } }
namespace corrections { namespace score { extern BooleanOptionKey const use_incorrect_hbond_deriv; } }
namespace corrections { namespace score { extern StringOptionKey const p_aa_pp; } }
namespace corrections { namespace score { extern BooleanOptionKey const p_aa_pp_nogridshift; } }
namespace corrections { namespace score { extern BooleanOptionKey const rama_not_squared; } }
namespace corrections { namespace score { extern FileOptionKey const rama_map; } }
namespace corrections { namespace score { extern BooleanOptionKey const dun10; } }
namespace corrections { namespace score { extern StringOptionKey const dun10_dir; } }
namespace corrections { namespace score { extern BooleanOptionKey const dun08; } }
namespace corrections { namespace score { extern StringOptionKey const dun08_dir; } }
namespace corrections { namespace score { extern StringOptionKey const dun02_file; } }
namespace corrections { namespace score { extern StringOptionKey const ch_o_bond_potential; } }
namespace corrections { namespace score { extern BooleanOptionKey const hack_elec_co_only; } }
namespace corrections { namespace score { extern RealOptionKey const lj_hbond_hdis; } }
namespace corrections { namespace score { extern FileOptionKey const PB_potential_file; } }
namespace corrections { namespace score { extern BooleanOptionKey const PB_sidechain_only; } }
namespace corrections { namespace score { extern IntegerVectorOptionKey const PB_score_residue_range; } }
namespace corrections { namespace score { extern IntegerVectorOptionKey const PB_revamp_near_chain; } }
namespace corrections { namespace score { extern RealOptionKey const PB_potential_cap; } }
namespace corrections { namespace score { extern RealOptionKey const lj_hbond_OH_donor_dis; } }
namespace corrections { namespace score { extern BooleanOptionKey const score12prime; } }
namespace corrections { namespace score { extern BooleanOptionKey const hb_sp2_chipen; } }
namespace corrections { namespace score { extern RealOptionKey const hb_sp2_amp; } }
namespace corrections { namespace score { extern RealOptionKey const hb_sp2_peak_heigh_above_trough; } }
namespace corrections { namespace chemical { extern BooleanOptionKey const chemical; } }
namespace corrections { namespace chemical { extern BooleanOptionKey const icoor_05_2009; } }
namespace corrections { namespace chemical { extern BooleanOptionKey const parse_charge; } }

} // namespace OptionKeys
} // namespace options
} // namespace basic

#endif
