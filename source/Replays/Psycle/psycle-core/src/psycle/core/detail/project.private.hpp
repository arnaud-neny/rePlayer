// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net

///\file
///\brief private project-wide definitions. file included first by every translation unit (.cpp).
#pragma once
#include "config.private.hpp"

#define PSYCLE__CORE__SOURCE // makes PSYCLE__CORE__DECL dll-exports the symbols when PSYCLE__CORE__SHARED is defined

#define UNIVERSALIS__META__MODULE__NAME "psycle-core" // used in universalis::compiler::location
//#define UNIVERSALIS__META__MODULE__VERSION 0 // optionally used in universalis::compiler::location

#include "project.hpp"
