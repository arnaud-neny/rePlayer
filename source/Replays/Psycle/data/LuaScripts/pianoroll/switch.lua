--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  switch.lua
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
local window = require("psycle.ui.window")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()

local switch = window:new(parent)

switch.colors = {
  FONT = 0xFFFFFFFF,  
  BG = 0xFF333333, 
  COLOR = 0xFF666666  
}

local edge = dimension:new(5, 5) 

function switch:typename()
  return "switch"
end

function switch:new(parent)
  local c = window:new()                  
  setmetatable(c, self)
  self.__index = self  
  c:init(trackdata)
  if parent ~= nil then
    parent:add(c)
  end  
  return c
end

function switch:init()
  self.on = true 
  self:setautosize(true, false)
  
  self:viewdoublebuffered()
  self.switchwidth = 10
  self.headery = 15
  self:settext("Sequence", "All", "Current")
end

function switch:settext(header, on, off)
  self.headerlabel = header
  self.onlabel = on
  self.offlabel = off
  return self
end

function switch:draw(g)
  g:setcolor(switch.colors.BG)
  g:fillrect(rect:new(point:new(), dimension:new(self:position():width(), self:position():height())))  
  local centerx = (self:position():width() - self.switchwidth)
  g:setcolor(switch.colors.FONT)
  local textdim = g:textdimension(self.headerlabel)
  local textcenterx = (self:position():width() - textdim:width()) / 2
  g:drawstring(self.headerlabel, point:new(textcenterx, 0))
  g:drawstring(self.onlabel, point:new(0, 10 + textdim:height() / 2))
  g:drawstring(self.offlabel, point:new(0, (self:position():height() - self.headery)/ 2 + 10 + textdim:height() / 2))
  local dim = dimension:new(self.switchwidth, (self:position():height() - self.headery) / 2)
  g:setcolor(switch.colors.COLOR)
  if self.on == true then 
    g:fillroundrect(rect:new(point:new(centerx, self.headery), dim), edge)
  else
    g:fillroundrect(rect:new(point:new(centerx, (self:position():height() - self.headery)/ 2 + self.headery), dim), edge)
  end
  g:drawroundrect(rect:new(point:new(centerx, self.headery), dimension:new(self.switchwidth, self:position():height() - self.headery - 1)), edge)
end

function switch:onmousedown(ev)
  self.on = not self.on
  self:fls()
  self:onclick(self)
end

function switch:onclick(sender)
end

function switch:oncalcautodimension()
  return dimension:new(50, 20)
end

function switch:setproperties(properties)  
  switch.colors.FONT = properties.fontcolor and properties.fontcolor:value() or switch.colors.FONT 
  switch.colors.BG = properties.backgroundcolor  and properties.backgroundcolor:value() or switch.colors.BG 
  switch.colors.COLOR = properties.color and properties.color:value() or switch.colors.COLOR
 end

return switch