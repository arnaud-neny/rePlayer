--[[ 
psycle examples pluginloaddemo (c) 2014 by psycledelics
File:  plugloademo.lua
Desccription: demo that loads and uses a vst plugin inside
              main code in /plugload/machine.lua
copyright 2014 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

function psycle.info()
  local machinemodes = require("psycle.machinemodes")
  return {
    vendor="psycle examples",
    name="pluginloaddemo",
    generator = 0,
    version=0,
	  api=0
  }  
end

function psycle.start()
  psycle.setmachine(require("plugloaddemo.machine"))
end
