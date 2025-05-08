-- psycle pluginselector (c) 2016 by psycledelics
-- File: pluginselector.lua
-- copyright 2016 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local node = require("psycle.node")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local fontinfo = require("psycle.ui.fontinfo")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local lexer = require("psycle.ui.lexer")
local scintilla = require("psycle.ui.scintilla")
local search = require("search")

local output = scintilla:new()

function output:new(parent)    
  local c = scintilla:new()  
  setmetatable(c, self)
  self.__index = self
  c:init()
  if parent ~= nil then
    parent:add(c)
  end
  return c
end

function output:typename()
  return "output"
end

function output:init()
  self:setautosize(false, false)  
end

function output:onkeydown(ev)
  if ev:ctrlkey() then
    if ev:keycode() == 70 or ev:keycode() == 83 then
      ev:preventdefault()
    end            
  end
end    

function output:findprevsearch(page, pos)
  local cpmin, cpmax = 0, 0
  cpmin = pos
  cpmax = 0
  if self:hasselection() then 
    cpmax = cpmax - 1
  end
  return cpmin, cpmax
end

function output:findnextsearch(page, pos)
  local cpmin, cpmax = 0, 0
  cpmin = pos
  cpmax = self:length()
  if self:hasselection() then       
    cpmin = cpmin + 1
  end
  return cpmin, cpmax
end

function output:findsearch(page, dir, pos)
  local cpmin, cpmax = 0, 0
  if dir == search.DOWN then      
    cpmin, cpmax = self:findnextsearch(self, pos)
  else
    cpmin, cpmax = self:findprevsearch(self, pos)
  end
  return cpmin, cpmax
end

function output:onsearch(searchtext, dir, case, wholeword, regexp)    
  self:setfindmatchcase(case):setfindwholeword(wholeword):setfindregexp(regexp)      
  local cpmin, cpmax = self:findsearch(self, dir, self:selectionstart())
  local line, cpselstart, cpselend = self:findtext(searchtext, cpmin, cpmax)
  if line == -1 then      
    if dir == search.DOWN then
	  self:setsel(0, 0)
	  local cpmin, cpmax = self:findsearch(self, dir, 0)
	  line, cpselstart, cpselend = self:findtext(searchtext, cpmin, cpmax)        
	else
	  self:setsel(0, 0)
	  local cpmin, cpmax = self:findsearch(self, dir, self:length())
	  line, cpselstart, cpselend = self:findtext(searchtext, cpmin, cpmax)
	end             
  end
  if line ~= -1 then
	self:setsel(cpselstart, cpselend)
	if self.searchbeginpos == cpselstart then
	  self.searchbegin = -1        
	  self.searchrestart = true
	else
	  self.searchrestart = false
	end
	if self.searchbeginpos == -1 then
	  self.searchbeginpos = cpselstart        
	end
  end      
end

function output:setproperties(properties)      
  if properties.color then    
    self:setforegroundcolor(properties.color:value())	    
  end
  if properties.backgroundcolor then    
	  self:setbackgroundcolor(properties.backgroundcolor:value())
  end
  self:styleclearall()
  self:showcaretline()
  if properties.color then    
    self:setlinenumberforegroundcolor(properties.color:value())
    self:setcaretcolor(properties.color:value())
  end
  if properties.backgroundcolor then    
	  self:setlinenumberbackgroundcolor(properties.backgroundcolor:value())
    self:setcaretlinebackgroundcolor(properties.backgroundcolor:value())
  end
  self:setselalpha(75)
  self:setfontinfo(fontinfo:new():setsize(12):setfamily("consolas"))    
  self:fls()
end

return output