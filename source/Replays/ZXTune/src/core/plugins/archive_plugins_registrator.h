/**
 *
 * @file
 *
 * @brief  Archive plugins registrator interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// local includes
#include "archive_plugin.h"
#include "registrator.h"

namespace ZXTune
{
  class ArchivePluginsRegistrator : public PluginsRegistrator<ArchivePlugin>
  {};
}  // namespace ZXTune
