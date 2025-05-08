--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  controltweakstate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local point = require("psycle.ui.point")
local metrics = require("psycle.ui.systemmetrics")
local cursorstyle = require("psycle.ui.cursorstyle")

local controltweakstate = {}

function controltweakstate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self 
  return m
end

function controltweakstate:statustext()
  local result = ""
  if self.event then
    result =  "Parameter is " -- .. self.event:parameter()
  end
  return result
end

function controltweakstate:handlemousemove(view) 
  local diff = self.centermousepos_:y() - view:cursorposition():y()
  if diff ~= 0 then
    local direction = diff < 0 and -1 or 1
    local step = direction * 0.01 
    if step ~= 0 then
      for i=1, #self.selection_.events do
        local event = self.selection_.events[i]    
        event:setnorm(math.max(0, event:norm() + step))
        view.drag:pattern(view)
                 :syncevent(event)      
      end
      view:setcursorpos(self.centermousepos_)
      view:redrawsection(self.sectionindex_)
      view.drag:pattern(view):notify()
    end
  end
  return nil
end

function controltweakstate:handlemouseup(view)  
  return view.states.idling
end

function controltweakstate:enter(view)
  self.sectionindex_ = view:sectionindex(view:mousepos():y()) 
  view:capturemouse()
  self.restoremousepos_ = view:cursorposition()  
  self.centermousepos_ =
     point:new(metrics.screendimension():width() / 2,
               metrics.screendimension():height() / 2)
  view:hidecursor()                                  
      :setcursorpos(self.centermousepos_)    
  self.selection_ = view.drag:selection(view) 
end

function controltweakstate:exit(view)
  view:setcursorpos(self.restoremousepos_) 
      :releasemouse()  
      :showcursor()
      :fls()
end

return controltweakstate
