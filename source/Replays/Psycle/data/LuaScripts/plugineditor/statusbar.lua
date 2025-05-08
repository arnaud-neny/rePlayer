--[[ 
psycle plugineditor (c) 2016 by psycledelics
File: statusbar.lua
copyright 2016 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")
local signal = require("psycle.signal")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local alignstyle = require("psycle.ui.alignstyle")
local group = require("psycle.ui.group")
local text = require("psycle.ui.text")
local edit = require("psycle.ui.edit")

local statusbar = group:new()

function statusbar:new(parent)  
  local c = group:new()
                 :setautosize(false, false)  
  setmetatable(c, self)  
  self.__index = self
  c:init()
  if parent ~= nil then
    parent:add(c)
  end
  return c
end

function statusbar:typename()
  return "statusbar"
end

function statusbar:init()  
  self.gotopage = signal:new()
  self.escape = signal:new()
  self:setalign(alignstyle.TOP)
      :setautosize(false, false)       
      :setposition(rect:new(point:new(), dimension:new(0, 20)));
  self.status, self.label = {}, {}  
  self.status.insert = self:createtext("ON", 30)
  self.label.insert = self:createtext("INSERT")
  self.status.col = self:createtext("1", 30)
  self.label.col = self:createtext("COL")   
  self.status.line = self:createedit("1", 30)
  self.label.line = self:createtext("LINE")
  self.status.modified = self:createtext("", 100)
  self.status.searchrestart = self:createtext("", 100)    
  self.status.control = self:createtext("", 30, alignstyle.LEFT)  
  self.status.prompttext = self:createtext("", 30):setalign(alignstyle.LEFT):setautosize(true, false)
  function self.status.control:onkeydown(ev)
    ev:preventdefault()
  end
  local that = self
  function self.status.control:onkillfocus()    
    that.commandhandler:jumpmain()
    that:clearcontrol()
    if that.commandhandler.helplevel < 3 then
      that.commandhandler:hide():parent():updatealign()
    end
  end
  self:initdefaultcolors()
  self.prefix = ""
end

function statusbar:setcommandhandler(commandhandler)
  self.commandhandler = commandhandler
  self.commandhandler.dojump:connect(statusbar.onjumpcommandhandler, self)
  commandhandler:setstatusbar(self)
  self:updatealign()
end

function statusbar:onjumpcommandhandler(link)  
  if link.prompt then
    self.status.prompttext:settext(link.prompt)
  end
end

function statusbar:updatestatus(status)
  if not self.status.line:hasfocus() then
    self.status.line:settext(status.line)  
  end
  self.status.col:settext(status.column)     
  self.status.insert:settext(
      statusbar.booltostring(not status.overtype, "ON", "OFF"))
  self.status.searchrestart:settext(
      statusbar.booltostring(status.searchrestart,"SEARCH AT BEGINNING POINT"))
  self.status.modified:settext(
      statusbar.booltostring(status.modified, "MODIFIED"))
end

function statusbar:setcontrolkey(keycode)
  self.prefix = "^" .. string.char(keycode)
  self.status.control:settext(self.prefix)  
end

function statusbar.booltostring(value, ontext, offtext)
  local result = ""
  if value == true then
    result = ontext
  elseif offtext then
    result = offtext    
  end
  return result
end

function statusbar:clearcontrol()
  self.status.control:settext(""):fls()
  self.prefix = ""
  self.status.prompttext:settext("")
end

function statusbar:onkeydown(ev)  
  if ev:keycode() == keycodes.ESCAPE then    
    self.commandhandler:jumpmain()    
    ev:stoppropagation()
    ev:preventdefault()
    if self.commandhandler.helplevel < 3 then
      self.commandhandler:hide():parent():updatealign()
    end
    self.escape:emit(self)
    self:clearcontrol()
  else
    self.status.control:settext(self.prefix .. string.char(ev:keycode()))
    self.commandhandler:jump(ev:keycode())    
    ev:stoppropagation()
    ev:preventdefault()
  end  
end

function statusbar:createedit(label, width, align)
  if not align then
    align = alignstyle.RIGHT
  end
  local wrapper = group:new(self):setautosize(false, false)
                       :setalign(align)
                       :setmargin(boxspace:new(2, 0, 2, 5))
                       :setpadding(boxspace:new(1))
  local result = edit:new(wrapper)
                     :settext(label)            
                     :setalign(alignstyle.CLIENT)                     
  local that = self
  function result:onkeydown(ev)         
    if ev:keycode() == keycodes.ENTER then   
      that.gotopage:emit(self)         
    elseif ev:keycode() == keycodes.ESCAPE then    
      that.escape:emit(self)      
    end    
    ev:stoppropagation() 
  end
  function wrapper:onmouseenter()                     
    self:setpadding(boxspace:new(0))
    self:addornament(ornamentfactory:createlineborder(0xFFFFFFFF))                     
  end
  function wrapper:onmouseout()                     
    self:removeornaments()
        :setpadding(boxspace:new(1))
  end
  if width then
    result:setautosize(false, false)    
    result:setposition(rect:new(point:new(), dimension:new(width, 15)))
    wrapper:setposition(rect:new(point:new(), dimension:new(width, 0)))
  else    
    result:setautosize(true, false)
  end        
  return result
end

function statusbar:createtext(label, width, align)
  if not align then
    align = alignstyle.RIGHT
  end
  local result = text:new(self)
                     :settext(label)
                     :setalign(align)
                     :setverticalalignment(alignstyle.CENTER)
                     :setmargin(boxspace:new(0, 0, 0, 5))
  if width then
    result:setautosize(false, false)    
    result:setposition(rect:new(point:new(), dimension:new(width, 0)))
  else    
    result:setautosize(true, false)
  end        
  return result
end

function statusbar:initdefaultcolors()
  self:setcolor(0xFFFFFFFF)
      :setstatuscolor(0xFFFFFFFF)
  -- :setbackgroundcolor(0xFF333333)
  return self
end

function statusbar:setcolor(color) 
  for i = 1, #self.label do
    self.label[i]:setcolor(color)
  end
  return self
end

function statusbar:setstatuscolor(color) 
  for i = 1, #self.status do
    self.status[i]:setcolor(color)
  end    
  return self
end

function statusbar:setbackgroundcolor(color)   
  local backgroundfill = ornamentfactory:createfill(color)
  self:addornament(backgroundfill)
  for i = 1, #self.status do
    self.status[i]:removeornaments():addornament(backgroundfill)
  end
  for i = 1, #self.label do
    self.label[i]:removeornaments():addornament(backgroundfill)
  end  
  return self
end

function statusbar:setproperties(properties)
  local setters = {"color", "statuscolor", "backgroundcolor"}
  for _, setter in pairs(setters) do        
    local property = properties[setter]      
    if property then      
      statusbar["set"..setter](self, property:value())
    end
  end
  self:fls()  
end

return statusbar
