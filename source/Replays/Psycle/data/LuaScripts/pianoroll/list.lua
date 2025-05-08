--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  list.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local list = {}

function list:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:clear()
  return m
end

function list:first()
  return self.head  
end

function list:clear()
  self.head, self.tail = nil, nil
end

function list:search(pos, start)
  local insertptr = nil
  local e = start and start or self.head
  while e and e:position() <= pos do    
    insertptr = e
    e = e.next
  end  
  return insertptr
end

function list:insert(ev, pos, start)
  insertptr, ev.prev , ev.next = nil, nil, nil   
  if ev then    
    insertptr = self:search(pos, start)    
    if not self.head then
      self.head = ev
    else
      if not insertptr then
        self.head.prev = ev
        ev.next = self.head
        self.head = ev
      else
        ev.prev = insertptr
        ev.next = insertptr.next
        insertptr.next = ev        
      end           
    end
    if ev.next then
      ev.next.prev = ev
    else
      self.tail = ev
    end    
  else     
    psycle.output("Warning: pattern insert is nil.")
  end
  return nil
end

function list:prevevent(track, event)
  local result = nil
  if event then 
    result = event.prev
    while result and result:track() ~= track do        
      result = result.prev
    end
  end
  return result
end

function list:nextevent(track, event)
  local result = nil
  if event then 
    result = event.next
    while result and result:track() ~= track do     
      result = result.next
    end
  end
  return result
end

function list:erase(event)       
  if event.next then
    event.next.prev = event.prev        
  end
  if event.prev then
    event.prev.next = event.next
  end
  if event.prev == nil then     
    self.head = event.next
  end            
  return self   
end

function list:append(event)
  event.prev, event.next = nil, nil                        
  if self.head == nil then              
    self.head = event
  else
    event.prev = self.tail
    self.tail.next = event            
  end
  self.tail = event
  return self
end

return list
