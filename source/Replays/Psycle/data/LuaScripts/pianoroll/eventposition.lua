--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  eventposition.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local player = require("psycle.player"):new()

local eventposition = {}

function eventposition:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function eventposition:init(position, note, seqpos)
  self.position_ = position and position or 0.0
  self.note_ = note and note or 48
  self.seqpos_ = seqpos and seqpos or 0  
end

function eventposition:clone()
  local clone = eventposition:new()
  clone.position_ = self.position_
  clone.note_ = self.note_
  clone.seqpos_ = self.seqpos_
  return clone
end

function eventposition:set(position, note, seqpos)
  self.position_ = position
  self.note_ = note
  self.seqpos_ = seqpos
  return self
end

function eventposition:setposition(position)
  self.position_ = position
  return self
end

function eventposition:position()
  return self.position_
end

function eventposition:rasterposition()
  return math.floor(self.position_*player:tpb())/player:tpb()
end

function eventposition:setnote(note)
  self.note_ = note
  return self
end

function eventposition:note()
  return self.note_ 
end

function eventposition:setseqpos(seqpos)
  self.seqpos_ = seqpos
  return self
end

function eventposition:seqpos()
  return self.seqpos_
end

return eventposition
