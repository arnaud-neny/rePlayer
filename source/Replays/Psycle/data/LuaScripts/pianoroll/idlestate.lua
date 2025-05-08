--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  idlestate.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]
local player = require("psycle.player")
local hitarea = require("hitarea")
local cursorstyle = require("psycle.ui.cursorstyle")
local machinebar = require("psycle.machinebar"):new()

local idlestate = {
  cursor = {
    [hitarea.NONE] = cursorstyle.DEFAULT,
    [hitarea.LEFT] = cursorstyle.E_RESIZE,
    [hitarea.FIRST] = cursorstyle.MOVE,
    [hitarea.MIDDLE] = cursorstyle.DEFAULT,
    [hitarea.MIDDLESTOP] = cursorstyle.DEFAULT,
    [hitarea.RIGHT]  = cursorstyle.E_RESIZE,
    [hitarea.STOP]  = cursorstyle.DEFAULT    
  },
  status = {
    [hitarea.NONE] = "Press left mouse button to insert a note and set the cursor.",
    [hitarea.LEFT] = "Press left mouse button to change the start pos of the note.",
    [hitarea.FIRST] = "Press left mouse button to move or right button to remove the note.",
    [hitarea.MIDDLE] = "Press left mouse button to insert a note or right button to remove the note.",
    [hitarea.MIDDLESTOP] = "",
    [hitarea.RIGHT]  = "Press left mouse button to change the length of the note.",
    [hitarea.STOP]  = "Press left mouse button to remove the note stop."   
  }
}

function idlestate:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  m:init()
  return m
end

function idlestate:init()
  self.player_ = player:new()
  self.hitarea = hitarea.NONE
end

function idlestate:statustext()
  return self.status[self.hitarea]
end

function idlestate:handlemouseup(view)
  self:updatecursor(view)
  return nil
end

function idlestate:handlemousemove(view)  
  self:updatecursor(view)  
  return nil 
end

function idlestate:updatecursor(view)
  self.hittest = view:hittest()
  self.hitarea = self.hittest and self.hittest:hitarea() or hitarea.NONE
  view:setcursor(self.cursor[self.hitarea])
end

function idlestate:handlemousedown(view, button)  
  local nextstate = nil
  if self.hittest then
    if button == 1 then
      if self.hitarea == hitarea.RIGHT then 
        view.sequence:deselectall()
        self.hittest:select()
        nextstate = view.states.resizingright  
      elseif self.hitarea  == hitarea.LEFT then 
        view.sequence:deselectall()
        self.hittest:select()
        nextstate = view.states.resizingleft
      elseif self.hitarea == hitarea.FIRST or self.hitarea == hitarea.MIDDLESTOP then        
        view.cursor:settrack(self.hittest:event():track())
        if not self.hittest:event():selected() then
          self.hittest:pattern():deselectall()
        end
        self.hittest:select()
        view:startdrag(self.hittest:pattern():selection())
        self.player_:playnote(view:eventmousepos():note(), 6)
        view.keyboard:playnote(view:eventmousepos():note())        
        nextstate = view.states.dragging     
      elseif self.hitarea == hitarea.NONE or self.hitarea == hitarea.MIDDLE or
        (self.hitarea == hitarea.STOP and button == 1) then
        view:adjustcursor()        
        self:insert(view)        
        view:startdrag(view.drag:pattern(view):selection())
        nextstate = view.states.dragging   
      end
    elseif button == 2 then
      if self.hitarea == hitarea.STOP then        
        self.hittest:pattern():erasestopoffset(self.hittest:event())
        view:setlaststop(0)      
      elseif self.hitarea ~= hitarea.NONE then
        if not self.hittest:event():selected() then
          self.hittest:pattern():deselectall()
        end        
        self.hittest:select()
        self.hittest:pattern():eraseselection() 
      end
      view.sequence:deselectall()
    end
    view:fls()
    self:updatecursor(view)
  end
  return nextstate
end

function idlestate:enter(view)
  self.player_:stopnote(view:eventmousepos():note(), 6)  
  self:updatecursor(view)
end

function idlestate:insert(view)
  if view.drag:pattern(view) then
    local events = view.drag:pattern(view):copy(view:chord():pattern(),
                                                view:eventmousepos():note() - view:chord():offset(),
                                                view:eventmousepos():rasterposition(),
                                                view.cursor:track(),
                                                view.laststopoffset_)
    for i=1, #events do
      events[i]:setmach(machinebar:currmachine())
               :setinst(machinebar:curraux())               
    end                                     
    view.drag:pattern(view):deselectall():setselection(events)                                    
    self.player_:playnote(view:eventmousepos():note(), 6)
    view.keyboard:playnote(view:eventmousepos():note())
  end                              
end

return idlestate
