-- psycle pianoroll (c) 2017 by psycledelics
-- File: chord.lua
-- copyright 2017 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local listeners = require("psycle.listener")
local pattern = require("pattern")
local patternevent = require("patternevent")

local psbdir = require("psycle.config"):new("PatternVisual"):luapath() .. 
               "\\pianoroll\\chords\\"

local chord = { offset_ = 24 }

function chord:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function chord:init(name)
  self.listeners_ = listeners:new()  
  self.pattern_ = pattern:new()
  if name then
    self.name_ = name
    self.pattern_:importpsb(psbdir .. name .. ".psb")
  end
end

function chord:setname(name)
  self.name_ = name
  return self
end

function chord:name()
  return self.name_
end

function chord:clear()
  self.pattern_:clear()
  return self
end

function chord:setroot()
  self.pattern_:clear() 
  self.pattern_:insert(patternevent:new(self.offset_, 0))
  self.pattern_.numlines_ = 1
  self:notify()
  return self
end

function chord:dump()
  self.pattern_:dump()
end

function chord:addlistener(listener)
  self.listeners_:addlistener(listener)
  return self
end

function chord:notify()
  self.listeners_:notify(self, "chordchanged")
  return self
end

function chord:pattern()
  return self.pattern_
end

function chord:offset()
  return self.offset_
end

return chord
