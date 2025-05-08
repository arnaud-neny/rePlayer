--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  scroller.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local point = require("psycle.ui.point")
local listeners = require("psycle.listener")

local scroller = {}

function scroller:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init()
  return m
end

function scroller:init()
  self.listeners_ = listeners:new()
  self.dx_, self.dy_ = 0, 0
  self.diffx_, self.diffy_ = 0, 0
  self.hmin_, self.hmax_ = 0, 1  
  self.vmin_, self.vmax_ = 0, 1
  self.midkey_ = 0
  self.inputs_ = { horizontal = {}, vertical = {} }
end

function scroller:setview(view)
  self.view_ = view
  view:addsizelistener(self)
  self:updatehscrollrange()
end
      
function scroller:addlistener(listener)
  self.listeners_:addlistener(listener)
  return self
end

function scroller:addhorizontalinput(input)
  self.inputs_.horizontal[#self.inputs_.horizontal + 1] = input 
  local that = self
  function input:onscroll(input)
    local olddx = that.dx_
    that.dx_ = -input:scrollposition() * that.view_:zoom():rasterwidth()
    for i=1, #that.inputs_.horizontal do
      local input = that.inputs_.horizontal[i]
      if input ~= self then
        that.inputs_.horizontal[i]:setscrollposition(-that.dx_ / that.view_:zoom():rasterwidth())
      end
    end
    that.diffx_ = that.dx_ - olddx    
    that.listeners_:notify(that, "onscroll")  
  end
end

function scroller:addverticalinput(input)
  self.inputs_.vertical[#self.inputs_.vertical + 1] = input 
  local that = self
  function input:onscroll(input)
    local olddy = that.dy_
    that.dy_ = (-input:scrollposition() * that.view_:zoom():height())  
    for i=1, #that.inputs_.vertical do
      local input = that.inputs_.vertical[i]
      if input ~= self then
        that.inputs_.vertical[i]:setscrollposition(-that.dy_ / that.view_:zoom():height())
      end
    end
    that.midkey_ = that:midkey()   
    that.diffy_ = that.dy_ - olddy    
    that.listeners_:notify(that, "onscroll")
  end
end

function scroller:updatehscrollrange()    
  local hmax =  (self.view_:preferredgridwidth() - self.view_:position():width())/self.view_:zoom():rasterwidth() 
  self:sethscrollrange(0, hmax)            
  return self
end

function scroller:updatevscrollrange()
  local visiblekeys = self.view_:position():height() / self.view_:zoom():height()
  self:setvscrollrange(0, self.view_.keyboard.keymap:numkeys() - visiblekeys)          
end

function scroller:sethscrollrange(from, to)
  self.hmin_, self.hmax_ = from, math.ceil(math.max(0, to))
  for i=1, #self.inputs_.horizontal do
    self.inputs_.horizontal[i]:setscrollrange(from, self.hmax_)
  end
  return self
end

function scroller:hrange()
  return self.hmin_, self.hmax_
end
          
function scroller:setvscrollrange(from, to)  
  self.vmin_, self.vmax_ = from, math.ceil(math.max(0, to))
  for i=1, #self.inputs_.vertical do
    self.inputs_.vertical[i]:setscrollrange(from,  self.vmax_)
  end  
  if -self.dy_ >= self.vmax_ * self.view_:zoom():height() then     
    self:setdy(0)
  end
  return self
end

function scroller:vrange()
  return self.vmin_, self.vmax_
end

function scroller:dx()
  return self.dx_
end

function scroller:dy()
  return self.dy_
end

function scroller:setdx(dx)
  self.diffx_ = dx - self.dx_
  self.dx_ = dx
  self.diffy_ = 0
  for i=1, #self.inputs_.horizontal do
    self.inputs_.horizontal[i]:setscrollposition(-dx / self.view_:zoom():width())
  end  
  self.listeners_:notify(self, "onscroll")
end

function scroller:setdy(dy)
  self.diffy_ = dy - self.dy_  
  self.dy_ = dy  
  self.diffx_ = 0
  for i=1, #self.inputs_.vertical do
    self.inputs_.vertical[i]:setscrollposition(-dy / self.view_:zoom():height())
  end
  self.listeners_:notify(self, "onscroll")
end

function scroller:diff()
  return point:new(self.diffx_, self.diffy_)
end
        
function scroller:aspoint()
  return point:new(self.dx_, self.dy_)
end

function scroller:scrollto(dx, dy)
  self.diffx_ = dx - self.dx_
  self.diffy_ = dy - self.dy_
  self.dx_, self.dy_ = dx, dy
  self.listeners_:notify(self, "onscroll")
end

function scroller:scrolltoseqpos(seqpos)
  if self.view_.displaymode == self.view_.DRAWALL then
  local dx = math.min(self.hmax_*self.view_:zoom():rasterwidth(), self.view_:gridpositions():at(seqpos))
  for i=1, #self.inputs_.horizontal do
    self.inputs_.horizontal[i]:setscrollposition(dx / self.view_:zoom():rasterwidth())
  end
  self:scrollto(-dx, self:dy()) 
  else  
    for i=1, #self.inputs_.horizontal do
      self.inputs_.horizontal[i]:setscrollposition(0)
    end
    self:setdx(0)    
  end
  return self
end

function scroller:onsizegridview(sender)
  self:updatehscrollrange(sender)
  self:updatevscrollrange(sender)  
  self.listeners_:prevent()
  self:restoremidkey()
  self.listeners_:enable()
end

function scroller:onpatterneventwidthchanged(sender)
  self:updatehscrollrange(self.view_)
end

function scroller:onpatterneventheightchanged(sender)
  self:updatevscrollrange(self.view_)
end

function scroller:centertokey(key)
  local visiblekeys = self.view_:position():height() / self.view_:zoom():height()
  local center = self.view_.keyboard.keymap.range.to_ - key - math.floor(visiblekeys/2) 
  self:setdy(-math.max(0, math.min(self.view_:zoom():height() * self.vmax_, self.view_:zoom():height() * center)))
end

function scroller:midkey()
  return self.view_.keyboard.keymap.range.to_ - (0.5*self.view_:position():height() - self:dy())/self.view_:zoom():height() 
end

function scroller:restoremidkey()
   self.midkey_ = self.midkey_ ~= 0 and self.midkey_ or self:midkey()
   self:centertokey(self.midkey_)
end

function scroller:visiblekeys()
  return self.view_:visiblenotes()
end

function scroller:onwheel(ev)  
  if ev:wheeldelta() <= 0 then
    local pos = math.max(-self.vmax_ * self.view_:zoom():height(),
                         self:dy() - 4*self.view_:zoom():height())
    self:setdy(pos)    
  else
    local pos = math.min(0, self:dy() + 4*self.view_:zoom():height())
    self:setdy(pos)
  end
  self.midkey_ = self:midkey()
end
 
return scroller
