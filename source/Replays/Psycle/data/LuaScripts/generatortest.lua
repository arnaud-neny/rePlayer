--[[ 
psycle luaris generatortest (c) 2014 by psycledelics
File:  generatortest.lua
copyright 2014 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

function psycle.info()
  local machinemodes = require("psycle.machinemodes")
  return  {
      vendor="psycle",
      name="luaris",
      generator = 1, --machine.GENERATOR,
      mode = machinemodes.GENERATOR,
      version=0,
      api=0 -- noteon = 1
  }
end

function psycle.start()  
  psycle.setmachine(require("generatortest.machine"))
end
