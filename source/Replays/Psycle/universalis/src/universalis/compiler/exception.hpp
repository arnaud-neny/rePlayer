// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.pastnotecut.org : johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>
#include <string>

namespace universalis { namespace compiler { namespace exceptions {

/// provides information about the exception in an ellipsis catch(...) clause.
/// Not all compilers makes it possible to obtain information.
/// The GNU compiler and Borland's one do.
std::string UNIVERSALIS__DECL ellipsis_desc();

}}}
