--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  dragstate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local cursorstyle = require("psycle.ui.cursorstyle")
local player = require("psycle.player"):new()
local keycodes = require("psycle.ui.keycodes")

local dragstate = { scrolltimeout = 45 }

function dragstate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self 
  return m
end

function dragstate:statustext(view)
  return "Note will has be the key " .. view:eventmousepos():note() .. 
         " and beat at " .. view:eventmousepos():rasterposition() .. 
         ". Move the note and release mouse button to commit." 
end

function dragstate:handlemousemove(view)
  self:updateplaynote(view)
  view.keyboard.activemap_ = {}
  view.keyboard:playnote(view:eventmousepos():note())  
  return nil
end

function dragstate:updateplaynote(view)
  if view:eventmousepos():note() < 120 and
    self.playnote ~= view:eventmousepos():note() then
    self.playnote = view:eventmousepos():note()
    player:playnote(self.playnote, 6)    
  end
end

function dragstate:handlemouseup(view)  
  view:stopdrag()
  view:releasemouse()
  view.keyboard:stopnote(view:eventmousepos():note())
  return view.states.idling
end

function dragstate:handlekeydown(view, ev)
  local nextstate = nil
  if ev:keycode() == keycodes.ESCAPE then
    view:abortdrag(view)    
    view:releasemouse()
    nextstate = view.states.idling
  end
  return nextstate
end

function dragstate:enter(view)  
  view:setcursor(cursorstyle.MOVE)
      :capturemouse()
      :fls()
  view.controlview:fls()
  view:enableautoscroll()
  self.playnote = view:eventmousepos():note()
end

function dragstate:exit(view, mousepos)   
  view:fls()
  view.controlview:fls()
  view:preventautoscroll()
end

return dragstate
