--[[ psycle pianoroll (c) 2017 by psycledelics
File: controlheaderview.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local group = require("psycle.ui.group")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local player = require("psycle.player"):new()
local patternevent = require("patternevent")
local keycodes = require("psycle.ui.keycodes")
local cmddef = require("psycle.ui.cmddef")
local machinebar = require("psycle.machinebar"):new()
local trackheader = require("trackheader")
local rawpattern = require("psycle.pattern"):new()
local toolicon = require("psycle.ui.toolicon")

local controlheaderview = group:new(parent)

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

controlheaderview.colors = {
  FONT = 0xFFFFFFFF, 
  BG = 0xFF999999
}

function controlheaderview:typename()
  return "trackheader"
end

function controlheaderview:new(parent, ...)
  local c = group:new()                  
  setmetatable(c, self)
  self.__index = self  
  c:init(...)
  if parent ~= nil then
    parent:add(c)
  end  
  return c
end

function controlheaderview:init(cursor, statustext) 
  self:setposition(rect:new(point:new(), dimension:new(0, 10)))
  self:setautosize(true, false)
  self.cursor = cursor
  self.trackheader = trackheader:new(self)  
  self.cursor = cursor
  self.status = statustext
  self:viewdoublebuffered()
  self.player_ = player  
  self.activeevents_ = {}  
  self:initicons()
end

function controlheaderview:initicons()
  toolicon:new(self):settext("X"):setalign(alignstyle.TOP)
  toolicon:new(self):settext("+"):setalign(alignstyle.BOTTOM)
end

function controlheaderview:transparent()
  return false
end

function controlheaderview:draw(g)
  self:clearbg(g)
 -- local from, to = self.cursor.scrolloffset_, math.min(self.cursor.scrolloffset_ + self:visibletracks(), rawpattern:numtracks()-1)
  --for track = from, to do       
 --   self.trackheader:draw(g, track, self.cursor)      
 -- end  
end

function controlheaderview:clearbg(g)   
  g:setcolor(self.colors.BG)
  g:fillrect(self:position())
end

function controlheaderview:visibletracks()
  return math.floor(self:position():width() / self.trackheader.fieldwidth_)
end

function controlheaderview:updatecursorposition()  
end

function controlheaderview:setproperties(properties)   
  if properties.backgroundcolor then
    self.backgroundcolor_ = properties.backgroundcolor:value()   
  end
end

function controlheaderview:onplaybarupdate(playtimer)
  --self.activeevents_ = playtimer.activeevents_
 -- self.repaintmode = controlheaderview.DRAWPATTERNEVENTS
  --self:fls()
 -- self.repaintmode = controlheaderview.DRAWALL
end

function controlheaderview:oncalcautodimension()
  return dimension:new(200, 200)
end

function controlheaderview:onmousedown(ev)
  local x = ev:clientpos():x() - self:absoluteposition():left() 
  local y = ev:clientpos():y() - self:absoluteposition():top()
  local track = self.trackheader:hittesttrack(point:new(x, y), self.cursor.scrolloffset_)
  local trackchanged = self.cursor:track() ~= track
  self.cursor:settrack(track)
  local icon = self:hittesticon(point:new(x, y))
  self:toggleicon(icon, track) 
  if trackchanged then
    self.cursor:settrack(track)
    self.patternview:fls()
  end
  self:fls()
end

function controlheaderview:onmousemove(ev)
  local x = ev:clientpos():x() - self:absoluteposition():left()
  local y = ev:clientpos():y() - self:absoluteposition():top()
  local track = self.trackheader:hittesttrack(point:new(x, y), self.cursor.scrolloffset_)
  local icon = self:hittesticon(point:new(x, y))
  local oldicon = self.trackheader.hover.icon
  self.trackheader.hover.track_ = track
  self.trackheader.hover.icon = icon
  if oldicon ~= icon then
    if icon > 0 and icon < 5 then
      self:setstatus(status[icon])
    end
    self:fls()
  end
end

function controlheaderview:onmouseout(ev)
  if self.trackheader.hover.icon ~= 0 then
    self.trackheader.hover.icon = 0
    self:fls()
  end
end

function controlheaderview:mixdigit(input, val)
  local result = 0
  local iseven = self.cursor:column() % 2 == 0
  if iseven then
    result = input*16 + val % 16
  else
    result = math.floor(val/16)*16 + input
  end
  return result
end

function controlheaderview:handlekeyinput(ev, patternview)
  
end

function controlheaderview:hittesticon(point)
  local track = self.trackheader:hittesttrack(point, self.cursor.scrolloffset_)
  local result = -1  
  for i, field in pairs(self.trackheader.fields) do     
    local fieldrect = rect:new(point:new(self.trackheader:headerleft(track, self.cursor.scrolloffset_) + field.left + 3, 3),
      dimension:new(field.width, 15))
    if fieldrect:intersect(point) then
      result = i
      break
    end
  end
  return result
end

function controlheaderview:toggleicon(icon, track)
  if icon ~= -1 then
    if icon == 1 then
      self.cursor.trackmodes_[track + 1] = self.cursor.trackmodes_[track + 1] == trackheader.NOTES
                                          and trackheader.DRUMS or trackheader.NOTES       
      self.patternview:fls()
    elseif icon == 2 then
      player:toggletracksoloed(track)
    elseif icon == 3 then
      player:toggletrackmuted(track)
    elseif icon == 4 then
      player:toggletrackarmed(track)
    end
  end
end

function controlheaderview:playposition()
  local sequence = player:playpattern()
  return sequence, self.patternview.grids[sequence + 1].pattern:linetobeat(player:line())
end

function controlheaderview:setstatus(text)
  self.status:settext(text)
  self.status:fls()
end

function controlheaderview:onmouseenter()
  self:setstatus("")
end

function controlheaderview:oncalcautodimension()
  return dimension:new(200, 23)
end

function controlheaderview:ontrackchanged()
  self:fls()
end

function controlheaderview:setproperties(properties)  
  controlheaderview.colors.FONT = properties.fontcolor and properties.fontcolor:value() or controlheaderview.colors.FONT
  controlheaderview.colors.BG = properties.backgroundcolor  and properties.backgroundcolor:value() or controlheaderview.colors.BG
  self.trackheader:createheaderbg()
end

return controlheaderview
