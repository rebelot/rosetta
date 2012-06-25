// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   src/basic/database/sql_utils.hh
/// @author Sam DeLuca

#ifndef INCLUDED_basic_database_sql_utils_HH
#define INCLUDED_basic_database_sql_utils_HH

#include <utility/sql_database/DatabaseSessionManager.hh>

#include <platform/types.hh>
#include <utility/down_cast.hh>
#include <utility/file/FileName.fwd.hh>
#include <utility/file/FileName.hh>
#include <utility/file/PathName.fwd.hh>
#include <utility/file/PathName.hh>
#include <utility/pointer/ReferenceCount.fwd.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/pointer/owning_ptr.functions.hh>
#include <utility/pointer/owning_ptr.fwd.hh>
#include <utility/pointer/owning_ptr.hh>
#include <utility/sql_database/DatabaseSessionManager.fwd.hh>
#include <utility/vector1.hh>
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>
#include <boost/scoped_ptr.hpp>
#include <cppdb/frontend.h>
#include <boost/uuid/uuid.hpp>

namespace basic {
namespace database {

///@brief Aquire a database session using the command line options
utility::sql_database::sessionOP
get_db_session();

///@brief Aquire a database session using the command line parameters
/// For postgres databases, the pq_schema acts like a namespace in the
/// database
utility::sql_database::sessionOP
get_db_session(
	std::string const & db_name,
	std::string const & pq_schema="");

///@brief Aquire a database session using the command line parameters
/// For postgres databases, the pq_schema acts like a namespace in the
/// database
utility::sql_database::sessionOP
get_db_session(
	utility::sql_database::DatabaseMode::e db_mode,
	std::string const & db_name,
	std::string const & pq_schema="");

cppdb::statement
safely_prepare_statement(
	std::string const & statement_string,
	utility::sql_database::sessionOP db_session);

void
safely_write_to_database(
	cppdb::statement & statement);

cppdb::result
safely_read_from_database(
	cppdb::statement & statement);

void
insert_or_ignore(
	std::string table_name,
	std::vector<std::string> column_names,
	std::vector<std::string> values,
	utility::sql_database::sessionOP db_session);

void
check_statement_sanity(std::string sql);

bool
table_exists(
	utility::sql_database::sessionOP db_session,
	std::string const & table_name);

///@brief set the number of 1kb pages to use for cache
void
set_cache_size(
	utility::sql_database::sessionOP db_session,
	platform::Size cache_size);

utility::vector1<boost::uuids::uuid>
struct_ids_from_tag(
	utility::sql_database::sessionOP db_session,
	std::string const & tag);

void write_schema_to_database(
	std::string schema,
	utility::sql_database::sessionOP db_session);


}
}

#endif /* INCLUDED_basic_database_sql_utils_HH */
