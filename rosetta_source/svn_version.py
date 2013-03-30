# This function expects that the current working directory is the Mini root directory.
# If that's ever not true, we need to modify this to take an optional dir name on the cmd line.
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

def svn_version():
    '''
    Generates a C++ header file with a summary of the current version(s) of the working copy, if any.
    If this code is not a Subversion checkout, the version will be given as "exported".
    If the program "svnversion" is not available, the version will be given as "unknown".
    Although this is being placed in core/, it doesn't really belong to any subproject.
    There's no good way to know when the version summary will change, either, so we just generate the file every time.
    '''
    import os, re
    # If svnversion is not found, returns "" -> "unknown"
    # These commands work correctly because our current working directory is the Mini root.
    ver = os.popen("svnversion .").read().strip() or "unknown"
    url = "unknown"

    #if we aren't in an svn repository, try git
    if ver == "unknown" or ver == "exported":
        ver = os.popen("git log -1 --format='%H'").read().strip() or "unknown"
        if ver != "unknown":
            url = os.popen("git remote -v |grep fetch |awk '{print $2}'|head -n1").read().strip()
            if url == "":
                url = "unknown"
    else:
        svn_info = os.popen("svn info").read()
        match = re.search('URL: (.+)', svn_info)
        if match: url = match.group(1)
        else: url = "unknown"
    # normpath() converts foward slashes to backslashes on Windows
    f = open( os.path.normpath("src/devel/svn_version.cc"), "w" )
    f.write('''// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   devel/svn_version.cc
///
/// @brief
/// @author Ian W. Davis
/// @author Andrew Leaver-Fay

/*****************************************************************************
*   This file is automatically generated by the build system.                *
*   DO NOT try modifying it by hand -- your changes will be lost.            *
*   DO NOT commit it to the Subversion repository.                           *
*****************************************************************************/

#include <core/svn_version.hh>

namespace devel {

std::string rosetta_svn_version() { return "%(ver)s"; }
std::string rosetta_svn_url() { return "%(url)s"; }

class VersionRegistrator
{
public:
	VersionRegistrator() {
		core::set_svn_version_and_url( rosetta_svn_version(), rosetta_svn_url() );
	}
};

// There should only ever be one instance of this class
// so that core::set_svn_version_and_url is called only once
VersionRegistrator vr;

void
register_version_with_core() {
	// oh -- there's nothing in this function.  But
	// forcing devel::init to call this function ensures
	// that the vr variable in this file gets instantiated
}

} // namespace devel
''' % vars())
    f.close()
def main():
    # Run with timing
    import sys, time
    starttime = time.time()
    sys.stdout.write("Running versioning script ... ")
    sys.stdout.flush() # Make sure it gets dumped before running the function.
    svn_version()
    sys.stdout.write("Done. (%.1f seconds)\n" % (time.time() - starttime) )

if __name__ == "__main__" or __name__ == "__builtin__": main()
