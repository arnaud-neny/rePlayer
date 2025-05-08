--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  machine.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

print = psycle.output

println = function(...)
  psycle.output(...)
  psycle.output("\n")
end

lighten = function(argb, dc)
  local hi, lo = math.floor(argb / 65536), argb % 65536
  local a, r = math.floor(hi / 256), hi % 256 + dc 
  local g, b = math.floor(lo / 256) + dc, lo % 256 + dc
  return  16777216*math.min(math.max(0, a), 255) + 
          65536*math.min(math.max(0, r), 255) + 
          256*math.min(math.max(0, g), 255) + 
            math.min(math.max(0, b), 255)
end

machine = require("psycle.machine"):new()

local mainviewport = require("mainviewport")
local settingsmanager = require("psycle.settingsmanager")
local mainmenu = require("mainmenu")

function machine:info()
  return {
    vendor = "psycle",
    name = "pianoroll",
    mode = machine.HOST,
    version = 0,
    api = 0
  }
end

function machine:init()  
  self.settingsmanager = settingsmanager:new("pianoroll")
  psycle.setsetting(self.settingsmanager)   
  self.mainviewport = mainviewport:new()
  self:setviewport(self.mainviewport)
  self.mainmenu = mainmenu:new(self.mainviewport:selectioncommands())
  psycle.setmenurootnode(self.mainmenu:rootnode())
end

return machine
