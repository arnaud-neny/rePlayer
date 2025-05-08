--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  machinetypes.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

return {
 UNDEFINED = -1,
 MASTER = 0,
 SINE = 1, -- now a plugin
 DIST = 2, -- now a plugin
 SAMPLER = 3,
 DELAY = 4, -- now a plugin
 MACH2PFILTER = 5, -- now a plugin
 GAIN = 6, -- now a plugin
 FLANGER = 7, -- now a plugin
 PLUGIN = 8,
 VST = 9,
 VSTFX = 10,
 SCOPE = 11, -- Test machine. removed
 XMSAMPLER = 12,
 DUPLICATOR = 13,
 MIXER = 14,
 RECORDER = 15, 
 DUPLICATOR2 = 16,
 LUA = 17,
 LADSPA = 18,
 DUMMY = 255
}
