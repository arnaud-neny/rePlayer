--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  screenrange.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local screenrange = {}

function screenrange:new(...)
  local m = {}                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)  
  return m
end

function screenrange:init(seqpos, left, right)
  self.seqpos = seqpos
  self.left_, self.right_ = left, right
end

function screenrange:has(event)
  return event:position() + event:length() >= self.left_ and event:position() < self.right_
end

function screenrange:haspos(pos)
  return pos >= self.left_ and pos < self.right_
end

function screenrange:onscreen()
  return self.right_ >= 0 and self.left_ ~= self.right_
end

function screenrange:reset(left, right)
  self.left_, self.right_ = left, right
end

function screenrange:left()
  return self.left_
end

function screenrange:right()
  return self.right_
end


return screenrange
