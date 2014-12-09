// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

// This file has been generated by Py++.

#include "boost/python.hpp"

#include <basic/options/option.hh>

#include <utility/keys/KeyLookup.hh>
#include <utility/options/keys/BooleanOptionKey.hh>

#include <string>

//#define PYROSETTA
#include <utility/exit.hh>

#include <basic/options/keys/in.OptionKeys.gen.hh>

#include <platform/types.hh>

#include <iostream>


// Includes for dummy bindings to simplify import orders
// #include <basic/datacache/WriteableCacheableDataFactory.hh>
// #include <basic/resource_manager/FallbackConfigurationFactory.hh>
// #include <basic/resource_manager/ResourceManager.hh>
// #include <basic/resource_manager/ResourceLoaderFactory.hh>
// #include <basic/resource_manager/ResourceOptionsFactory.hh>
// #include <basic/resource_manager/ResourceLocatorFactory.hh>
// #include <basic/resource_manager/ResourceManagerFactory.hh>


namespace bp = boost::python;

//using namespace core::io::pdb;



template< typename T, typename K >
T get_option(std::string const & id)
{
	if( !utility::options::OptionKeys::has( id ) ) utility_exit_with_message( "get_option: OptionKey with id " + id + " not found!" );

	return basic::options::option[ dynamic_cast<K const &>( utility::options::OptionKeys::key( id ) ) ].value();
}

template< typename T, typename K >
void set_option(std::string const & id, T const & value)
{
	if( !utility::options::OptionKeys::has( id ) ) 	utility_exit_with_message( "set_option: OptionKey with id " + id + " not found!" );

	basic::options::option[ dynamic_cast<K const &>( utility::options::OptionKeys::key( id ) ) ].value( value );
}


// Special case: FileVectorOption (we want to convert it to string vector on the fly)
utility::vector1<std::string> get_file_vector_option(std::string const & id)
{
	//if( !utility::options::OptionKeys::has( id ) )
	//	throw "get_option: OptionKey with id " + id + " not found!";
	if( !utility::options::OptionKeys::has( id ) ) 	utility_exit_with_message( "get_option: OptionKey with id " + id + " not found!" );

	return basic::options::option[ dynamic_cast<utility::options::FileVectorOptionKey const &>( utility::options::OptionKeys::key( id ) ) ].value();
}


void __basic_by_hand_beginning__()
{
	bp::def("get_boolean_option", &get_option< bool,        utility::options::BooleanOptionKey> );
	bp::def("get_integer_option", &get_option< int,         utility::options::IntegerOptionKey> );
	bp::def("get_real_option",    &get_option< platform::Real,  utility::options::RealOptionKey> );
	bp::def("get_string_option",  &get_option< std::string, utility::options::StringOptionKey> );
	bp::def("get_file_option",    &get_option< std::string, utility::options::FileOptionKey> );

	bp::def("get_boolean_vector_option", &get_option< utility::vector1<bool>,        utility::options::BooleanVectorOptionKey> );
	bp::def("get_integer_vector_option", &get_option< utility::vector1<int>,         utility::options::IntegerVectorOptionKey> );
	bp::def("get_real_option",           &get_option< utility::vector1<platform::Real>,  utility::options::RealVectorOptionKey> );
	bp::def("get_string_vector_option",  &get_option< utility::vector1<std::string>, utility::options::StringVectorOptionKey> );
	bp::def("get_file_vector_option",    &get_option< utility::vector1<std::string>, utility::options::FileVectorOptionKey> );

	bp::def("set_boolean_option", &set_option< bool,        utility::options::BooleanOptionKey> );
	bp::def("set_integer_option", &set_option< int,         utility::options::IntegerOptionKey> );
	bp::def("set_real_option",    &set_option< platform::Real,  utility::options::RealOptionKey> );
	bp::def("set_string_option",  &set_option< std::string, utility::options::StringOptionKey> );
	bp::def("set_file_option",    &set_option< std::string, utility::options::FileOptionKey> );

	bp::def("set_boolean_vector_option", &set_option< utility::vector1<bool>,        utility::options::BooleanVectorOptionKey> );
	bp::def("set_integer_vector_option", &set_option< utility::vector1<int>,         utility::options::IntegerVectorOptionKey> );
	bp::def("set_real_option",           &set_option< utility::vector1<platform::Real>,  utility::options::RealVectorOptionKey> );
	bp::def("set_string_vector_option",  &set_option< utility::vector1<std::string>, utility::options::StringVectorOptionKey> );
	bp::def("set_file_vector_option",    &set_option< utility::vector1<std::string>, utility::options::FileVectorOptionKey> );

	// Dummy imports to simplify monolith build imports logic
	//typedef boost::python::class_< ::basic::datacache::DataCache<basic::datacache::CacheableData> > DataCache_T_basic_datacache_CacheableData_T_exposer_type;
    //DataCache_T_basic_datacache_CacheableData_T_exposer_type DataCache_T_basic_datacache_CacheableData_T_exposer("__DataCache_T_basic_datacache_CacheableData_T", "Indexed storage for objects derived from a ReferenceCountable\n data type.\nIntended for use as a generic data cache by storing objects\n derived from a ReferenceCountable data type in a unique slot designated\n by an integer id (enum, size index, etc.). The DataCache will only store\n one object per slot/id.  For example, see the PoseDataCache used in\n core::pose::Pose, which is indexed by the enum basic::pose::datacache:CacheableDataType.\n Currently when data is set(), it is not cloned -- classes deriving from\n DataCache should remember to overload set() if they need cloning behavior.\n@tparam Data Class derived from utility::pointer::ReferenceCount that\n defines a virtual clone() method.\n", boost::python::init <  >() );

	// Dummy bindings to simplify import orders
	// boost::python::class_< utility::SingletonBase<basic::datacache::WriteableCacheableDataFactory>, boost::noncopyable >( "__utility_SingletonBase_basic_datacache_WriteableCacheableDataFactory__");
	// boost::python::class_< utility::SingletonBase<basic::resource_manager::FallbackConfigurationFactory>, boost::noncopyable >( "__utility_SingletonBase_basic_resource_manager_FallbackConfigurationFactory__");
	// boost::python::class_< utility::SingletonBase<basic::resource_manager::ResourceManager>, boost::noncopyable >( "__utility_SingletonBase_basic_resource_manager_ResourceManager__");
	// boost::python::class_< utility::SingletonBase<basic::resource_manager::ResourceLoaderFactory>, boost::noncopyable >( "__utility_SingletonBase_basic_resource_manager_ResourceLoaderFactory__");
	// boost::python::class_< utility::SingletonBase<basic::resource_manager::ResourceOptionsFactory>, boost::noncopyable >( "__utility_SingletonBase_basic_resource_manager_ResourceOptionsFactory__");
	// boost::python::class_< utility::SingletonBase<basic::resource_manager::ResourceLocatorFactory>, boost::noncopyable >( "__utility_SingletonBase_basic_resource_manager_ResourceLocatorFactory__");
	// boost::python::class_< utility::SingletonBase<basic::resource_manager::ResourceManagerFactory>, boost::noncopyable >( "__utility_SingletonBase_basic_resource_manager_ResourceManagerFactory__");

}
