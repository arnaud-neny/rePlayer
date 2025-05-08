--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  hittest.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local hitarea = require("hitarea")

local hittest = {}

function hittest:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function hittest:init(eventposition, pattern)
  self.hitarea_ = hitarea.NONE
  self.pattern_ = pattern 
  self.eventposition_ = eventposition:clone()
end

function hittest:select()
  if self.event_ then
    self.event_:select(self.pattern)
  end
  return self
end

function hittest:note()
  return self.eventposition_:note()
end

function hittest:position()
  return self.eventposition_:rasterposition()
end

function hittest:sethitarea(hitarea)
  self.hitarea_ = hitarea
  return self
end

function hittest:hitarea()
  return self.hitarea_
end

function hittest:setevent(event)
  self.event_ = event
  return self
end

function hittest:event()
  return self.event_
end

function hittest:pattern()
  return self.pattern_
end

return hittest
