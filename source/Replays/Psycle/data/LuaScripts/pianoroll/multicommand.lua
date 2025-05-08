--[[ psycle pianoroll (c) 2017 by psycledelics
File: multicommand.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local command = require("psycle.command")

local multicommand = command:new()

function multicommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function multicommand:init(...)
  local args = table.pack(...)
  self.commands_ = {}
  for i, v in ipairs(args) do
    self.commands_[i] = v
  end
end

function multicommand:addcommand(command)
  self.commands_[#self.commands + 1] = commnd
  return selfs
end

function multicommand:execute()
  for i=1, #self.commands_ do
    self.commands_[i]:execute()
  end
end

return multicommand
