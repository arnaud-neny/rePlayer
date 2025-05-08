--[[ psycle pianoroll (c) 2017 by psycledelics
File: trackheader.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local image = require("psycle.ui.image")
local graphics = require("psycle.ui.graphics")
local cfg = require("psycle.config"):new("PatternVisual")
local player = require("psycle.player"):new()


local icondir = cfg:luapath() .. "\\psycle\\ui\\icons\\"
local hexdigits = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'}

local trackheader = {}

trackheader.NOTES = 1
trackheader.DRUMS = 2

function trackheader:new(...)
  local c = {}
  setmetatable(c, self)
  self.__index = self  
  c:init(...)   
  return c
end

function trackheader:init(view)
  self.view = view
  self.hover = {
    track = 0,
    icon = 0
  }
  self:loadicons()   
  self.fieldwidth_ = 0
  local margin = 3    
  for i = 1, #self.fields do 
    local field = self.fields[i]
    field.left = self.fieldwidth_     
    self.fieldwidth_ = field.width + self.fieldwidth_ + margin 
    margin = 2      
  end    
  self.fieldwidth_ = self.fieldwidth_ + 6
  self:createheaderbg()
end

function trackheader:loadicons()
  self.noteimg = image:new()
                      :load(icondir .. "note.png")
                      :settransparent(0xFFFFFFFF)
  self.drumimg = image:new()
                      :load(icondir .. "drums.png")
                      :settransparent(0xFFFFFFFF)   
end

trackheader.fields = {
  {width = 37, label = nil, active = 0xFF364036, hover = 0xFF364036},           
  {width = 15, label = "S", active = 0xFFA6FF4D, hover = 0xFFA6FF4D},
  {width = 15, label = "M", active = 0xFFFFD24D, hover = 0xFFFFD24D},
  {width = 15, label = "R", active = 0xFFFF4D4D, hover = 0xFFFF4D4D}
}

function trackheader:draw(g, track, cursor)  
  g:translate(point:new(self:headerleft(track, cursor.scrolloffset_), 0))
  if cursor:track() == track then
     g:drawimage(self.headeractivebg, point:new()) 
  else
     g:drawimage(self.headerbg, point:new())
  end
  if track < 64 then
    self:drawtrackfield(g, track, cursor.trackmodes_[track + 1])
    self:drawactivefields(g, track)  
  end
  g:retranslate()
end

function trackheader:setscrolloffset(offset)
  self.scrolloffset_ = offset
end

function trackheader:incscrolloffset()
  self.scrolloffset_ = self.scrolloffset_ + 1
end

function trackheader:decscrolloffset()
  if self.scrolloffset_ > 0 then  
    self.scrolloffset_ = self.scrolloffset_ - 1
  end
end

function trackheader:drawfieldborders(g)
  g:setcolor(self.view.colors.BORDER)
  local edge = dimension:new(6, 6)
  for i = 1, #self.fields do 
    local field = self.fields[i]         
    local fieldrect = rect:new(point:new(field.left + 3, 3), dimension:new(field.width, 15))         
    g:drawroundrect(fieldrect, edge)            
  end
end

function trackheader:drawfieldlabels(g)
  g:setcolor(self.view.colors.FONT)
  for i = 1, #self.fields do 
    local field = self.fields[i]         
    local fieldrect = rect:new(point:new(field.left + 3, 3), dimension:new(field.width, 15))         
    if field.label ~= nil then
      g:drawstring(field.label, point:new(field.left + 7, 5))       
    end             
  end
end

function trackheader:drawactivefields(g, track)
  local activeicons = {
    false,                    
    player:istracksoloed(track),
    player:istrackmuted(track),
    player:istrackarmed(track)
  }      
  activeicons[self.hover.icon] = 
      self.hover.track_ == track and self.hover.icon > 1
  for i, icon in pairs(activeicons) do
    if icon then              
      self:drawfieldactive(g, i)          
    end
  end
end

function trackheader:drawfieldactive(g, field)
  local field = self.fields[field]
  local fieldrect = rect:new(point:new(field.left + 3, 3), dimension:new(field.width -1, 14))     
  g:setcolor(field.active)  
  g:fillroundrect(fieldrect, dimension:new(5, 5))
  if field.label ~= nil then               
    g:setcolor(0xFF222222)        
    g:drawstring(field.label, point:new(field.left + 7, 5))       
  end
end

function trackheader:drawtrackfield(g, track, trackmode)
local field = self.fields[1]
  g:setcolor(0xFFFFFFFF) 
  g:drawstring("" .. track, point:new(field.left + 7, 5)) 
  if trackmode == trackheader.NOTES then
    g:drawimage(self.noteimg, point:new(field.left + field.width - 10, 5))
  else
    g:drawimage(self.drumimg, point:new(field.left + field.width - 10, 5))
  end
  g:setcolor(0xFFFFFFFF) 
end

function trackheader:createheaderbg()
  local w = self.fieldwidth_
  local h = 23
  local position = rect:new(point:new(), dimension:new(w, h))
  self.headerbg = image:new():reset(dimension:new(w, h))
  local g = graphics:new(self.headerbg)
  g:setcolor(self.view.colors.BG, 50)
  g:fillrect(position)
  self:drawfieldborders(g, 0, 100)
  self:drawfieldlabels(g, 0, 100)
  g:dispose()
  self.headeractivebg = image:new():reset(dimension:new(w, h))
  local g = graphics:new(self.headeractivebg)
  g:setcolor(self.view.colors.BG, 50)
  g:fillrect(position)
  g:setcolor(lighten(self.view.colors.BG, 50))
  for i = 1, #self.fields do 
    local field = self.fields[i]             
    local fieldrect = rect:new(point:new(field.left + 3, 3), dimension:new(field.width, 15))         
    g:fillroundrect(fieldrect, dimension:new(5, 5))
  end
  self:drawfieldborders(g, 0, 0)
  self:drawfieldlabels(g, 0, 100)
  g:dispose()
end

function trackheader:headerleft(track, scrolloffset)
  return (track - scrolloffset) * self.fieldwidth_
end

function trackheader:hittesttrack(point, scrolloffset)
  local result = 0
  result = math.floor(point:x() / (self.fieldwidth_)) + scrolloffset
  return result
end

function trackheader:trackwidth()
  return self.fieldwidth_
end

function trackheader:trackposition(track, scrolloffset)
  return rect:new(point:new(self:headerleft(track, scrolloffset), 0),
         dimension:new(self.fieldwidth_, self.view:position():height()))
end

return trackheader
