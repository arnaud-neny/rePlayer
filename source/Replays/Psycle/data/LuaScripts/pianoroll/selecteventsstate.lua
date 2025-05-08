--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  selecteventsstate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local rect = require("psycle.ui.rect")

local selecteventsstate = {}

function selecteventsstate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function selecteventsstate:statustext()
  return "Move mouse to size the selection box. Release mouse to select events inside the box."
end

function selecteventsstate:handlemousemove(view)
  view.selectionrect:setbottomright(view:mousepos())
  local events = view:hittestselrect()
  for i=1, view.sequence:len() do    
    view.sequence:at(i):setselection(events[i])    
  end
  view:fls()  
  return nil
end

function selecteventsstate:handleautoscroll(view)
  self:handlemousemove(view)
end

function selecteventsstate:handlemouseup(view)
  view:releasemouse()
  return view.states.idling
end

function selecteventsstate:enter(view)
  view:enableautoscroll()
  view:capturemouse()
  view.selectionrect = rect:new(view:mousepos(), view:mousepos())
end

function selecteventsstate:exit(view, mousepos)
  view:preventautoscroll()
  view.editcommands.editmode:execute()
  view.selectionrect = nil
  view:fls()
end

return selecteventsstate
