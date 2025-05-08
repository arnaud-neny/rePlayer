-- psycle separator (c) 2015 by psycledelics
-- File: separator.lua
-- copyright 2016 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local point = require("psycle.ui.point")
local rect = require("psycle.ui.rect")
local dimension = require("psycle.ui.dimension")
local window = require("psycle.ui.window")

local separator = window:new()

separator.HORIZONTAL = 1
separator.VERTICAL = 2

separator.settings = {
  direction = separator.VERTICAL,
  colors = { foreground = 0xFF696969 }  
}

function separator:new(parent)
  local c = window:new()
  setmetatable(c, self)
  self.__index = self
  if parent ~= nil then  
   parent:add(c)   
  end
  c:init()  
  return c
end

function separator:init()
  self:setautosize(true, true)   
  self.direction_ = separator.settings.direction
  self.color_ = separator.settings.colors.foreground  
  self:setdebugtext("separator")
end

function separator:setcolor(color) 
  self.color_ = color
end

function separator:color()
  return self.color_; 
end

function separator:viewhorizontal() 
  self.direction_ = separator.HORIZONTAL 
end

function separator:viewvertical() 
  self.direction_ = separator.VERTICAL
end

function separator:viewdirection()
  return self.direction_
end

function separator:draw(g)  
  g:setcolor(self.color_)  
  if self.direction_ == separator.VERTICAL then
    local centerx = self:dimension():width() / 2    
    g:drawline(point:new(centerx, 0), point:new(centerx, self:dimension():height()))
  elseif direction == separator.HORIZONTAL then
    local centery = dimension():height() / 2
    g:drawline(point:new(0, centery), point:new(self:dimension():width(), centery))
  end
end

function separator:oncalcautodimension()  
  return dimension:new(20, 20)
end

function separator:transparent()  
  return true
end

return separator