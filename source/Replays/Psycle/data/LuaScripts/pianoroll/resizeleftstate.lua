--[[ psycle pianoroll (c) 2017 by psycledelics
File: resizeleftstate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local cursorstyle = require("psycle.ui.cursorstyle")

local resizeleftstate = {}

function resizeleftstate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function resizeleftstate:handlemousemove(view)
  self:update(view)
  return nil
end

function resizeleftstate:update(view)  
  for i=1, #self.selection.events do
    local event = self.selection.events[i]
    local prev = self.selection.prev[i]
    local next = self.selection.next[i]
    local pos = view:eventmousepos():rasterposition()
    if prev == nil or 
       (pos >= 0 and ((prev.stopoffset == 0 and pos > prev:position()) or (prev:hasstop() and pos > prev:position() + prev.stopoffset)) and 
          (nextevent==nil or ((event.stopoffset == 0 and pos < next:position()) or
                                   (event:hasstop() and pos < event:position() + event.stopoffset)))) then
      self.selection.pattern:setleft(event, pos)  
    end   
  end
  view:fls()
end

function resizeleftstate:handlemouseup(view)  
  return view.states.idling
end

function resizeleftstate:enter(view)
  self.selection = view.drag:selection(view)
end

function resizeleftstate:exit(view)
  self.selection = nil
end

return resizeleftstate
