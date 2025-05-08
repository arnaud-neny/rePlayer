--[[ psycle pianoroll (c) 2017 by psycledelics
File: redrawcommand.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local redrawcommand = {}

function redrawcommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function redrawcommand:init(...)
  self.targets_ = table.pack(...)
end

function redrawcommand:execute()
  for i=1, #self.targets_ do
    self.targets_[i]:fls()
  end
end

return redrawcommand
