--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  rulergrid.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local pattern = require("pattern")
local player = require("psycle.player")
local hitarea = require("hitarea")
local midihelper = require("psycle.midi")
local rastergrid = require("rastergrid")

local rulergrid = rastergrid:new()

local cursordimension = dimension:new(5,5)

function rulergrid:new(...)
  local m = rastergrid:new(...)
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function rulergrid:init()   
end

function rulergrid:draw(g, screenrange, pattern, seqpos)   
   self:drawcursorbar(g, seqpos) 
end

function rulergrid:drawevents()   
end

function rulergrid:drawdragging()                                       
end

function rulergrid:hittest(position, gridpos, pattern, seqpos)
  local result = { hitarea = hitarea.NONE, grid = self }  
  return result
end

function rulergrid:hittestrect(rect)
  local result = {} 
  return result
end

function rulergrid:drawbarendings(g, screenrange) 
  for i=0, screenrange:right(), self.beatsperbar do     
    local x = i*self.view:zoom():width() 
    g:setcolor(self.view.colors.line4beatcolor)
    g:drawline(point:new(x, 0), point:new(x, 20))    
    g:setcolor(self.view.colors.fontcolor)
    local startbeat = self.view.sequence:startbeat(screenrange.seqpos)   
    g:drawstring((startbeat + i), point:new(x + 3, 0))
    if i == 0 then
      local seqhex = string.format("%02X", (screenrange.seqpos - 1).."")
      local pathex = string.format("%02X", self.view.sequence:patternindex(screenrange.seqpos).."")
      g:drawstring(i .. " [" .. seqhex .. ": " .. pathex .. "]", point:new(x + 3, 10)) 
    else
      g:drawstring((i), point:new(x + 3, 10))
    end    
  end
end

function rulergrid:bardimension()
   return dimension:new(math.floor(self.beatsperbar * self.view:zoom():width()), self:preferredheight())
end

function rulergrid:rendercursorbar(g, pos)
  g:fillrect(rect:new(point:new(self.view:zoom():beatwidth(pos) - 2, 0), cursordimension))          
end

function rulergrid:preferredheight()
  return 30
end

return rulergrid
