--[[ psycle plugineditor (c) 2017 by psycledelics
File: commands.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local command = require("psycle.command")

local runcommand = command:new()

function runcommand:new(mainviewport)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.mainviewport = mainviewport
  return c
end

function runcommand:execute()
  self.mainviewport:playplugin()
end

local commandfactory = {}

commandfactory.RUN = 1

function commandfactory.build(cmd, ...)
  local result = nil
  if cmd == commandfactory.RUN then
    result = runcommand:new(...)    
  end 
  return result
end

return commandfactory
