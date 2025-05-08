// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "project.hpp"

/// Interprets argument as a string litteral.
/// The indirection in the call to # lets the macro expansion on the argument be done first.
#define DIVERSALIS__STRINGIZE(tokens) DIVERSALIS__STRINGIZE__DETAIL__NO_EXPANSION(tokens)

///\internal
/// Don't call this macro directly ; call DIVERSALIS__STRINGIZE, which calls this macro after macro expansion is done on the argument.
///\relates DIVERSALIS__STRINGIZE
#define DIVERSALIS__STRINGIZE__DETAIL__NO_EXPANSION(tokens) #tokens
