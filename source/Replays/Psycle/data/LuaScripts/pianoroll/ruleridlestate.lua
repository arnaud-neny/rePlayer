--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  ruleridlestate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local ruleridlestate = {}

function ruleridlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self  
  return m
end

function ruleridlestate:statustext()
  return "Press left mouse button to set the cursor."
end

function ruleridlestate:handlemousedown(view, mousepos, button)  
  if view:eventmousepos():seqpos() then
    view.cursor:setseqpos(view:eventmousepos():seqpos())
               :setposition(view:eventmousepos():rasterposition())    
  end  
  return nil
end

return ruleridlestate
