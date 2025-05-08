--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  chordselectidlestate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local idlestate = {}

function idlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self  
  return m
end

function idlestate:statustext()
  return ""
end

return idlestate
