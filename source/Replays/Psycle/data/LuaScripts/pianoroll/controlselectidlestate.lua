--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  controlselectidlestate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local hitarea = require("hitarea")
local cursorstyle = require("psycle.ui.cursorstyle")
local patternevent = require("patternevent")
local player = require("psycle.player"):new()
local machinebar = require("psycle.machinebar"):new()
local keycodes = require("psycle.ui.keycodes")
local rect = require("psycle.ui.rect")
local point = require("psycle.ui.point")

local controlselectidlestate = {
  cursor = {
    [hitarea.NONE] = cursorstyle.DEFAULT,
    [hitarea.LEFT] = cursorstyle.E_RESIZE,
    [hitarea.FIRST] = cursorstyle.MOVE,
    [hitarea.MIDDLE] = cursorstyle.MOVE,
    [hitarea.MIDDLESTOP] = cursorstyle.MOVE,
    [hitarea.RIGHT]  = cursorstyle.E_RESIZE,
    [hitarea.STOP]  = cursorstyle.CROSSHAIR,
    [hitarea.PLAYBAR]  = cursorstyle.E_RESIZE  
  },
  status = {
    [hitarea.NONE] = "Press left mouse to begin the selection.",
    [hitarea.LEFT] = "Press left mouse button to change the start pos of the command.",
    [hitarea.FIRST] = "Press left mouse button to move or right button to remove the command.",
    [hitarea.MIDDLE] = "Press left mouse button to insert a command or right button to remove the command.",
    [hitarea.MIDDLESTOP] = "",
    [hitarea.RIGHT]  = "Press left mouse button to change the length of the command.",
    [hitarea.STOP]  = "Press left mouse button to remove the command stop.",
    [hitarea.PLAYBAR]  = "Press left mouse button to move the play position."  
  }
}

function controlselectidlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  self.hittest = nil
  return m
end

function controlselectidlestate:statustext()
  return self.status[self.hitarea]
end

function controlselectidlestate:handlemousemove(view)
  self:updatehitarea(view)
  return nil 
end

function controlselectidlestate:updatehitarea(view)
  local hittest = view:hittest(view:mousepos()) 
  if hittest and self.hitarea ~= hittest:hitarea() then    
    self.hitarea = hittest:hitarea()    
    view:setcursor(self.cursor[self.hitarea])      
  end
  self.hittest = hittest
end

function controlselectidlestate:handlemousedown(view, button)  
  local nextstate = nil 
  if button == 1 then 
    if self.hitarea == hitarea.RIGHT then    
      nextstate = nil
    elseif self.hitarea  == hitarea.LEFT then         
      nextstate = nil
    elseif self.hitarea == hitarea.FIRST or self.hitarea == hitarea.MIDDLESTOP then
    elseif self.hitarea == hitarea.NONE or self.hitarea == hitarea.MIDDLE then
     view:deselectall()
     nextstate = view.states.selectingevents
    elseif self.hitarea == hitarea.PLAYBAR then
      nextstate = view.states.movingplaybar     
    end
  elseif button == 2 then
    if self.hitarea ~= hitarea.NONE then
      self.hittest:pattern():erasecmd(self.hittest:event())      
      view:fls()
    end  
  end 
  return nextstate
end

function controlselectidlestate:handlemouseup(view)  
end

function controlselectidlestate:handlekeydown(view, ev)  
  if ev:ctrlkey() and ev:keycode() == keycodes.KEYB then
    self.selbegin = view.cursor:position()
  elseif ev:ctrlkey() and ev:keycode() == keycodes.KEYE then
    self.selend = view.cursor:position()  
  end  
  self:updateselection(view)
end

function controlselectidlestate:enter(view) 
  self.hittest, self.hitarea = nil, nil
  self:updatehitarea(view) 
end

function controlselectidlestate:insert(view)
  self.hittest.note = 255 --
  --view:keyfrom(mousepos:y())  
  self.hittest:setevent(patternevent:new(self.hittest:note(),
                                        self.hittest:position(),
                                        view.cursor:track(),
                                        machinebar:currmachine(),
                                        machinebar:curraux())
                                    :setstopoffset(0))      
  self.hittest:event():setparameter(math.floor(255*(1 - view:mousepos():y()/40)))                                  
  self.hittest:pattern():insert(self.hittest:event(), self.hittest:position())   
  view.drag:sethittest(self.hittest, self.hittest:event())
  view:fls()
end

return controlselectidlestate
