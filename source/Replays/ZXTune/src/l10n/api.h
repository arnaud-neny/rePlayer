/**
 *
 * @file
 *
 * @brief  Localization support API
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#ifdef NO_L10N
#  include "src/api_stub.h"
#else
#  include "src/api_real.h"
#endif
