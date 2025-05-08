-- includes
local param = require("psycle.parameter")
local env = require("psycle.envelope")
local osc = require("psycle.osc");
local dspmath = require("psycle.dsp.math")
local array = require("psycle.array")

local voice = {}

local osctypes = {"sin", "saw", "square", "tri"}
local osctypesoff = {"sin", "saw", "square", "tri", "off"}

-- setup voice parameters
local p = require("orderedtable").new()
-- osc params
p.osclb = param:newlabel("Oscs")
for i=1, 3 do
  p["osc"..i] = param:newknob("oscshape"..i, "", 1, 5, 4, 1)
  p["osc"..i].display = function(self) return osctypesoff[self:val()] end
  p["oscsemi"..i] = param:newknob("oscsemi"..i,"",-60,60,120,0)
  p["osccent"..i] = param:newknob("osccent"..i,"",-100,100,200,0)  
end
p.sync = param:newknob("Sync Osc1","",0,2000,2000,0)  
-- vca params
p.vcalb = param:newlabel("Vol Env")
p.vcaa = param:newknob("A", "s", 0.01, 5.01, 500, 1)
p.vcah = param:newknob("H", "s", 0.01, 5.01, 500, 1)
p.vcad = param:newknob("D", "s", 0.01, 5.01, 500, 1)
p.vcas = param:newknob("S", "%", 0, 100, 100, 50)
p.vcar = param:newknob("R", "s", 0.01, 5.01, 500, 1)
--frequency lfo params
p.flfolb = param:newlabel("Lfo Freq")
p.flfof = param:newknob("LFO Freq", "Hz", 0, 20, 200, 5)
p.flfog = param:newknob("LFO Gain", "", 0, 100, 100, 10)
p.flfos = param:newknob("LFO Shape", "", 1, 3, 2, 1)
function p.flfos:display() return osctypes[self:val()] end
-- pitch modulation params
p.pmlb = param:newlabel("Pitch Mod")
p.pm1t = param:newknob("T1", "s", 0.01, 5.01, 500, 1)
p.pm2t = param:newknob("T2", "s", 0.01, 5.01, 500, 1)
p.pm3t = param:newknob("T3", "s", 0.01, 5.01, 500, 1)
p.pm4t = param:newknob("T4", "s", 0.01, 5.01, 500, 1)
p.pm1p = param:newknob("P1", "Hz", -1, 1, 100, 1)
p.pm2p = param:newknob("P2", "Hz", -1, 1, 100, -0.2)
p.pm3p = param:newknob("P3", "Hz", -1, 1, 100, 0.5)
p.pm4p = param:newknob("P4", "Hz", -1, 1, 100, 0)
p.pmg = param:newknob("Gain", "", 0, 100, 100, 0)
-- resampler params
p.resamplerlb = param:newlabel("Resampler")
p.resampler = param:newknob("Resampler", "", 1, 3, 2, 2)
local resamplertypes = {"zerohold", "linear", "sinc"}
function p.resampler:display() return resamplertypes[self:val()] end
voice.params = p  -- used in machine::init

function voice:new()
  local v = {}      
  setmetatable(v, self)
  self.__index = self  
  ahdsr = require("psycle.ahdsr");
  v.ahdsr = ahdsr:new(p.vcaa:val(), p.vcah:val(), p.vcad:val(), p.vcas:norm(),
	  			    p.vcar:val(), ahdsr.LIN)
  v.envf = env:new({{p.pm1t:val(), p.pm1p:val()},
                    {p.pm2t:val(), p.pm2p:val()},
					{p.pm3t:val(), p.pm3p:val()},
					{p.pm4t:val(), p.pm4p:val()}},
					4)
  v.envglide = nil
  v.oscvol = 1
  v.lfo = osc:new(p.flfos:val(), p.flfof:val()) 
  v.lfo:setgain(p.flfog:val())  
  v.vibrato = osc:new(osc.SIN, 4)
  v.oscs = {}  
  v.basenote = 60
  v.master = osc:new(osc.SQR, 263.1)
  for i=1, 3 do   
    local f = dspmath.notetofreq(v.basenote + p["oscsemi"..i]:val()+p["osccent"..i]:val()/100)
    v.oscs[i] = osc:new(p["osc"..i]:val(), f)
	v.oscs[i]:setgain(0.3)
	v.oscs[i].active = true
  end  
  for i,k in p:opairs() do
    k:addlistener(v)
  end
  v.isplaying = false  
  return v
end

function voice:noteon(note)
  self.basenote = note
  for i=1, 3 do   
    local f = dspmath.notetofreq(note + p["oscsemi"..i]:val()+p["osccent"..i]:val()/100)
	if (self.oscs[i].sync == nil) then
      self.oscs[i]:setfrequency(f)
	else
	  self.master:setfrequency(f)
	end
	self.oscs[i]:start(0)
  end
  self.master:start(0)
  self.ahdsr:start()
  self.envf:start()
  self.lfo:start(0)  
end

