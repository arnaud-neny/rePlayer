--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  controlselecteventstate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local rect = require("psycle.ui.rect")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local hitarea = require("hitarea")
local cursorstyle = require("psycle.ui.cursorstyle")

local controlselecteventsstate = {}

function controlselecteventsstate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function controlselecteventsstate:handlemousemove(view)
  view.selectionrect:setright(view:mousepos():x())
  view:fls()
  local events = view:hittestselrect()
  local num = view.sequence:len()
  for i=1, num do    
    view.sequence:at(i):setselection(events[i])    
  end
  view:fls()   
  return nil
end

function controlselecteventsstate:handlemouseup(view) 
  view:adjustcursor(true)
  view.selectionrect = nil
  view.editcommands.editmode:execute()
  view.patternview:fls()
  view:fls()
  return view.states.idling
end

function controlselecteventsstate:enter(view)   
  view.selectionrect = rect:new(point:new(view:mousepos():x(), 0),
                                dimension:new(0, view.grid:preferredheight() - 1))
  view.selectionsection = view:sectionindex(view:mousepos():y())                       
end                        

return controlselecteventsstate
