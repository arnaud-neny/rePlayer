--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  drag.lua
copyright 2017 members of the psycle project http://psycle.source_forge.net
This source_ is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local player = require("psycle.player"):new()
local eventposition = require("eventposition")
local rawpattern = require("psycle.pattern"):new()

local drag = {}

function drag:new(sequence)
  local m = {}         
  setmetatable(m, self)
  self.__index = self  
  m:init(sequence)    
  return m
end

function drag:init(sequence)
  self.sequence_ = sequence
end

function drag:start(view, source)
  self:setsource(source)
  self.dragstart_ =  view:eventmousepos():clone()
  self:calcminmaxnotes()
  self:calcminmaxpositions()
  return self
end

function drag:setsource(source)
  self.source_ = source 
  return self
end

function drag:source()
  return self.source_
end

function drag:stop(view)
  if view:eventmousepos():note() < 120 then
    player:stopnote(view:eventmousepos():note(), 6)
  end
  self.dragstart_ = nil
  return self
end

function drag:abort(view) 
  self:stop(view)
  return self
end

function drag:copy(view)
  local pattern = self.sequence_:at(view:eventmousepos():seqpos())
  for i=1, #self.source_ do 
    self.source_[i] = self.source_[i]:deselect():clone():select(pattern)    
  end
  self:insert(view)
  return self
end

function drag:erase()
  local sourcepattern = self.sequence_:at(self.dragstart_:seqpos())
  if self.source_ then
    for i=1, #self.source_ do   
      sourcepattern:erase(self.source_[i])
    end
  end
  return self
end  

function drag:insert(view)  
  local seqpos = view:eventmousepos():seqpos()
  if seqpos then
    local note = self:dragnote(view:eventmousepos():note())
    local position = self:dragposition(view:eventmousepos():rasterposition())
    if view:eventmousepos():seqpos() == 0 then
      seqpos = 1
      position = self:dragposition(0.0)
    end
    local targetpattern = self.sequence_:at(seqpos)
    if targetpattern and self.source_ then
      for i=1, #self.source_ do
        local event = self.source_[i]  
        event:setnote(event:note() - self.dragstart_:note() + note)
        rawpattern.addundo(targetpattern.ps_, event:track(), event.line,
            1, 1, event:track(), event.line, 0, seqpos - 1);      
        targetpattern:insert(event, event:position() - self.dragstart_:rasterposition() + position)
      end
    end
  end
  return self
end

function drag:dragging()
  return self.dragstart_ ~= nil
end

function drag:pattern(view)
  return self.sequence_:at(view:eventmousepos():seqpos())
end

function drag:minmaxnotes()
  local minnote, maxnote = 119, 0
  for i=1, #self.source_ do
    local note = self.source_[i]:note()
    minnote = math.min(note, minnote)
    maxnote = math.max(note, maxnote)
  end
  return minnote, maxnote
end

function drag:minmaxpositions()
  local minposition, maxposition = 10000000, 0
  for i=1, #self.source_ do
    local position = self.source_[i]:position()
    minposition = math.min(position, minposition)
    maxposition = math.max(position, maxposition)
  end
  return minposition, maxposition
end

function drag:calcminmaxnotes()
  local minnote, maxnote = self:minmaxnotes()
  self.minnote_ = 12 + self.dragstart_:note() - minnote
  self.maxnote_ = 119 + self.dragstart_:note() - maxnote   
end

function drag:calcminmaxpositions()
  local minposition, maxposition = self:minmaxpositions()
  self.minposition_ = self.dragstart_:rasterposition() - minposition 
  self.maxposition_ = self.sequence_:numbeats() + self.dragstart_:rasterposition() - maxposition 
end

function drag:dragnote(note)
  return math.min(self.maxnote_, math.max(self.minnote_, note))
end

function drag:dragposition(position)
  return math.min(self.maxposition_, math.max(self.minposition_, position))
end

function drag:selection(view)
  local pattern = self:pattern(view)
  local selection = { events = {}, prev = {}, next = {} }
  selection.pattern = pattern
  local events = pattern:selection()
  for i=1, #events do
    local event = events[i]
    selection.events[i] = event
    selection.prev[i] = pattern:prevevent(event:track(), event)
    selection.next[i] = pattern:nextevent(event:track(), event)    
  end
  return selection
end



return drag
