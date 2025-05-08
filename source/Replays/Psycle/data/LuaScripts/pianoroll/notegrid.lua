--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  notegrid.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local hitarea = require("hitarea")
local hittest = require("hittest")
local midihelper = require("psycle.midi")
local rastergrid = require("rastergrid")
local patternevent = require("patternevent")

local notegrid = rastergrid:new()

local trackmodes = { 
  NOTES = 1,
  DRUMS = 2
}

function notegrid:new(...)
  local m = rastergrid:new(...)
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function notegrid:init()
end

function notegrid:draw(g, screenrange, pattern, seqpos)
  self.resizedimension = dimension:new(3, self.view:zoom():height()-2)   
  self:drawevents(g, screenrange, pattern, seqpos)  
  self:drawdragging(g, seqpos)
  self:drawcursorbar(g, seqpos) 
end

function notegrid:drawevents(g, screenrange, pattern, seqpos)
  self.visiblenotes = self.view:visiblenotes()  
  if self.view.paintmode == self.view.DRAWEVENTS then
    local timer = self.view.playtimer   
    if timer:currsequence() + 1 == seqpos then    
      for _, event in pairs(timer:outdatedevents()) do
        if event:position() >= screenrange:right() then      
          break
        end
        if event:isnote() and self:checkevent(g, event, seqpos, screenrange) then
          self:drawcolorevent(g, event, seqpos, screenrange, false)
        end      
      end         
      for _, event in pairs(timer:activeevents()) do
        if event:position() >= screenrange:right() then      
          break
        elseif event:isnote() and self:checkevent(g, event, seqpos, screenrange) then
          self:drawcolorevent(g, event, seqpos, screenrange,true)          
        end   
      end      
    end
  else
    local event = self.view.gridpositions_:firstnote(screenrange, pattern, seqpos)       
    while event and event:position() < screenrange:right() do        
      if self:checkevent(g, event, seqpos, screenrange) then
        local events, trackevent = nil, nil
        event, events, trackevent = self:orderbytrack(event)        
        for i=1, #events do
          self:drawcolorevent(g, events[i], seqpos, screenrange)
        end
        if trackevent then        
          self:drawcolorevent(g, trackevent, seqpos, screenrange)
        end
      end
      event = event.next
    end
  end
end

