--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  sliceidlestate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local hitarea = require("hitarea")

local sliceidlestate = {}

function sliceidlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function sliceidlestate:statustext(view)
  local result = ""
  if not view.slicestart then
    result = "Press left mouse button to start drawing a slice line."
  else
    result = "Move mouse to the end of slice. Release button to slice the events."
  end
  return result
end

function sliceidlestate:handlemousemove(view)
  if view.slicestart then
    view:updateslice()
  end
  return nil 
end

function sliceidlestate:handlemousedown(view)
  view:capturemouse()
  view:startslice()
  view:enableautoscroll()
  return nil
end

function sliceidlestate:handleautoscroll(view)
  if view.slicestart then
    view:updateslice()
  end
end

function sliceidlestate:handlemouseup(view)
  view:releasemouse()
  view:stopslice()  
  view:fls()
  view:preventautoscroll()
  return nil
end

return sliceidlestate
