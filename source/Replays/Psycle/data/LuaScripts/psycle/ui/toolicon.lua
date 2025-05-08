-- psycle toolicon (c) 2015 by psycledelics
-- File: toolicon.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local image = require("psycle.ui.image")
local window = require("psycle.ui.window")
local listener = require("psycle.listener")
local serpent = require("psycle.serpent")
local signal = require("psycle.signal")

function weakref(data)
    local weak = setmetatable({content=data}, {__mode="v"})
    return function() return weak.content end
end

local toolicon = window:new()

toolicon.NONE = 0
toolicon.CENTER = 7
toolicon.TOP = 2
toolicon.LEFT = 3
toolicon.RIGHT = 4
toolicon.BOTTOM = 5
toolicon.LEFTJUSTIFY = 1
toolicon.RIGHTJUSTIFY = 2
toolicon.CENTERJUSTIFY = 3

function toolicon:new(parent, filename, trans)  
  local c = window:new()  
  setmetatable(c, self)
  self.__index = self
  c:init(filename, trans)
  if parent ~= nil then  
   parent:add(c)   
  end
  return c
end

function toolicon:typename()
  return "toolicon"
end

function toolicon:onclick()
end

function toolicon:setcommand(command)
  self.command_ = weakref(command)
end

function toolicon:init(filename, trans)  
  self:setautosize(true, true)  
  self:initdefaultcolors()  
  self.verticalalignment_ = toolicon.CENTER
  self.justify_ = toolicon.NONE  
  self.on_ = false  
  self.text_ = ""
  if filename then
    self.img = image:new():load(filename)
    if trans ~=nil then
      self.img:settransparent(trans)
    end    
  end  
  self.clicklistener_ = listener:new("onclick", true)
  self.click = signal:new()
  self.istoggle_ = false
  self:setposition(rect:new(point:new(0, 0), dimension:new(20, 20)))
  self.hover = false
  self.usetoggleimage_ = false
  self.is_toggle = false 
  self.groupindex = -1
end

function toolicon:setgroupindex(index)
  self.groupindex = index
  return self
end

function toolicon:initdefaultcolors()
  self.color = 0xFFCACACA    
  self.backgroundcolor = 0xFF232323  
  self.activecolor = 0xFFCACACA
  self.activebackgroundcolor = 0xFF568857
  self.hovercolor = 0xFFCACACA
  self.hoverbackgroundcolor = 0xFF414135
  return self
end

function toolicon:setjustify(justify)
  self.justify_ = justify
  return self
end

function toolicon:setverticalalignment(alignment)
  self.verticalalignment_ = alignment
  return self
end

function toolicon:justifyoffset()
  local result = 0
  if self.img then
    if self.justify_ == toolicon.CENTERJUSTIFY then
      result = (self:dimension():width() - self.img:dimension():width())/2
     elseif self.justify_ == toolicon.RIGHTJUSTIFY then
       result = self:dimesion():width() - self.img:dimension():width()
     end
  end
  return result
end

function toolicon:verticalalignmentoffset()
  local result = 0
  if self.img then
    if self.verticalalignment_ == toolicon.CENTER then	  		
      result = (self:dimension():height() - self.img:dimension():height())/2
    elseif self.verticalalignment_ == toolicon.BOTTOM then 				
	  result = self:dimension():height() - self.img:dimension():height()
    end	
  end
  return result
end

function toolicon:enabletoggle()
  self.is_toggle = true
  return self
end

function toolicon:preventtoggle()
  self.is_toggle = false
  return self
end

function toolicon:settoggleimage(image)
  self.toggleimage_ = image
  self.oldimage = nil  
end

function toolicon:draw(g)    
  self:updatebackgroundcolor(g)
  g:fillrect(rect:new(point:new(), self:dimension()))
  local xpos = 0
  if self.img then
    g:drawimage(self.img, point:new(self:justifyoffset(), self:verticalalignmentoffset())) 
    xpos = xpos + self.img:dimension():width()
  end    
  self:updatecolor(g)    
  local ypos =
      (self:dimension():height() - g:textdimension(self.text_):height())/2
  if self.justify_ == toolicon.CENTERJUSTIFY then
    xpos = (self:dimension():width() - g:textdimension(self.text_):width()) / 2 - 2   
  end  
  g:drawstring(self.text_, point:new(xpos + 2, ypos))
end

function toolicon:updatecolor(g)
  if self.on_ then
    if self.hover then
      g:setcolor(self.activecolor)
    else
      g:setcolor(self.color)
    end
  else
    if self.hover then
      g:setcolor(self.hovercolor)
    else
      g:setcolor(self.color)
    end
  end
end

function toolicon:updatebackgroundcolor(g)
  if self.on_ then
   -- if self.hover then
      g:setcolor(self.activebackgroundcolor)
    --else
    --  g:setcolor(self.backgroundcolor)
   -- end
  else
    if self.hover then
      g:setcolor(self.hoverbackgroundcolor)
    else     
      g:setcolor(self.backgroundcolor)
    end
  end
end

function toolicon:onmousedown(ev) 
  self:capturemouse()
  self:seton(not self.on_)
  self:swapimage():fls()
  self.clicklistener_:notify(self)  
end

function toolicon:onmouseenter(ev)  
  self.hover = true
  if (not self.on_) then
    self:fls()
  end
end

function toolicon:onmousemove(ev)  
  local oldhover = self.hover
  self.hover = self:absoluteposition():intersect(ev:clientpos())            
  if self.hover ~= oldhover then    
    self:fls()
  end
end

function toolicon:onmouseout(ev)   
  self.hover = false
  if (not self.on_) then     
    self:fls()    
  end  
end

function toolicon:onmouseup(ev)  
  self:releasemouse()
  if not self.is_toggle then
    self:seton(false)
    if not self.usetoggleimage_ then
      self:swapimage():fls()
    end
  end
  if (self.hover) then
    self:onclick()       
    if self.command_  and self.command_ ~= nil then
      self.command_():execute()
    end
    self.click:emit(self)        
  end  
end

function toolicon:toggle()
  self:seton(not self.on_)
end

function toolicon:seton(on)  
  if self.on_ ~= on then
    self.on_ = on       
    if self.toolbar~=nil and on and self.groupindex == self.toolbar.groupindex then
      self.toolbar:onnotify(self)      
    end
    self:fls()    
  end
  return self
end

function toolicon:swapimage()
  if (self.toggleimage_) then
    if self.oldimage then
      self.img = self.oldimage
      self.oldimage = nil
    else
      self.oldimage = self.img
      self.img = self.toggleimage_
    end
  end
  return self
end

function toolicon:on()
  return self.on_
end

function toolicon:settoolbar(toolbar)
  self.toolbar = toolbar
  return self
end

function toolicon:addlistener(listener)    
  self.clicklistener_:addlistener(listener)  
  return self
end

function toolicon:settext(text)  
  self.text_ = text
  self:fls()
  return self
end

function toolicon:text()
  return self.text_
end

function toolicon:oncalcautodimension()
  return dimension:new(20, 17)
end

function toolicon:transparent()  
  return false
end

function toolicon:resethover()
  self.hover = false
  self:fls()
end

function toolicon:setproperties(properties)  
  local setters = {"color", "backgroundcolor", "activecolor",
                   "activebackgroundcolor", "hovercolor",
                   "hoverbackgroundcolor"}                       
  for _, setter in pairs(setters) do        
    local property = properties[setter]      
    if property then    
      self[setter] = property:value()     
    end
  end
  self:fls()
end

return toolicon