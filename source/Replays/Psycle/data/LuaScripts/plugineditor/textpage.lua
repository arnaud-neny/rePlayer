--[[ 
psycle pluginselector (c) 2016 by psycledelics
File: pluginselector.lua
copyright 2016 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local cfg = require("psycle.config"):new("PatternVisual")
local node = require("psycle.node")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local lexer = require("psycle.ui.lexer")
local scintilla = require("psycle.ui.scintilla")
local search = require("search")
local serpent = require("psycle.serpent")
local sci = require("scintilladef")
local textpage = scintilla:new()

textpage.pagecounter = 0
textpage.indicator = 14

textpage.EDITMODE = 0
textpage.INFOMODE = 1

textpage.keywords = "and break do else elseif end false for function if in local nil not or repeat return then true until while"

function textpage:new(parent)    
  local c = scintilla:new(parent)  
  setmetatable(c, self)
  self.__index = self
  c:init()
  textpage.pagecounter = textpage.pagecounter + 1
  return c
end

function textpage:typename()
  return "textpage"
end

function textpage:init()  
  self:setautosize(false, false)
  self.mode = textpage.EDITMODE  
  self.preventedkeys = {0x41, 0x46, 0x4A, 0x4B, 0x44, 0x45, 0x51, 0x56, 0x45,
      78, 79, 87, 70, 83, 87}
  self.blockforegroundcolor = 0x00dd00
  psycle.setting():addlistener(self)
  self:onapplysetting()
  self:initblockmodus()
end

function textpage:setpreventedkeys(preventedkeys)
  self.preventedkeys = preventedkeys
  return self
end

function textpage:onkeydown(ev)  
  if ev:ctrlkey() then     
    self:preventkeys(ev)      
  end        
end

function textpage:preventkeys(ev)
  for _, preventedkey in pairs(self.preventedkeys) do
    if ev:keycode() == preventedkey then      
      ev:preventdefault()
      break;
    end      
  end
end

function textpage:onmarginclick(linepos)
  self:addbreakpoint(linepos)
end

function textpage:addbreakpoint(linepos)
  self:definemarker(1, 31, 0x0000FF, 0x0000FF)
  self:addmarker(linepos, 1)
  return self
end

function textpage:status()  
  local that = self
  return {
    line = that:line() + 1,
    column = that:column() + 1,
	  modified = that:modified(),
	  overtype = that:overtype()
  }  
end

function textpage:createdefaultname()
  return "new" .. textpage.pagecounter  
end

function textpage:findprevsearch(page, pos)
  local cpmin, cpmax = 0, 0
  cpmin = pos
  cpmax = 0
  if self:hasselection() then 
    cpmax = cpmax - 1
  end
  return cpmin, cpmax
end

function textpage:findnextsearch(page, pos)
  local cpmin, cpmax = 0, 0
  cpmin = pos
  cpmax = self:length()
  if self:hasselection() then       
    cpmin = cpmin + 1
  end
  return cpmin, cpmax
end

function textpage:findsearch(page, dir, pos)
  local cpmin, cpmax = 0, 0
  if dir == search.DOWN then      
    cpmin, cpmax = self:findnextsearch(self, pos)
  else
    cpmin, cpmax = self:findprevsearch(self, pos)
  end
  return cpmin, cpmax
end

function textpage:onsearch(searchtext, dir, case, wholeword, regexp)    
  self:setfindmatchcase(case)
  self:setfindwholeword(wholeword)
  self:setfindregexp(regexp)      
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

function textpage:onapplysetting()
  local setting = psycle.setting():find("textpage")
  if setting and setting.properties then
    local properties = setting.properties
    if properties.mapcapslocktoctrl ~= nil and 
       properties.mapcapslocktoctrl:value() then    
      self:mapcapslocktoctrl()
    else
      self:enablecapslock()
    end     
  end
  local setting = psycle.setting():find("tab")
  if setting and setting.properties then
    local properties = setting.properties
    if properties.tabwidth ~= nil then    
      self:settabwidth(properties.tabwidth:value())
    end   
  end
end

function textpage:setproperties(properties)    
  if properties.color then
    self:setforegroundcolor(properties.color:value())	    
  end
  if properties.backgroundcolor then    
	  self:setbackgroundcolor(properties.backgroundcolor:value())
  end
  self:styleclearall()
  self:showcaretline()    
  local lexsetters = {"commentcolor", "commentlinecolor", "commentdoccolor",                      
                      "operatorcolor", "wordcolor", "stringcolor",
                      "identifiercolor", "numbercolor"}  
  local lex = lexer:new() 
  lex:setkeywords(textpage.keywords)  
  for _, setter in pairs(lexsetters) do        
    local property = properties["lua_lexer_" .. setter]      
    if property then            
      lex["set"..setter](lex, property:value())
    end
  end 
  self:setlexer(lex)  
  local setters = {"linenumberforegroundcolor", "linenumberbackgroundcolor",
                   "foldingbackgroundcolor", "selbackgroundcolor",
                   "caretcolor", "caretlinebackgroundcolor"} 
  for _, setter in pairs(setters) do        
    local property = properties[setter]      
    if property then            
      textpage["set"..setter](self, property:value())
    end
  end    
  local markerfore = 0xFFFFFFFF
  local markerback = 0xFFFFFFFF
  if properties.foldingmarkerforecolor then
    markerfore = properties.foldingmarkerforecolor:value()
  end
  if properties.foldingmarkerbackcolor then
    markerback = properties.foldingmarkerbackcolor:value()
  end  
  if properties.blockforegroundcolor then
    self.blockforegroundcolor = properties.blockforegroundcolor:value()
    self:setupindicators()
  end
  self:setfoldingmarkercolors(markerfore, markerback)
  self:setselalpha(75)    
  self:showcaretline()
  return self
end

function textpage:setblockbegin()
  self.blockbegin = self:selectionstart()
  self:updateblock()
  return self
end

function textpage:setblockend()
  self.blockend = self:selectionstart()
  self:updateblock()
  return self
end

function textpage:deleteblock() 
  if self.blockbegin ~=-1 and self.blockend ~= -1 then   
    self:deletetextrange(self.blockbegin, self.blockend - self.blockbegin)
    self.blockbegin, self.blockend = -1, -1
    self:clearblockselection()
  end
  return self
end

function textpage:deletesel()
  if self:hasselection() then
    self:replacesel("")
    self:clearblockselection()
  end
  return self
end

function textpage:copyblock() 
  if self.blockbegin ~=-1 and self.blockend ~= -1 then
    local text = self:textrange(self.blockbegin, self.blockend)
    self:inserttext(text, self:currentpos())
  end
  return self
end

function textpage:moveblock() 
  if self.blockbegin ~=-1 and self.blockend ~= -1 then   
    local text = self:textrange(self.blockbegin, self.blockend)
    self:deletetextrange(self.blockbegin, self.blockend - self.blockbegin)
    local cp = self:currentpos()           
    self:inserttext(text, cp)
    self.blockbegin = cp
    self.blockend = cp + #text
    self:updateblock()
  end
  return self
end

function textpage:updateblock()
  if self.blockbegin ~=-1 and self.blockend ~= -1 then
     self:clearblockselection()
     self:f(sci.SCI_SETINDICATORCURRENT, textpage.indicator, 0)
     self:f(sci.SCI_INDICATORFILLRANGE, self.blockbegin,
            self.blockend - self.blockbegin)      
  end
end

function textpage:clearblockselection()
  self:f(sci.SCI_SETINDICATORCURRENT, textpage.indicator, 0)
  self:f(sci.SCI_INDICATORCLEARRANGE, textpage.indicator, self:length())
  return self
end

function textpage:initblockmodus()
  self.blockbegin, self.blockend = -1, -1  
  self:setupindicators()
  return self
end

function textpage:setupindicators()
   self:f(sci.SCI_INDICSETSTYLE, 8, sci.INDIC_PLAIN)
   self:f(sci.SCI_INDICSETSTYLE, 9, sci.INDIC_SQUIGGLE)
   self:f(sci.SCI_INDICSETSTYLE, 10, sci.INDIC_TT)
   self:f(sci.SCI_INDICSETSTYLE, 11, sci.INDIC_DIAGONAL)
   self:f(sci.SCI_INDICSETSTYLE, 12, sci.INDIC_STRIKE)
   self:f(sci.SCI_INDICSETSTYLE, 13, sci.INDIC_BOX)
   self:f(sci.SCI_INDICSETSTYLE, 14, sci.INDIC_ROUNDBOX)
   self:f(sci.SCI_INDICSETFORE, textpage.indicator, self.blockforegroundcolor)
   return self
end

function textpage:selall()
  self:setsel(0, self:length())
  return self
end

function textpage.__gc()
  textpage.pagecounter = textpage.pagecounter - 1
end

return textpage
