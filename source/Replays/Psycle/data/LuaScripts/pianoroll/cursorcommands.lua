--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  cursorcommands.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local command = require("psycle.command")
local rawpattern = require("psycle.pattern"):new()

local navleftcommand = command:new()

function navleftcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function navleftcommand:init(cursor)
  self.cursor_ = cursor
end

function navleftcommand:execute()
  self.cursor_:dec()
end

local navrightcommand = command:new()

function navrightcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function navrightcommand:init(cursor)
  self.cursor_ = cursor
end

function navrightcommand:execute()
  self.cursor_:inc()
end

local navupcommand = command:new()

function navupcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function navupcommand:init(cursor)
  self.cursor_ = cursor
end

function navupcommand:execute()
  self.cursor_:decrow()  
end

local navdowncommand = command:new()

function navdowncommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function navdowncommand:init(cursor)
  self.cursor_ = cursor
end

function navdowncommand:execute()
  self.cursor_:incrow()    
end

local navpageupcommand = command:new()

function navpageupcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function navpageupcommand:init(cursor)
  self.cursor_ = cursor
end

function navpageupcommand:execute()
  self.cursor_:decrow()      
end

local navpagedowncommand = command:new()

function navpagedowncommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function navpagedowncommand:init(cursor)
  self.cursor_ = cursor
end

function navpagedowncommand:execute()
  self.cursor_:incrow()      
end

local trackprevcommand = command:new()

function trackprevcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function trackprevcommand:init(cursor)
  self.cursor_ = cursor
end

function trackprevcommand:execute()   
  self.cursor_:dectrack() 
end

local tracknextcommand = command:new()

function tracknextcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function tracknextcommand:init(cursor)
  self.cursor_ = cursor
end

function tracknextcommand:execute()  
  self.cursor_:inctrack()
end

local commandfactory = {}

commandfactory.LEFT = 1
commandfactory.RIGHT = 2
commandfactory.UP = 3
commandfactory.DOWN = 4
commandfactory.PAGEUP = 5
commandfactory.PAGEDOWN = 6
commandfactory.TRACKPREV = 7
commandfactory.TRACKNEXT = 8

function commandfactory.build(cmd, ...)
  local result = nil
  if cmd == commandfactory.LEFT then
    result = navleftcommand:new(...)
  elseif cmd == commandfactory.RIGHT then
    result = navrightcommand:new(...)
  elseif cmd == commandfactory.UP then
    result = navupcommand:new(...)
  elseif cmd == commandfactory.DOWN then
    result = navdowncommand:new(...)    
  elseif cmd == commandfactory.PAGEUP then
    result = navpageupcommand:new(...)
  elseif cmd == commandfactory.PAGEDOWN then
    result = navpagedowncommand:new(...)  
  elseif cmd == commandfactory.TRACKPREV then
    result = trackprevcommand:new(...)
  elseif cmd == commandfactory.TRACKNEXT then
    result = tracknextcommand:new(...)    
  end 
  return result
end

return commandfactory
