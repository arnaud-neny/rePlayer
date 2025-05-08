--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  controlidlestate.lua
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

local controlidlestate = {
  cursor = {
    [hitarea.NONE] = cursorstyle.DEFAULT,
    [hitarea.LEFT] = cursorstyle.E_RESIZE,
    [hitarea.FIRST] = cursorstyle.MOVE,
    [hitarea.MIDDLE] = cursorstyle.N_RESIZE,
    [hitarea.MIDDLESTOP] = cursorstyle.MOVE,
    [hitarea.RIGHT]  = cursorstyle.E_RESIZE,
    [hitarea.STOP]  = cursorstyle.CROSSHAIR     
  },
  status = {
    [hitarea.NONE] = "Press left mouse button to insert a command.",
    [hitarea.LEFT] = "Press left mouse button to change the start pos of the command.",
    [hitarea.FIRST] = "Press left mouse button to move or right button to remove the command.",
    [hitarea.MIDDLE] = "Press left mouse button to insert a command or right button to remove the command.",
    [hitarea.MIDDLESTOP] = "",
    [hitarea.RIGHT]  = "Press left mouse button to change the length of the command.",
    [hitarea.STOP]  = "Press left mouse button to remove the command stop." 
  }
}

function controlidlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  self.hittest = nil
  return m
end

function controlidlestate:statustext()
  return self.status[self.hitarea]
end

function controlidlestate:handlemousemove(view)
  self:updatehitarea(view)
  return nil
end

function controlidlestate:updatehitarea(view)
  local hittest = view:hittest(view:mousepos()) 
  if hittest and self.hitarea ~= hittest:hitarea() then    
    self.hitarea = hittest:hitarea()   
    view:setcursor(self.cursor[self.hitarea])    
  end
  self.hittest = hittest
end

function controlidlestate:handlemousedown(view, button)  
  local nextstate = nil 
  if self.hittest then
    if button == 1 then 
      if self.hitarea == hitarea.FIRST or self.hitarea == hitarea.MIDDLESTOP or 
         self.hitarea == hitarea.MIDDLE then
        view:adjustcursor(true)
        if not self.hittest:event():selected() then       
          view.drag.sequence_:deselectall()
          self.hittest:select()
        end        
        nextstate = view.states.tweaking 
      elseif self.hitarea == hitarea.NONE then        
        self:insert(view)
        view:adjustcursor()
        self.hittest:select()
        nextstate = view.states.tweaking
      end
    elseif button == 2 then
      if self.hitarea ~= hitarea.NONE then    
        self.hittest:select()
        self.hittest:pattern():eraseselection() 
      end
      if self.hittest then
        self.hittest:pattern():deselectall()
      end       
      view:fls()    
    end 
  end
  return nextstate
end

function controlidlestate:enter(view)  
  self.hittest, self.hitarea = nil, nil
  self:updatehitarea(view)  
end

function controlidlestate:insert(view)
  self.hittest:setevent(patternevent:new(self.hittest:note(),
                                         self.hittest:position(),
                                         self.hittest.section:track(),
                                         machinebar:currmachine(),
                                         machinebar:curraux()))                       
  self.hittest:event():clearstop()
  self.hittest:event():setnorm(1 - (view:mousepos():y() - self.hittest.sectionoffset) / view.grid:preferredheight())  
  self.hittest:pattern():insert(self.hittest:event(), self.hittest:position())  
  view:fls()
end

return controlidlestate
