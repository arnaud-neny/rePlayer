--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  pattern.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local patternevent = require("patternevent")
local rawpattern = require("psycle.pattern"):new()
local player = require("psycle.player")
local signal = require("psycle.signal")
local list = require("list")

local blank = { parameter = 0, cmd = 0, mach = 255, inst = 255, val = 255 }
local stopnote = { parameter = 0, cmd = 0, mach = 0, inst = 0, val = 120 }

local pattern = {}

pattern.MAXLINES = 1024
pattern.MAXTRACKS = 64

function pattern:new(ps)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(ps)
  return m
end

function pattern:init(ps)
  self.notelist_ = list:new()
  self.cmdlist_ = list:new()
  self.ps_ = ps 
  self.player_ = player:new()
  self:import()
  self.changed = signal:new()
  self.numlines_ = 64
end

function pattern:firstnote()
  return self.notelist_:first()
end

function pattern:firstcmd()
  return self.cmdlist_:first()
end

function pattern:import()
  if self.ps_ then
    self:clear()
    local prevev = {}
    local numlines = rawpattern:numlines(self.ps_)  
    for line=0, numlines - 1 do
      local events = rawpattern:line(self.ps_, line)
      for i = 1, #events do
        local ev = events[i]      
        local pos = line / self.player_:tpb()
        if ev.val == 120 then
          local prev = prevev[ev.track + 1]
          if prev then
            prev:setstopoffset(pos - prev:position())        
          end
        else
          local event = patternevent:new():setposition(pos):setraw(ev)
          if ev.val < 120 then                                 
            event.line = line 
            self.notelist_:append(event)
            local prev = prevev[ev.track + 1]
            if prev and not prev:hasstop() then
              prev:setlength(pos - prev:position())
            end 
            prevev[ev.track + 1] = event                   
          elseif ev.inst ~= 0xFF  or ev.cmd ~= 0 or ev.parameter ~= 0 then          
            self.cmdlist_:append(event)              
            event.line = line
            event:setlength(self.player_:bpt())
            event.ps = self.ps_
          end      
        end
      end
    end
    for track=1, rawpattern:numtracks() do
      local prev = prevev[track]
      if prev and not prev:hasstop() then
        prev:setlength(self:numbeats() - prev:position())
      end    
    end
  end
  return self
end

function pattern:importpsb(filename)
  local function readuint8(file)
    local bytes = file:read(1)
    local b = bytes:byte(1, 1)
    return b
  end  
  local function readint32(file)
    local bytes = file:read(4)
    local b1, b2, b3, b4 = bytes:byte(1, 4)
    n = b1 + b2*256 + b3*65536 + b4*16777216
    return n > 2147483647 and (n - 4294967296) or n
  end  
	local file = io.open(filename, "rb")
  if (file) then
    self:clear()
    local prevev = {}
    local numtracks = math.min(readint32(file), pattern.MAXTRACKS)
    local numlines = math.min(readint32(file), pattern.MAXLINES)
    for track=0, numtracks - 1 do
			for line=0, numlines - 1 do
        local note = readuint8(file)
        local inst = readuint8(file)
        local mach = readuint8(file)
        local cmd = readuint8(file)
        local parameter = readuint8(file)
        local pos = line / self.player_:tpb()
        if note == 120 then
          local prev = prevev[track + 1]
          if prev then
            prev:setstopoffset(pos - prev:position())        
          end
        else
          local event = patternevent:new()
                                    :setposition(pos)
                                    :setnote(note)
                                    :setinst(inst)
                                    :setmach(mach)
                                    :setparameter(parameter)
                                    :settrack(track)                                    
          if note < 120 then                                 
            event.line = line 
            self.notelist_:append(event)
            local prev = prevev[track + 1]
            if prev and not prev:hasstop() then
              prev:setlength(pos - prev:position())
            end 
            prevev[track + 1] = event                   
          elseif inst ~= 0xFF  or cmd ~= 0 or parameter ~= 0 then          
            self.cmdlist_:append(event)              
            event.line = line
            event:setlength(self.player_:bpt())         
          end 
        end          
      end
    end
    io.close(file)
    for track=1, numtracks do
      local prev = prevev[track]
      if prev and not prev:hasstop() then
        prev:setlength(self:numbeats() - prev:position())
      end    
    end
    self.numlines_ = numlines     
  else
    psycle.output("Read pattern block error: " .. filename .. "\n")
  end	
end

