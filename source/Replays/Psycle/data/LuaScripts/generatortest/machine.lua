--  psycle lua synthdemo
--  demo module that demonstrates how to build a basic synth
--  file : machine.lua


-- require('mobdebug').start()

machine = require("psycle.machine"):new()

local array = require("psycle.array")
local dspmath = require("psycle.dsp.math")

local arps = {}
local channels = {}
local notes = {}
-- require
local voice = require("generatortest.voice")
local arp = require("psycle.arp")
local param = require("psycle.parameter")
  
-- plugin info
-- moved to register file /LuaScripts/generatortest.lua

-- help text displayed by the host
function machine:help()
  return "01xx : slide up\n"..
            "02xx : slide down\n"..
            "04xy : vibrato(frq,gain)\n"..
            "0Cxx : volumne\n"..
            "C3xx : partomento"
end

local filtertypes = {"LowPass", "HighPass", "BandPass", "BandReject", "None",
                             "ITLOWPASS", "MPTLP", "MPTHigh", "LP12E",
                             "HP12E", "BP12E", "BR12"}

-- plugin constructor

function machine:init(samplerate)   
  voice.samplerate = samplerate        
  filter = require("psycle.dsp.filter")
  filtercurr = filter:new(filter.LOWPASS)
  
  local f1 = filter:new(filter.LOWPASS)
  f1 = nil
  collectgarbage()
  
  -- setup voice independent parameters
  p = require("orderedtable"):new()
  p.mlb = param:newlabel("Master")
  p.vol = param:newknob("vol", "", 0, 1, 100, 0.5)
  p.flb = param:newlabel("Filter")
  p.ft = param:newknob("FilterType","",0, 11, 11, 0):addlistener(self)
  function p.ft:display()    
    return filtertypes[self:val()+1]
  end
  p.fc = param:newknob("VCF CutOff","",0,127,127,127):addlistener(self)
  p.fr = param:newknob("VCF Resonance","",0,127,127,0):addlistener(self)
  -- set parameters to the host
  self:addparameters(p)  
  self:addparameters(voice.params) 
  p = require("orderedtable"):new()  
  -- arp params
  p.arplb = param:newlabel("Arp")
  p.arpmode = param:newknob("Arp Mode", "", 0, 16, 16, 0):addlistener(self)
  p.arpspeed = param:newknob("Arp Speed", "bpm", 1, 1000, 999, 125):addlistener(self)
  p.arpsteps = param:newknob("Arp Steps", "", 1, 16, 15, 4):addlistener(self)  
  self:addparameters(p)  
  self:setnumcols(2)  
  -- create 3 polyphonic voices
  self.currvoice = 1 
  for i=1, 3 do arps[#arps+1] = arp:new(voice:new()) end  
  for i=0, 64 do channels[i] = 0 end  
  for i=0, 119 do notes[i] = nil end  
end

-- fill the audio buffer
function machine:work()     
  local numinst = #arps
  for i = 1, numinst do
    arps[i]:work({self:channel(0)})
  end  
  filtercurr:work(self:channel(0):mul(p.vol:val()))        
  self:channel(1):copy(self:channel(0))  
end

function machine:freevoice()   
  local v = arps[self.currvoice];
  local c = 0  
  while v:isplaying() do
    if self.currvoice == #arps then
      self.currvoice = 1
    else
      self.currvoice = self.currvoice + 1
    end
	c = c + 1
	if c > #arps then	   
	   local newarp = arp:new(voice:new())
	   arps[#arps+1] = newarp
	   newarp.mode = arps[1].mode
	   newarp.steps = arps[1].steps
	   newarp:setspeed(arps[1].speed)
       self.currvoice = #arps
	   break
	end
	v = arps[self.currvoice]
  end    
  return self.currvoice
end

function machine:seqtick(channel, note, ins, cmd, val)
  local curr = arps[channels[channel]]
  if (note < 119) then 
    if cmd == 195 and curr~=nil and curr:isplaying() then  -- portamento         
	  curr.voice:glideto(note, 0.2)
    else	
      if curr~=nil and (curr:isplaying()) then                  
	  curr:faststop()
	  end	    
	  channels[channel] = self:freevoice()	
	  curr = arps[self.currvoice];	
      curr:noteon(note)
    end   
  elseif curr~=nil and note==120 then
     arps[channels[channel]]:noteoff()	 
     channels[channel]=0
  end
  if curr~=nil then    
    curr.voice:initcmd(note, cmd, val);
  end
end

-- alternative to seqtick, if noteon=1 is added to info.
function machine:command(lastnote, inst, cmd, val)
  if lastnote then
     notes[lastnote].voice:initcmd(lastnote, cmd, val);
  end
end

function machine:noteon(note, lastnote, inst, cmd, val)
  -- search free voice  
  if lastnote and cmd == 195 then  -- porta    
	notes[lastnote].voice:glideto(note, 0.2)
	notes[note] = notes[lastnote];
  else
	notes[note] = arps[self:freevoice()]
    notes[note]:noteon(note)
	self:command(note, inst, cmd, val);
  end
end

function machine:noteoff(note, lastnote, inst, cmd, val)
  if (cmd == 195) then return end
  if lastnote then
    notes[note]:faststop()
  else
     notes[note]:noteoff()
  end  
end
-- end of alternative code

function machine:stop()    
  local num = #arps
  for i = 1, num do arps[i]:faststop() end
  for i=0, 64 do channels[i] = 0 end
  self.currvoice = 1
  for i = 0, 119 do notes[i] = nil end    
end

function machine:ontweaked(param)
  if param==p.ft then     
  elseif param==p.fc then
     filtercurr:setcutoff(param:val())
  elseif param==p.fr then
     filtercurr:setresonance(param:val())
  elseif param==p.arpmode then	
    for i=1, #arps do arps[i]:setmode(param:val()) end
  elseif param==p.arpspeed then
    for i=1, #arps do arps[i]:setspeed(param:val()) end
  elseif param==p.arpsteps then
    for i=1, #arps do arps[i]:setsteps(param:val()) end
  end	 
end

function machine:onaftertweaked()
end

return machine