function voice:noteoff()
  self.ahdsr:release()
  self.envf:release()
  self.lastnote = basenote
end

function voice:faststop()
  self.ahdsr:fastrelease()
end

function voice:initcmd(note, cmd, val)
   if cmd == 1 then          	
	 self:glideto(self.basenote+val/2, 0.5)
   elseif cmd ==2 then
     self:glideto(self.basenote-val/2, 0.5)
   elseif cmd == 3 then
     self:glide(val)
   elseif cmd == 4 then     
     if val == 0 then
       self.vibrato:stop(0)  
	 else	   
	   local y = val % 16
       local x = math.floor(val/16)
       self.vibrato:setfrequency(y)
	   self.vibrato:setgain(x)
       self.vibrato:start(0)   
	 end
   elseif cmd == 11 then
     local y = val % 16
     local x = math.floor(val/16)
     p.arpmode:setval(y)
	 p.arpsteps:setval(x)
	 self.arp.playing = p.arpmode:val() ~= 0
   elseif cmd == 12 then   
     self.oscvol = val/127
   end
end

function voice:work(samplelist)  
  local samples = samplelist[1]
  if self.ahdsr:isplaying() then
    local num = samples:size() 
    -- process pitch envelope
    local fm = self.envf:work(num)
    fm:mul(p.pmg:val())  
    -- process pitch lfo 
    self.lfo:work(fm)
    -- process glide
    if self.envglide~= nil then
      fm:add(self.envglide:work(num))
    end
    -- process vibrato
    if self.vibrato:isplaying() then    
      self.vibrato:work(fm)
    end  
	-- process ahdsr  
    e = self.ahdsr:work(num) 
    for i=1, #self.oscs do  
      if (self.oscs[i].active == true) then
	    self.oscs[i]:setam(e):setfm(fm):work(samples)        
	  end
    end
    -- check glide end
    if self.envglide~= nil and not self.envglide:isplaying() then
      for i=1, #self.oscs do  	  
	    local f = dspmath.notetofreq(self.glidenote + p["oscsemi"..i]:val()+p["osccent"..i]:val()/100)
        self.oscs[i]:setfrequency(f)	  
	  end
	  self.basenote = self.glidenote;
	  self.envglide = nil
    end 
    samples:mul(self.oscvol)
  end
end

function voice:glideto(note, t)
  local f = dspmath.notetofreq(note);
  local currf = f - self.oscs[1]:frequency()  
  self.envglide = env:new({{t, currf}}, 2);  
  self.envglide:start()
  self.glidenote = note
end

function voice:glide(val)
   self.envglide = env:new({{0.5, val}}, 2);
   self.envglide:start()
   self.glidenote = self.basenote
end

function voice:ontweaked(param)
  if param==p["osc1"] then
    if (param:val() ~= 5) then
      self.oscs[1]:setshape(param:val())
	  self.oscs[1].active =  true
	else
	  self.oscs[1].active = false
	end
  elseif param==p["osc2"] then
    if (param:val() ~= 5) then
      self.oscs[2]:setshape(param:val())
	  self.oscs[2].active =  true
	else
	  self.oscs[2].active = false
	end
  elseif param==p["osc3"] then
    if (param:val() ~= 5) then
      self.oscs[3]:setshape(param:val())
	  self.oscs[3].active =  true
	else
	  self.oscs[3].active = false
	end
  elseif param==p.flfof then
    self.lfo:setfrequency(param:val())
  elseif param==p.flfog then    
    self.lfo:setgain(param:val())
  elseif param==p.flfos then
    self.lfo:setshape(param:val())
  elseif param==p.vcaa then
	self.ahdsr:setattack(param:val())
  elseif param==p.vcah then
    self.ahdsr:sethold(param:val())
  elseif param==p.vcad then
	self.ahdsr:setdecay(param:val())
  elseif param==p.vcas then
    self.ahdsr:setsustain(param:norm())
  elseif param==p.vcar then
    self.ahdsr:setrelease(param:val())
  elseif param==p.pm1t then
	self.envf:settime(1, param:val())
  elseif param==p.pm2t then
    self.envf:settime(2, param:val())
  elseif param==p.pm3t then
	self.envf:settime(3, param:val())
  elseif param==p.pm4t then
    self.envf:setpeak(3, param:norm())
  elseif param==p.pm1p then
    self.envf:setpeak(1, param:val())
  elseif param==p.pm2p then
    self.envf:setpeak(1, param:val())
  elseif param==p.pm3p then
    self.envf:setpeak(1, param:val())
  elseif param==p.pm4p then
    self.envf:setpeak(1, param:val())
  elseif param==p.resampler then
     for i=1, 3 do        
	   self.oscs[i]:setquality(param:val())
     end
  elseif param==p.sync then
    if param:val() == 0 then
	   self.oscs[1]:setsync(nil)
	else
	   if (self.oscs[1].sync == nil) then
	      self.oscs[1]:setsync(self.master)
       end	   
	   self.oscs[1]:setfrequency(param:val())
	end
  end
end

return voice