function pattern:changelength(event, length)
  local line = self:beattoline(event:position() + length)
  if line <= event.line then
    line = event.line + 1
  end
  if self.ps_ and event:hasstop() then
    rawpattern:setevent(self.ps_, event:track(), event.line + self:beattoline(event.stopoffset), blank)    
  end
  local oldev = rawpattern:eventat(self.ps_, event:track(), line)
  if oldev.val ~= 255 then
    event.stopoffset = 0
  else     
    event:setstopoffset(self:linetobeat(line - event.line))
    stopnote.inst = event:inst()
    stopnote.mach = event:mach()
    rawpattern:setevent(self.ps_, event:track(), line, stopnote)
  end
  return self
end

function pattern:setinst(event, inst)  
  event:setinst(inst)
  if self.ps_ then
    rawpattern:setevent(self.ps_, event:track(), event.line, event:raw())
  end
  return self
end

function pattern:setmach(event, mach)
  event:setmach(mach)
  if self.ps_ then
    rawpattern:setevent(self.ps_, event:track(), event.line, event:raw())
  end
  return self
end

function pattern:setcmd(event, cmd)
  event:setcmd(cmd)
  if self.ps_ then
    rawpattern:setevent(self.ps_, event:track(), event.line, event:raw())
  end
  return self
end

function pattern:setparameter(event, param)
  event:setparameter(param)
  if self.ps_ then
    rawpattern:setevent(self.ps_, event:track(), event.line, event:raw())
  end
  return self
end

function pattern:changenote(event, note)
  self:syncevent(event:setnote(note))
  return self
end

function pattern:syncevent(event)
  if self.ps_ then
    rawpattern:setevent(self.ps_, event:track(), event.line, event:raw())
  end
  return self
end

function pattern:setposition(event, pos) 
  self:erase(event)
  self:insert(event, pos)
  return self
end

function pattern:setleft(event, pos)
  if event:hasstop() then    
    event:setstopoffset(event:position() - pos + event.stopoffset)
  end  
  self:setposition(event, pos)  
  return self
end

function pattern:erase(event)
  if event:note() < 120 then
    self:erasenote(event)
  else
    self:erasecmd(event)
  end
  return self
end

function pattern:erasenote(event)
    local prev = self:prevevent(event:track(), event)      
    if prev and not prev:hasstop() then 
      local next = self:nextevent(event:track(), event)
      prev:setlength((next and next:position() or self:numbeats()) - prev:position())
    end
    self.notelist_:erase(event)
    if self.ps_ then
      rawpattern:setevent(self.ps_, event:track(), event.line, blank)
      if event:hasstop() then
        rawpattern:setevent(self.ps_, event:track(), event.line + self:beattoline(event.stopoffset), blank)
      end  
    end
   self.changed:emit(self)
   return self
end

function pattern:erasecmd(event) 
   self.cmdlist_:erase(event)
   rawpattern:setevent(self.ps_, event:track(), event.line, blank)
   self.changed:emit(self)
   return self
end

function pattern:erasestopoffset(event)   
  if event:hasstop() then   
    local next = self:nextevent(event:track(), event)
    event:setlength((next and next:position() or self:numbeats()) - event:position())   
    rawpattern:setevent(self.ps_, event:track(), event.line + self:beattoline(event.stopoffset), blank) 
    event:clearstop()
  end
  return self
end 

function pattern:insert(ev, pos, start)    
  ev.line = math.floor((pos and pos or ev:position()) * self.player_:tpb()) 
  ev:setposition(self:linetobeat(ev.line))
  if ev:note() < 120 then
    self:insertnote(ev, start)
  else
    self:insertcmd(ev, start)
  end
  return self
end

function pattern:insertnote(ev, start) 
  self.notelist_:insert(ev, ev:position())
  if self.ps_ then  
    local raw = rawpattern:eventat(self.ps_, ev.track_, ev.line)
    if raw.val > 120 then
      local cmd = self.cmdlist_:search(ev:position())
      if cmd and cmd:position() == ev:position() then
        self:erasecmd(cmd)
      end
    end
  end
  local prev = self:prevevent(ev:track(), ev)    
  if prev then
    if prev:position() == ev:position() then
      self.notelist_:erase(prev)
      prev = self:prevevent(ev:track(), ev)        
    end    
    if prev then      
      if not prev:hasstop() then
        prev:setlength(ev:position() - prev:position())
      elseif prev:position() + prev.stopoffset == ev:position() then
        if prev.line + 1 == ev.line then
          prev:clearstop()
        else
          prev:setstopoffset(prev:length() - self.player_:bpt())
        end
      elseif prev:position() + prev.stopoffset > ev:position() then
        prev:clearstop()
        prev:setlength(ev:position() - prev:position())
      end
    end
  end
  local next = self:nextevent(ev:track(), ev)
  ev:setlength((next and next:position() or self:numbeats()) - ev:position())  
  self:syncevent(ev)
  if ev:hasstop() then  
    if next and ev.line + self:beattoline(ev.stopoffset) >= next.line then
      ev:clearstop()
    else    
      ev:setlength(ev.stopoffset)
      stopnote.inst = ev:inst()
      stopnote.mach = ev:mach()
      if self.ps_ then
        rawpattern:setevent(self.ps_, ev:track(), ev.line + self:beattoline(ev.stopoffset), stopnote)
      end      
    end
  end
  self.changed:emit(self)
  return self
