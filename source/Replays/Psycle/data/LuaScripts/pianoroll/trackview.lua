--[[ psycle pianoroll (c) 2017 by psycledelics
File: trackview.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local region = require("psycle.ui.region")
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local window = require("psycle.ui.window")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local player = require("psycle.player"):new()
local patternevent = require("patternevent")
local keycodes = require("psycle.ui.keycodes")
local cmddef = require("psycle.ui.cmddef")
local machinebar = require("psycle.machinebar"):new()
local trackedit = require("trackedit")
local rawpattern = require("psycle.pattern"):new()
local listener = require("psycle.listener")

local trackview = window:new(parent)

trackview.DRAWALL = 1
trackview.DRAWPATTERNEVENTS = 2

trackview.EDITCURSOR = 1
trackview.EDITPLAY = 2

local hexinput = {
  [keycodes.DIGIT0] = 0, [keycodes.DIGIT1] = 1, [keycodes.DIGIT2] = 2,
  [keycodes.DIGIT3] = 3, [keycodes.DIGIT4] = 4, [keycodes.DIGIT5] = 5,
  [keycodes.DIGIT6] = 6, [keycodes.DIGIT7] = 7, [keycodes.DIGIT8] = 8,
  [keycodes.DIGIT9] = 9, [keycodes.KEYA] = 10, [keycodes.KEYB] = 11,
  [keycodes.KEYC] = 12, [keycodes.KEYD] = 13, [keycodes.KEYE] = 14,
  [keycodes.KEYF] = 15
}

local status = {
  [1] = "Press left mouse to toggle the note rendering mode",
  [2] = "Press left mouse to solo the track.",
  [3] = "Press left mouse to mute the track.", 
  [4] = "Press left mouse to record the track.", 
}

trackview.colors = {
  CURSOR = 0xFF0000FF,
  FONT = 0xFFFFFFFF, 
  FONTCURSOR = 0xFFFFFFFF,
  BG = 0xFF999999
}

function trackview:typename()
  return "trackview"
end

function trackview:new(parent, ...)
  local c = window:new()                  
  setmetatable(c, self)
  self.__index = self  
  c:init(...)
  if parent ~= nil then
    parent:add(c)
  end  
  return c
end

function trackview:init(sequence, cursor, scroller)
  self.sequence_ = sequence
  self.sequence_:addlistener(self)
  self:setautosize(true, false)
  self.statuslisteners_ = listener:new()    
  self.cursor = cursor
  self.cursor:settrackview(self)  
  self.trackedit = trackedit:new(cursor, self)
  self:viewdoublebuffered()
  self.player_ = player  
  self.activeevents_ = {}
  self.repaintmode = trackview.DRAWALL 
  self.trackeditmode = self.EDITCURSOR
end

function trackview:transparent()
  return false
end

function trackview:toggletrackeditmode()
  self.trackeditmode = self.trackeditmode == self.EDITCURSOR and self.EDITPLAY or self.EDITCURSOR
  self:fls()
end

function trackview:draw(g, region)
  tracks = {
    from = self.cursor:scrolloffset(),
    to = math.min(self.cursor:scrolloffset() + self:visibletracks(), rawpattern:numtracks()-1)
  }
  g:setcolor(self.colors.BG)
  g:fillrect(region:bounds())  
  self:drawbackground(g, tracks)
  self:drawevents(g, tracks)
end 

function trackview:trackposition(track)
  return rect:new(point:new(self.trackedit.trackwidth_*(track - self.cursor:scrolloffset()), 0),
                  dimension:new(self.trackedit.trackwidth_, self:position():height()))
end

function trackview:drawbackground(g, tracks)  
  for track = tracks.from, tracks.to do    
    self.trackedit:draw(g, track, self.cursor)      
  end  
end

function trackview:visibletracks()
   return math.floor(self:position():width() / self.trackedit.trackwidth_)
end

function trackview:drawevents(g, tracks)
  if self.trackeditmode == trackview.EDITCURSOR then
    local pattern = self.sequence_:at(self.cursor:seqpos())
    if pattern then
      local numtracks = rawpattern:numtracks()
      local event = pattern:firstnote()
      while event and event:track() >= tracks.from and event:track() <= tracks.to do
        if event:position() == self.cursor:position() then
          local trackoffset = self.trackedit:headerleft(event:track(), self.cursor:scrolloffset())    
          g:translate(point:new(trackoffset, 0))
          self.trackedit:drawpatternevent(g, event)
          g:retranslate()
        end
        event = event.next
      end
      local event = pattern:firstcmd()
      while event and event:track() >= tracks.from and event:track() <= tracks.to do
        if event:position() == self.cursor:position() then
          local trackoffset = self.trackedit:headerleft(event:track(), self.cursor:scrolloffset())    
          g:translate(point:new(trackoffset, 0))        
          self.trackedit:drawpatternevent(g, event)
          g:retranslate()
        end
        event = event.next
      end      
    end
  else
    for i=1, #self.activeevents_ do
      local event = self.activeevents_[i]
      local trackoffset = self.trackedit:headerleft(event:track(), self.cursor:scrolloffset())    
      g:translate(point:new(trackoffset, 0))        
      self.trackedit:drawpatternevent(g, event, event:track(), self.cursor)
      g:retranslate()
    end
  end
end
  
function trackview:updatecursorposition()
  for i=1, 16 do
    if self.trackheaders[i].edit.hasfocus then
      self.trackheaders[i].edit.hasfocus = false
      self.trackheaders[i].edit:fls()
      break
    end
  end
  self.trackheaders[self.cursor:track()].edit.hasfocus = true
  self.trackheaders[self.cursor:track()].edit:fls()
end

function trackview:setproperties(properties)   
  if properties.backgroundcolor then
    self.backgroundcolor_ = properties.backgroundcolor:value()   
  end
end

function trackview:onplaybarupdate(timer)
  self.activeevents_ = timer.activeevents_
  if self.trackeditmode == trackview.EDITPLAY then
    if timer:haschanged() then
      self:fls()
    end
  else
    self:fls()
  end
end

function trackview:oncalcautodimension()
  return dimension:new(200, 200)
end

function trackview:onmousedown(ev)   
  local track = self.trackedit:hittesttrack(ev:windowpos(), self.cursor:scrolloffset())
  local trackchanged = self.cursor:track() ~= track 
  local col = self.trackedit:hittestcol(
      point:new(ev:windowpos():x() - self.trackedit:headerleft(track, self.cursor:scrolloffset()),
                ev:windowpos():y()))     
  self.cursor:setcolumn(col ~= -1 and col or 1)   
  if trackchanged then
    self.cursor:settrack(track)
  else
    self:redrawtrack(track)
  end
  if col ~= -1 then
    self:setstatus("Enter " .. self.trackedit.colpositions[col].label)
  end
end

function trackview:onmousemove(ev) 
  local track = self.trackedit:hittesttrack(ev:windowpos(), self.cursor:scrolloffset())
  local col = self.trackedit:hittestcol(
      point:new(ev:windowpos():x() - self.trackedit:headerleft(track, self.cursor:scrolloffset()),
                ev:windowpos():y()))
  if col ~= -1 then
    self:setstatus("Enter " .. self.trackedit.colpositions[col].label)
  end
end

function trackview:mixdigit(input, val)
  local result = 0
  local iseven = self.cursor:column() % 2 == 0
  if iseven then
    result = input*16 + val % 16
  else
    result = math.floor(val/16)*16 + input
  end
  return result
end

function trackview:inctrackoffset()
  self.cursor:scrollleft()
  self:fls()
end

function trackview:dectrackoffset()
  self.cursor:scrollright()
  self:fls()
end

function trackview:handlekeyinput(ev)
  if not ev:ispropagationstopped() and not ev:shiftkey() and not ev:ctrlkey() and self.cursor:column() > 1 then    
    local event = self:editpatternevent()   
    if event then      
     local input = hexinput[ev:keycode()]        
      if input then
        if self.cursor:column() == 2 or  self.cursor:column() == 3 then     
          event.pattern:setinst(event, self:mixdigit(input, event:inst()))
        elseif self.cursor:column() == 4 or  self.cursor:column() == 5 then     
          event.pattern:setmach(event, self:mixdigit(input, event:mach()))
        elseif self.cursor:column() == 6 or  self.cursor:column() == 7 then                 
          event.pattern:setcmd(event, self:mixdigit(input, event:cmd()))
        elseif self.cursor:column() == 8 or  self.cursor:column() == 9 then  
          event.pattern:setparameter(event, self:mixdigit(input, event:parameter()))
        end
        self.cursor:inc()
        ev:stoppropagation()
      end
    end   
  end
end

function trackview:editpatternevent()
  local result = nil
  if self.trackeditmode == self.EDITCURSOR then
    local pattern = self.sequence_:at(self.cursor:seqpos())
    if pattern then
      local event = pattern:firstnote()
      while event do
        if event:track() == self.cursor:track() and event:position() == self.cursor:position() then       
          result = event
          event.pattern = pattern          
          break
        end
        event = event.next
      end   
      if not result then
        local event = pattern:firstcmd()
        while event do
          if event:track() == self.cursor:track() and event:position() == self.cursor:position() then   
            event.pattern = pattern          
            result = event        
            break
          end
          event = event.next
        end      
      end
    end
  else
    result = self:playpatternevent()
  end
  return result
end

function trackview:playpatternevent()
  local result = nil
  for i=1, #self.activeevents_ do
    local event = self.activeevents_[i]
    if event:track() == self.cursor:track() then       
      result = event        
      break
    end
  end
  return result
end

function trackview:setstatus(text)
  self.statuslisteners_:notify(text, "onstatus")
end

function trackview:onmouseenter()
  self:setstatus("")
end

function trackview:oncalcautodimension()
  return dimension:new(200, 23)
end

function trackview:ontrackchanged()
  if self.cursor:track() ~= self.cursor:prevtrack() then   
    self:redrawtrack(self.cursor:prevtrack())
  end
  self:redrawtrack(self.cursor:track())
end

function trackview:ontrackscroll()
  self:fls()
end

function trackview:redrawtrack(track)
  self:fls(region:new():setrect(
    self.trackedit:trackposition(track, self.cursor:scrolloffset())))
end

function trackview:setproperties(properties)  
  trackview.colors.FONT = properties.fontcolor and properties.fontcolor:value() or trackview.colors.FONT
  trackview.colors.BG = properties.backgroundcolor  and properties.backgroundcolor:value() or trackview.colors.BG
  trackview.colors.CURSOR = properties.cursorcolor and properties.cursorcolor:value() or trackview.colors.CURSOR
  trackview.colors.FONTCURSOR = properties.fontcursorcolor and properties.fontcursorcolor:value() or trackview.colors.FONTCURSOR
end

function trackview:addstatuslistener(listener)
  self.statuslisteners_:addlistener(listener)
  return self
end

function trackview:onpatternchanged(pattern)
  if self.sequence_:at(self.cursor:seqpos()) == pattern then
    self:fls()
  end
end

return trackview
