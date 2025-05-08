-- psycle arp
-- file : psycle/arp.lua
-- copyright 2014 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.
--
-- arp like the aguru synth has

local player = require("psycle.player"):new()

local arp = {}

local notes = {
	{0,  3,  7, 12, 15, 19, 24, 27, 31, 36, 39, 43, 48, 51, 55, 60},
	{0,  4,  7, 12, 16, 19, 24, 28, 31, 36, 40, 43, 48, 52, 55, 60},
	{0,  3,  7, 10, 12, 15, 19, 22, 24, 27, 31, 34, 36, 39, 43, 46},
	{0,  4,  7, 10, 12, 16, 19, 22, 24, 28, 31, 34, 36, 40, 43, 46},
	{0, 12,  0,-12, 12,  0, 12,  0,  0, 12,-12,  0, 12,  0, 12,-12},
	{0, 12, 24,  0, 12, 24, 12,  0, 24, 12,  0,  0, 12,  0,  0, 24},
	{0, 12, 19,  0,  0,  7,  0,  7,  0, 12, 19,  0, 12, 19,  0,-12},
	{0,  3,  7, 12, 15, 19, 24, 27, 31, 27, 24, 19, 15, 12,  7,  3},
	{0,  4,  7, 12, 16, 19, 24, 28, 31, 28, 24, 19, 16, 12,  7,  4},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}
};

function arp:new(voice)
  local a = {}      
  setmetatable(a, self)
  self.__index = self
  a.voice = voice
  a.speed = 125
  a.steps = 4 
  a.sc = 0 -- sample count
  a.nc = 1 -- note count (1..16)
  a.next = 0 -- nextarp
  a.notes = notes  
  a.playing = false
  a.mode = 0
  a.reset = false
  a.vel = 127
  a:setspeed(125)
  return a
end

function arp:isplaying()
   -- first if for compatibility issues only
   if (self.voice.ahdsr ~= nil) then
     return self.voice.ahdsr:isplaying() or self.playing
   else
     return self.voice:isplaying() or self.playing 
   end
end

function arp:noteon(basenote)  
  self.sc = 0 -- sample count
  self.nc = 1 -- note count (1..16)
  self.next = 0 -- nextarp
  self.reset = true
  self.playing = self.mode ~= 0 
  self.basenote = basenote
  self.voice:noteon(basenote)
end

function arp:noteoff()
  self.playing = false
  self.voice:noteoff()
end

function arp:faststop()
  self.playing = false
  self.voice:faststop()  
end

function arp:setspeed(bpm)
  self.speed = bpm
  local sr = player:samplerate()
  self.period = (sr * 60)/(self.speed*4) - (0.03*sr);
end

function arp:setmode(mode)
  self.mode = mode
end

function arp:setsteps(steps)
  self.steps = steps
end

function arp:work(channels)
  local sr = player:samplerate()
  if self.mode == 0 or not self.playing then
    self.voice:work(channels)
  else
    local num = channels[1]:size()
	local k = 0
    local i = math.floor(self.next) - self.sc
    while (i < num) do
	  if (i-k) > 0 then	    
	    for j=1, #channels do channels[j]:margin(k, i) end
	    self.voice:work(channels)	     
	    k = i
	  end
      if self.reset then
		self.voice:noteon(self.basenote+self.notes[self.mode][self.nc])
	    self.nc = self.nc + 1
	    if self.nc > self.steps then
	      self.nc = 1
	    end
	    self.next = self.next + self.period
		self.reset = false
	  else
             self.voice:faststop()
		self.reset = true
		self.next = self.next + (sr*0.03)  -- 0.03 sec faststop
	  end
      i = math.floor(self.next) - self.sc 	  
	end
	for j=1, #channels do channels[j]:margin(k, num) end
	self.voice:work(channels)
	for j=1, #channels do channels[j]:clearmargin() end
	self.sc = self.sc + num
  end
end

return arp