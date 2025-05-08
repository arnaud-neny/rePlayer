-- psycle parameter
-- file : psycle/parameter.lua
-- copyright 2014 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.
-- 
-- The parameter module can be used to interact with the psycle plugin gui.
-- Use an ordered table (psycle.orderedtable) and add the parameters to that.
-- Finally add the ordered table in the machine:init with 
-- self:addparameters(table). Use the parameter method addlistener(listener) to
-- receive events (ontweaked/aftertweaked) with the listener.

local parameter = {MPFNULL = 0, MPFLABEL = 1, MPFSTATE = 2, MPFINFO = 3,
                   MPFLEDS = 4, MPFONOFF = 5}

-- overall constructor
function parameter:new(name, label, min, max, step, val, mpf, id)
  local o = {name_ = name,
			 label_ = label,
			 min_ = min,
			 max_ = max,
       velrange_ = 0, 
			 learn_ = false,
			 step_ = step,
			 val_ = (val-min)/(max-min),
			 mpf_ = mpf,			 
			 hasvel_ = false,
			 listener_ = {}
			 }
  if id==nil then
    o.id_ = name
  else
    o.id_ = id;
  end
  setmetatable(o.listener_, {__mode ="kv"})  -- weak table
  setmetatable(o, self)
  self.__index = self
  return o
end

function parameter:newcopy(src)   
  local o = {listener_ = {}}  
  setmetatable(o.listener_, {__mode ="kv"})  -- weak table
  setmetatable(o, self)
  self.__index = self
  o:copy(src)
  return o  
end

function parameter:copy(src)
  self.name_ = src.name_
  self.label_ = src.label_
  self.min_ = src.min_
  self.max_ = src.max_
  self.step_ = src.step_
  self.val_ = src.val_
  self.mpf_ = src.mpf_
  self.id_ = src.id_  
  self.velrange_ = src.velrange_
  self.learn_ = src.learn_
  self.hasvel_ = src.hasvel_
end

-- specialized constructors
function parameter:newspace()
  return self:new("", 0, 0, 0, 0, 0, parameter.MPFNULL)
end

function parameter:newlabel(name)
  return self:new(name, 0, 0, 0, 0, 0, parameter.MPFLABEL)
end

function parameter:newinfo(name)
  return self:new(name, "", 0, 10, 1, 2, 3)
end

function parameter:newleds(name, num, val)
  return self:new(name, "", 0, num, num, val, 4)
end

function parameter:newonoff(name, val)
  return self:new(name, "", 0, 1, 1, val, 5)
end

function parameter:newknob(name, label, min, max, step, val, hasvel)  
  local newparam = self:new(name, label, min, max, step, val, parameter.MPFSTATE)
  newparam.hasvel_ = hasvel
  return newparam
end

-- parameter methods
function parameter:__tostring() return self.name_ end
function parameter:hasvelrange() return self.velrange_ ~= 0 end
function parameter:velrange() return self.velrange_ end
function parameter:hasvel() return self.hasvel_ end
function parameter:start() return self.start_ end
function parameter:range() return self.min_, self.max_, self.step_ end
function parameter:mpf() return self.mpf_ end
function parameter:setid(id) self.id_ = id end
function parameter:id() return self.id_ end
function parameter:setname(str) self.name_ = str end
function parameter:name() return self.name_ end
function parameter:label() return self.label_ end
function parameter:setminval(min) local tmp = self:val() self.min_ = min if tmp < min then tmp = min end self:setval(tmp) end
function parameter:minval() return self.min_ end
function parameter:setmaxval(max) local tmp = self:val() self.max_ = max if tmp > max then tmp = max end self:setval(tmp) end
function parameter:maxval() return self.max_ end
function parameter:setsteps(steps) local tmp = self:val() self.step_ = steps self:setval(tmp) end
function parameter:steps() return self.step_ end
function parameter:val()
  local tmp = self.val_*(self.max_ - self.min_)+self.min_
  if vel == nil then
    return tmp
  else    
    local v = tmp + self.velrange_*vel
    return math.max(self.min_, math.min(v, self.max_))    
  end
end

function parameter:setval(val)
  self:setnorm((val - self.min_)/(self.max_ - self.min_))
  return self
end

function parameter:display()
  if self:hasvel() then
    return self:veldisplay()
  else
    return self:val()
  end
end  

function parameter:veldisplay()
  if self:hasvelrange() then	         
     if self.learn_ == 1 then		  
	   return self:start()..".."..self:start() + self:velrange()
	 else		  
	   local min, max = self:range()		  
	   local mv = math.max(min, math.min(self:val() + self:velrange(), max))
	   return self:val()..".."..mv		
	 end
  else
	return self:val()
  end  
end

function parameter:norm() return self.val_ end
function parameter:setnorm(val)
  val = math.floor(self.step_*val+0.5)/self.step_
  if self.val_ ~= val then	
	if self.learn_ == true then
	  local tmp = val*(self.max_ - self.min_) + self.min_
      self.velrange_ = tmp - self.start_	
    else
	  self.val_ = val
	end
    self:notify()
  end
  return self
end

function parameter:learn(dolearn)    
    if dolearn == true then	   
	  self.start_ = self.val_*(self.max_ - self.min_) + self.min_
    elseif dolearn == false and self_learn == true then
	  --setval(velpar:start())	  
	end
    self.learn_ = dolearn    
end

function parameter:startnotify()
  for k, v in pairs(self.listener_) do      
     if (v.onstarttweak ~=nil) then	   
       v:onstarttweak(self)
	 end
  end
  return self
end

function parameter:afternotify()
  for k, v in pairs(self.listener_) do      
     if (v.onaftertweaked ~=nil) then	   
       v:onaftertweaked(self)
	 end
  end
  return self
end

function parameter:addlistener(listener)
  local found = false
  for k=1, #self.listener_ do      
     if (self.listener_[k]==listener) then	   
       found = true
	   break
	 end
  end
  if not found then
    self.listener_[#self.listener_+1]=listener
  end
  return self
end

function parameter:notify()  
  for k, v in pairs(self.listener_) do     
     v:ontweaked(self)
  end
  return self
end

return parameter