end

function pattern:insertcmd(ev, start)   
  self.cmdlist_:insert(ev, ev:position())  
  ev:clearstop()
  ev:setlength(self.player_:bpt())
  if self.ps_ then
    local raw = rawpattern:eventat(self.ps_, ev.track_, ev.line)
    if raw.val < 120 then
      local note = self.notelist_:search(ev:position())   
      if note and note:position() == ev:position() then       
        self:erasenote(note)
      end
    end  
    local prev = self:prevevent(ev.track_, ev)
    if prev and prev:position() == ev:position() then      
      if prev.prev then
        prev = prev.prev
        prev.next = ev
        ev.prev = prev
      else
        self.cmdlist_.head = ev
        ev.prev = nil       
      end           
    end      
    self:syncevent(ev)
  end  
  self.changed:emit(self)
  return self
end

function pattern:prevevent(track, event)
  local result = nil
  if event then 
    result = event.prev
    while result and result:track() ~= track do        
      result = result.prev
    end
  end
  return result
end

function pattern:nextevent(track, event)
  local result = nil
  if event then 
    result = event.next
    while result and result:track() ~= track do     
      result = result.next
    end
  end
  return result
end

function pattern:clear()
  self.notelist_:clear()
  self.cmdlist_:clear()
  return self
end

function pattern:numbeats()
  return self.ps_ and self:linetobeat(rawpattern:numlines(self.ps_))
                  or self.numlines_ * self.player_:bpt()        
end

function pattern:numlines()
  return self.ps_ and rawpattern:numlines(0) or self.numlines_
end

function pattern:rasterbeat(position)
  return self:linetobeat(self:beattoline(position))
end

function pattern:linetobeat(linenumber)
  return linenumber / self.player_:tpb()
end

function pattern:beattoline(beat)
  return math.floor(beat * self.player_:tpb())
end 

function pattern:selectall()
  self:work(function(event) event:select(self) end, self:firstnote())
  self:work(function(event) event:select(self) end, self:firstcmd())
  return self
end

function pattern:deselectall()
  self:work(function(event) event:deselect() end, self:firstnote())
  self:work(function(event) event:deselect() end, self:firstcmd())
  return self
end

function pattern:work(f, head)
  local curr = head
  while curr do
    f(curr)
    curr = curr.next
  end
  return self
end 

function pattern:eraseselection()
  local events = self:selection()
  for i=1, #events do 
    self:erase(events[i])    
  end
  self.changed:emit(self)
  return self
end

function pattern:setselection(events)
  self:deselectall()
  if events then
    for i = 1, #events do    
      events[i]:select(self)
    end
  end
  return self
end

function pattern:selection()
  local result = {}
  local curr = self:firstnote()
  while curr do
    if curr:selected() then     
      result[#result + 1] = curr     
    end      
    curr = curr.next
  end
  local curr = self:firstcmd()
  while curr do
    if curr:selected() then     
      result[#result + 1] = curr     
    end      
    curr = curr.next
  end
  return result
end

function pattern:dump()
  psycle.output("Patterndump ", self.ps_, "--------Notes--------", "\n")
  self:work(function(event) event:dump() end, self:firstnote())  
  psycle.output("--------Commands--------", "\n")
  self:work(function(event) event:dump() end, self:firstcmd())
  return self
end

function pattern:deleterow(pos, track)
  rawpattern:deleterow(self.ps_, track, self:beattoline(pos))  
  self:import()
  return self
end

function pattern:insertrow(pos, track)
  rawpattern:insertrow(self.ps_, track, self:beattoline(pos))  
  return self:import()
end

function pattern:copy(source, note, pos, track, stopoffset)
  local events = {} 
  self:work(
    function(event)
      local copy = event:clone()
                        :setposition(event:position() + pos)
                        :setnote(event:note() + note)
                        :settrack(event:track() + track)
      if stopoffset then
        copy:setstopoffset(stopoffset)
      end
      self:insert(copy)
      events[#events + 1] = copy      
    end,
    source:firstnote()
  )
  return events
end

function pattern:preventnotify()
  self.changed:prevent()
  return self
end

function pattern:enablenotify()
  self.changed:enable()
  return self
end

function pattern:notify()
  self.changed:emit(self)
end

return pattern
