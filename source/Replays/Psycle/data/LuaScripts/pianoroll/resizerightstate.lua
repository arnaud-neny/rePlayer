--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  resizerightstate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local resizerightstate = { scrolltimeout = 50 }

local cursorstyle = require("psycle.ui.cursorstyle")

function resizerightstate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function resizerightstate:handlemousemove(view) 
  self:update(view)
  return nil
end

function resizerightstate:update(view)  
  for i=1, #self.selection.events do
    local event = self.selection.events[i]
    if not self.selection.next[i] or view:eventmousepos():rasterposition() < self.selection.next[i]:position() then
      local length = view:eventmousepos():rasterposition() - event:position()
      if length > 0 then
        self.selection.pattern:changelength(event, length)
        view:setlaststop(length)
      end
    end
  end
  view:fls()
end

function resizerightstate:handlemousedown(view)
end

function resizerightstate:handlemouseup(view)  
  return view.states.idling
end

function resizerightstate:enter(view)  
  self.selection = view.drag:selection(view)
  self:update(view)  
end

return resizerightstate
