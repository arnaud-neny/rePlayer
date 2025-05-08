--[[ psycle pianoroll (c) 2017 by psycledelics
File: trackheaderview.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local region = require("psycle.ui.region")
local alignstyle = require("psycle.ui.alignstyle")
local window = require("psycle.ui.window")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local player = require("psycle.player"):new()
local patternevent = require("patternevent")
local keycodes = require("psycle.ui.keycodes")
local cmddef = require("psycle.ui.cmddef")
local machinebar = require("psycle.machinebar"):new()
local trackheader = require("trackheader")
local rawpattern = require("psycle.pattern"):new()
local listener = require("psycle.listener")

local trackheaderview = window:new(parent)

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

trackheaderview.colors = {
  BORDER = 0xFF666666,
  FONT = 0xFFFFFFFF, 
  BG = 0xFF999999
}

function trackheaderview:typename()
  return "trackheader"
end

function trackheaderview:new(parent, ...)
  local c = window:new()                  
  setmetatable(c, self)
  self.__index = self  
  c:init(...)
  if parent ~= nil then
    parent:add(c)
  end  
  return c
end

function trackheaderview:init(cursor) 
  self:setautosize(true, false)  
  self.cursor = cursor
  self.player_ = player
  self.trackheader = trackheader:new(self)  
  self.statuslisteners_ = listener:new()
  self:viewdoublebuffered() 
end

function trackheaderview:transparent()
  return false
end

function trackheaderview:draw(g, region)
  self:clearbg(g)
  local from, to = self:trackrange(region)
  for track = from, to do       
    self.trackheader:draw(g, track, self.cursor)      
  end  
end

function trackheaderview:trackrange(region)
  return
    math.min(rawpattern:numtracks() - 1,
             math.floor(region:bounds():left() / self.trackheader:trackwidth())
             + self.cursor:scrolloffset()),
    math.min(rawpattern:numtracks() - 1,
             math.floor((region:bounds():right() - 1) / self.trackheader:trackwidth())
             + self.cursor:scrolloffset())  
end

function trackheaderview:clearbg(g)   
  g:setcolor(self.colors.BG)
  g:fillrect(self:position())
end

function trackheaderview:visibletracks()
  return math.floor(self:position():width() / self.trackheader.fieldwidth_)
end

function trackheaderview:setproperties(properties)   
  if properties.backgroundcolor then
    self.backgroundcolor_ = properties.backgroundcolor:value()   
  end
end

function trackheaderview:oncalcautodimension()
  return dimension:new(200, 200)
end

function trackheaderview:onmousedown(ev)  
  local track = self.trackheader:hittesttrack(ev:windowpos(), self.cursor:scrolloffset())
  local trackchanged = self.cursor:track() ~= track
  self.cursor:settrack(track)
  local icon = self:hittesticon(ev:windowpos())
  self:toggleicon(icon, track) 
  if trackchanged then    
    self.cursor:settrack(track)    
  end
  self:fls()
end

function trackheaderview:onmousemove(ev)
  local track = self.trackheader:hittesttrack(ev:windowpos(), self.cursor:scrolloffset())  
  local icon = self:hittesticon(ev:windowpos())
  local oldicon = self.trackheader.hover.icon
  local oldhovertrack = self.trackheader.hover.track_
  self.trackheader.hover.track_ = track
  self.trackheader.hover.icon = icon
  if oldicon ~= icon or oldhovertrack ~= track then
    if icon > 0 and icon < 5 then
      self:setstatus(status[icon])
    end
    if oldhovertrack and oldhovertrack ~= track then
      self:redrawtrack(oldhovertrack)
    end    
    self:redrawtrack(track)
  end
end

function trackheaderview:onmouseout(ev)
  if self.trackheader.hover.icon ~= 0 then
    self.trackheader.hover.icon = 0
    self:fls()
  end
end

function trackheaderview:hittesticon(point)
  local track = self.trackheader:hittesttrack(point, self.cursor:scrolloffset())
  local result = -1  
  for i, field in pairs(self.trackheader.fields) do     
    local fieldrect = rect:new(point:new(self.trackheader:headerleft(track, self.cursor:scrolloffset()) + field.left + 3, 3),
      dimension:new(field.width, 15))
    if fieldrect:intersect(point) then
      result = i
      break
    end
  end
  return result
end

function trackheaderview:toggleicon(icon, track)
  if icon ~= -1 then
    if icon == 1 then
      self.cursor:toggletrackmode(track)
    elseif icon == 2 then
      player:toggletracksoloed(track)
    elseif icon == 3 then
      player:toggletrackmuted(track)
    elseif icon == 4 then
      player:toggletrackarmed(track)
    end
  end
end

function trackheaderview:playposition()
  local sequence = player:playpattern()
  return sequence, self.patternview.grids[sequence + 1].pattern:linetobeat(player:line())
end

function trackheaderview:setstatus(text)
  self.statuslisteners_:notify(text, "onstatus")
end

function trackheaderview:onmouseenter()
  self:setstatus("")
end

function trackheaderview:oncalcautodimension()
  return dimension:new(200, 23)
end

function trackheaderview:ontrackchanged()
 if self.cursor:track() ~= self.cursor:prevtrack() then   
    self:redrawtrack(self.cursor:prevtrack())
  end
  self:redrawtrack(self.cursor:track())
end

function trackheaderview:ontrackscroll()
  self:fls()
end

function trackheaderview:redrawtrack(track)
  self:fls(region:new():setrect(
    self.trackheader:trackposition(track, self.cursor:scrolloffset())))
end

function trackheaderview:setproperties(properties)
  trackheaderview.colors.BORDER = properties.bordercolor  and properties.bordercolor:value() or trackheaderview.colors.BORDER
  trackheaderview.colors.FONT = properties.fontcolor and properties.fontcolor:value() or trackheaderview.colors.FONT
  trackheaderview.colors.BG = properties.backgroundcolor  and properties.backgroundcolor:value() or trackheaderview.colors.BG
  self.trackheader:createheaderbg()
end

function trackheaderview:onsize()
  if self.cursor:scrolloffset() + self:visibletracks() > rawpattern:numtracks() then
    self.cursor:setscrolloffset(math.max(0,rawpattern:numtracks() - self:visibletracks()))
  end
end

function trackheaderview:addstatuslistener(listener)
  self.statuslisteners_:addlistener(listener)
  return self
end

return trackheaderview