function notegrid:orderbytrack(event)
  local events = {}
  local trackevent = nil
  local collect = true
  while collect do
    if event:track() == self.view.cursor:track() then
      trackevent = event
    else
      events[#events + 1] = event                  
    end
    collect = event.next and 
              event.next:position() == event:position() and 
              event:note() == event.next:note()
    if collect then                    
      event = event.next
    end          
  end
  return event, events, trackevent
end

function notegrid:checkevent(g, event, seqpos, screenrange, iseventplayed)
  local result = false
  if (self.view.sequence.trackviewmode == self.view.sequence.VIEWALLTRACKS or event:track() == self.view.cursor:track()) and 
       self:iseventonscreen(event)        
    then
      if self.view.keyboard.keymap:has(event:note()) and screenrange:has(event) then
        result = true     
      end   
  end   
  return result
end

function notegrid:drawcolorevent(g, event, seqpos, screenrange, iseventplayed)  
    if not iseventplayed then
      iseventplayed = self.view.player_:playing() and
                      self.view.playtimer:currsequence() == seqpos - 1 and
                      self.view.playtimer:isnoteplayed(event)
    end                              
    self:seteventcolor(g, event, self.view.cursor:track(), iseventplayed)
    self:drawevent(g, event, self.view.cursor.trackmodes_[event:track() + 1], iseventplayed)
    if not self.view.gridpositions_.lastleftevent then
      self.view.gridpositions_:setlastleftevent(event, seqpos)
    end             
end

function notegrid:drawdragging(g, seqpos) 
  if self.view.drag:dragging() and self.view.drag:source() and self.view:eventmousepos():seqpos() and
     (self.view:eventmousepos():seqpos() == seqpos or self.view:eventmousepos():seqpos() == 0)
  then
    local dragnote = self.view.drag:dragnote(self.view:eventmousepos():note())
    local dragposition = self.view:eventmousepos():seqpos() == 0 and
                         self.view.drag:dragposition(0.0) or
                         self.view.drag:dragposition(self.view:eventmousepos():rasterposition())
    for i=1, #self.view.drag:source() do
      local dragevent = patternevent:new(
          self.view.drag:source()[i]:note() - self.view.drag.dragstart_:note() + dragnote, 
          self.view.drag:source()[i]:position() - self.view.drag.dragstart_:rasterposition() + dragposition)          
      dragevent:setlength(self.view.player_:bpt())
      g:setcolor(0xFFA0A0A0)
      self:drawevent(g, dragevent, self.view.cursor.trackmodes_[self.view.cursor:track() + 1])
    end
  end  
end

function notegrid:iseventonscreen(event)
  return event:note() >= self.visiblenotes.bottom  and event:note() <= self.visiblenotes.top
end

function notegrid:hittest(eventposition, pattern)
  local result = hittest:new(eventposition, pattern)  
  if pattern then
    local event = pattern:firstnote()
    while event do
       if event:note() == result:note() then
         event = self:updatetrackorder(self.view.cursor:track(), event)
         if event:overstop(result:position()) then
           result:sethitarea(hitarea.STOP)              
           break
         elseif event:over(result:position()) then
           if self.view:zoom():beatwidth(eventposition:position() - event:position()) < 4 then         
             result:sethitarea(hitarea.LEFT)
           elseif self.view:zoom():beatwidth(event:position() + event:length() - eventposition:position()) < 4 and
                  self.view:zoom():beatwidth(event:position() + event:length() - eventposition:position()) >= 0 then            
             result:sethitarea(hitarea.RIGHT)
           elseif result:position() < event:position() + self.view.player_:bpt() then
             result:sethitarea(hitarea.FIRST)
           else 
             result:sethitarea(event:hasstop() and hitarea.MIDDLESTOP or hitarea.MIDDLE)             
           end
           break
         end  
       end
       event = event.next     
    end
    if result:hitarea() ~= hitarea.NONE then
      result:setevent(event)   
    end
  end
  return result
end

function notegrid:updatetrackorder(track, event)
  local result = event
  while result:track() ~= track and 
        result.next and  
        result:note() == result.next:note() and           
        result:position() == result.next:position() do
    result = result.next
  end 
  return result
end

function notegrid:hittestselrect()
  local result = {}
  if self.view.selectionrect then
    local rect = self.view.selectionrect
    local num = self.view.sequence:len()
    for i=1, num do         
      local gridpos = self.view:gridpositions():pos(i)
      local top = self.view.keyboard.keymap.range.to_ - math.floor((rect:top()) / self.view:zoom():height())
      local bottom = self.view.keyboard.keymap.range.to_ - math.floor((rect:bottom()) / self.view:zoom():height())
      notetop = math.max(top, bottom)
      notebottom = math.min(top, bottom)
      local left = (rect:left() - gridpos)  / self.view:zoom():width()
      local right = (rect:right() - gridpos)  / self.view:zoom():width()
      beatleft = math.min(left, right)
      beatright = math.max(left, right)
      local event = self.view.sequence:at(i):firstnote()
      while event do        
        if (event:note() <= notetop and event:note() >= notebottom) and 
          event:position() + event:length() >= beatleft and
          event:position() <= beatright and
         (self.view.sequence.trackviewmode == self.view.sequence.VIEWALLTRACKS or event:track() == self.view.cursor:track())
          then
            if not result[i] then
              result[i] = {}
            end
            result[i][#result[i]+1] = event              
        end
        event = event.next        
      end
    end
  end  
  return result
end

function notegrid:updateeventdimension()  
  self.resizedimension = dimension:new(4, self.view:zoom():height()) 
end

function notegrid:drawevent(g, event, trackmode, iseventplayed)
  local y = self.view.keyboard:keypos(event:note())  
  local position = rect:new(point:new(self.view:zoom():beatwidth(event:position()), y + 1),
                            point:new(self.view:zoom():beatwidth(event:position() + event:length()),
                                      y + self.view:zoom():height() - 1))       
  if trackmode == trackmodes.NOTES then    
    self:drawnote(g, event, position, iseventplayed)
  else       
    self:drawdrum(g, event, position, iseventplayed)
    if event:hasstop() then
      self:drawnotestop(g, position)
    else
      self:drawsizer(g, position, trackmode)    
    end
  end
end

function notegrid:drawnote(g, event, position, iseventplayed)
  g:fillrect(position)   
  self:drawsizer(g, position, trackmodes.NOTES)
  self:drawnotename(g, event, position, iseventplayed)
  if event:hasstop() then
    self:drawnotestop(g, position)
  end
end

function notegrid:drawdrum(g, event, position)
  g:drawstring("X", position:topleft())
  local midy = position:top() + position:height() / 2
  g:drawline(point:new(position:left(), midy), point:new(position:right(), midy))
end

function notegrid:drawsizer(g, position, trackmode)
  g:setcolor(self.view.colors.eventresizecolor)
  if trackmode == trackmodes.NOTES then
    g:fillrect(rect:new(position:topleft(), self.resizedimension))       
    g:fillrect(rect:new(point:new(position:right() - 3, position:top()), self.resizedimension))
  else
    g:fillrect(rect:new(point:new(position:right() - 1, position:top()), dimension:new(1, self.view:zoom():height())))
  end
end

function notegrid:drawnotename(g, event, position, iseventplayed)
  if self.view:zoom():height() >= 12 then
    if iseventplayed then
      g:setcolor(self.view.colors.fonteventplaycolor)
    else
      g:setcolor(self.view.colors.fonteventcolor)
    end
    g:drawstring(midihelper.notename(event:note() - 12), point:new(position:left() + 5, position:top()))
  end
end

function notegrid:drawnotestop(g, position)
  g:setcolor(self.view.colors.eventresizecolor)
  g:drawrect(rect:new(point:new(position:right() + 1, position:top()),
                      dimension:new(self.view:zoom():rasterwidth() - 2, position:height() - 1)))
end

function notegrid:drawkeylines(g) 
  local w = self.bgbeats * self.view:zoom():width()
  g:setcolor(self.view.colors.backgroundcolor)  
  for note=self.view.keyboard.keymap.range.from_, self.view.keyboard.keymap.range.to_ do
    local y = self.view.keyboard:keypos(note) 
    if self.view.keyboard:isoctave(note) then
      g:setcolor(self.view.colors.octaverowcolor)
      g:fillrect(rect:new(point:new(0, y),
                          dimension:new(w, self.view:zoom():height())))
      g:setcolor(self.view.colors.backgroundcolor)                           
    end 
    g:drawline(point:new(0, y), point:new(w, y))               
  end
end

function notegrid:bardimension()
   return dimension:new(math.floor(self.beatsperbar * self.view:zoom():width()),
                        math.floor(self:preferredheight()))
end

function notegrid:preferredheight()
  return math.floor(self.view.keyboard.keymap:numkeys() * self.view:zoom():height())
end

return notegrid

