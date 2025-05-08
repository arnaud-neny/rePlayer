--[[ 
psycle examples hardsyncdemo (c) 2014 by psycledelics
File:  hardsyncdemo.lua

copyright 2014 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

function psycle.info()
  local machinemodes = require("psycle.machinemodes")  
  return {
    vendor = "psycle",
    name = "hardsyncdemo",
    generator = 1,
    version = 0,
    api = 0,
    noteon = 1
  }
end

function psycle.start()
  psycle.setmachine(require("hardsyncdemo.machine"))
end
