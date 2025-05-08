--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  selectidlestate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local hitarea = require("hitarea")

local selectidlestate = {}

function selectidlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  self.hittest = nil
  return m
end

function selectidlestate:statustext()
  return "Press left mouse button to start drawing a selectionbox."
end

function selectidlestate:handlemousemove(view)
  self.hittest = view:hittest() 
  return nil 
end

function selectidlestate:handlemousedown(view)
  local nextstate = nil
  if self.hittest and self.hittest:hitarea() == hitarea.NONE then  
    view:deselectall()   
    nextstate = view.states.selectingevents    
  end
  return nextstate
end

function selectidlestate:enter(view)
  self.hittest = view:hittest()
end

return selectidlestate
