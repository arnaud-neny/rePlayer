-- zoom.lua

local listener = require("psycle.listener")
local player = require("psycle.player"):new()

local zoom = {}

function zoom:new()  
  local m = {} 
  setmetatable(m, self)
  self.__index = self
  m:init()
  return m
end

function zoom:init()
  self.listeners_ = listener:new()
  self.owidth_ = 120
  self.oheight_ = 12
  self.width_ = 120
  self.height_ = 12
  self.vzoom_ = 10
  self.hzoom_ = 10
  self.gain_ = 0.1
end
    
function zoom:addlistener(listener)       
  self.listeners_:addlistener(listener)
  return self
end

function zoom:width()
  return self.width_
end
     
function zoom:rasterwidth()
  return self.width_ / player:tpb()
end

function zoom:height()
  return self.height_
end

function zoom:inchzoom()
  self.hzoom_ = math.min(self.hzoom_ + 1, 20)
  self.width_ = math.floor(self.owidth_ * self.hzoom_*self.gain_)
  self.listeners_:notify(self, "onpatterneventwidthchanged")
end

function zoom:dechzoom()
  self.hzoom_ = math.max(self.hzoom_ - 1, 1)
  self.width_ = math.floor(self.owidth_ * self.hzoom_*self.gain_)
  self.listeners_:notify(self, "onpatterneventwidthchanged")
end
    
function zoom:fithzoom(numlines, width)
  local rw = width / numlines
  local w = rw * player:tpb()
  self.hzoom_ = w / (self.owidth_ *self.gain_)
  self.width_ = math.floor(self.owidth_ * self.hzoom_*self.gain_)
  self.listeners_:notify(self, "onpatterneventwidthchanged")
end

function zoom:incvzoom()
  self.vzoom_ = self.vzoom_ + 1
  self.height_ = math.floor(self.oheight_ * self.vzoom_*self.gain_)
  self.listeners_:notify(self, "onpatterneventheightchanged")
end

function zoom:decvzoom()
  self.vzoom_ = self.vzoom_  - 1
  self.height_ = math.floor(self.oheight_ * self.vzoom_*self.gain_)
  self.listeners_:notify(self, "onpatterneventheightchanged")
end

function zoom:fitvzoom(numkeys, height)
  local h = height / numkeys
  self.vzoom_ = h / (self.oheight_ * self.gain_)
  self.height_ = self.oheight_ * self.vzoom_ * self.gain_
  self.listeners_:notify(self, "onpatterneventheightchanged")
end
     
function zoom:beatwidth(beat)
  return beat * self.width_
end

function zoom:beat(screenpos)
  return math.max(0, screenpos / self.width_)
end

return zoom
