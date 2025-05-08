--[[ psycle pianoroll (c) 2017 by psycledelics
File: trackedit.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local midihelper = require("psycle.midi")
local image = require("psycle.ui.image")
local graphics = require("psycle.ui.graphics")
local cfg = require("psycle.config"):new("PatternVisual")
local player = require("psycle.player"):new()

local icondir = cfg:luapath() .. "\\psycle\\ui\\icons\\"
local hexdigits = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'}
local cmdnames = {
  [121] = "twk",
  [123] = "mc m",
  [124] = "tws", 
  [255] = ""
}

local trackedit = {}

trackedit.NOTES = 1
trackedit.DRUMS = 2

function trackedit:new(...)
  local c = {}
  setmetatable(c, self)
  self.__index = self  
  c:init(...)   
  return c
end

function trackedit:init(cursor, view)
  self.cursor_ = cursor
  self.view = view  
  self.hover = { track = 0, icon = 0 }  
  self.trackwidth_ = 97
  self.track_dimension = dimension:new(97, 23);
end

trackedit.colpositions = {
  {x = 1, width = 24, label = "Note"},
  {x = 25, width = 8, label = "Instrument"},
  {x = 33, width = 8, label = "Instrument"},
  {x = 41, width = 8, label = "Machine"},
  {x = 49, width = 8, label = "Machine"},
  {x = 57, width = 8, label = "Command"},
  {x = 65, width = 8, label = "Command"},
  {x = 73, width = 8, label = "Parameter"},
  {x = 81, width = 8, label = "Parameter"}
}

function trackedit:draw(g, track, cursor)
  local offset = point:new(self:headerleft(track, cursor.scrolloffset_), 0)
  g:translate(offset)
  g:setcolor(lighten(self.view.colors.BG, 5))      
  --g:fillrect(self.track_dimension)
  self:drawcursor(g, cursor, track)  
  g:retranslate() 
end

function trackedit:drawcursor(g, cursor, track)
  if cursor:track() == track and cursor:column() >= 0 and cursor:column() <= #self.colpositions then
    g:setcolor(self.view.colors.CURSOR)   
    g:fillrect(rect:new(point:new(self.colpositions[cursor:column()].x, 1),
                                  dimension:new(self.colpositions[cursor:column()].width, 14)))
  end
end

function trackedit:drawbyte(g, val, col, track)
    local hi = math.floor(val / 16)
    local lo = val % 16   
    self:updatefontcolor(g, track, col)  
    g:drawstring(hexdigits[hi + 1], point:new(self.colpositions[col].x + 2, 3))
    self:updatefontcolor(g, track, col + 1)
    g:drawstring(hexdigits[lo + 1], point:new(self.colpositions[col + 1].x + 2, 3))   
end

function trackedit:drawpatternevent(g, event)
  local notename = self:notename(event:note())
  if notename then
   self:updatefontcolor(g, event:track(), 1)
   g:drawstring(notename, point:new(5, 3))
  end
  if event:inst() ~= 255 then
    self:drawbyte(g, event:inst(), 2, event:track())
  end
  if event:mach() ~= 255 then
    self:drawbyte(g, event:mach(), 4, event:track())
  end
  if event:cmd() ~= 0 or event:parameter() ~= 0 then
    self:drawbyte(g, event:cmd(), 6, event:track())
    self:drawbyte(g, event:parameter(), 8, event:track())
  end  
end

function trackedit:updatefontcolor(g, track, col)
  if col == self.view.cursor:column() and track == self.view.cursor.track_ then
    g:setcolor(self.view.colors.FONTCURSOR)
  else
    g:setcolor(self.view.colors.FONT)
  end
end

function trackedit:notename(key)
  result = nil
  if key >=0 and key < 120 then
    result = midihelper.notename(key - 12)
  elseif key > 120 and key <= 124 then  
    result = cmdnames[key]
  end  
  return result
end

function trackedit:headerleft(track, scrolloffset)
  return (track - scrolloffset) * self.trackwidth_
end

function trackedit:hittesttrack(point, scrolloffset)
  return math.floor(point:x() / (self.trackwidth_)) + scrolloffset
end

function trackedit:hittestcol(point)  
  local result = -1
  for i=1, #self.colpositions do 
    local col = self.colpositions[i]
    if point:x() >= col.x and point:x() < col.x + col.width then
      result = i
    end
  end
  return result
end

function trackedit:trackposition(track, scrolloffset)
  return rect:new(point:new(self:headerleft(track, scrolloffset), 0),
         dimension:new(self.trackwidth_, self.view:position():height()))
end

return trackedit
