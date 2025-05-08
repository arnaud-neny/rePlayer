--[[ psycle plugineditor (c) 2017 by psycledelics
File: commands.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local boxspace = require("psycle.ui.boxspace")
local command = require("psycle.command")
local textpage = require("textpage")
local filesave = require("psycle.ui.filesave")
local fileopen = require("psycle.ui.fileopen")

local newpagecommand = command:new()

function newpagecommand:new(pagecontainer, name, preventedkeys)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  c.name = name
  c.preventedkeys = preventedkeys  
  return c
end

function newpagecommand:execute()
  local page = textpage:new(nil)                       
                       :setpreventedkeys(self.preventedkeys)                       
  if self.name == nil then                       
    self.name = page:createdefaultname();
  end  
  self.pagecontainer_:addpage(page, self.name)  
end

local loadpagecommand = command:new()

function loadpagecommand:new(pagecontainer, fileexplorer, preventedkeys)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  c.fileexplorer_ = fileexplorer
  c.fileopen = fileopen:new()
  c.fileopen:setfolder(fileexplorer:path())
  c.preventedkeys = preventedkeys    
  return c
end

function loadpagecommand:execute()
  self.fileopen:show()
  if self.fileopen:isok() then
    local page = self:dopageexist(self.fileopen:filename())  
    if page ~= nil then
      page:reload()
      if self.pagecontainer_:activepage() ~= page then   
        self.pagecontainer_:setactivepage(page)
      end
    else
      local sep = "\\"  
      local dir = self.fileopen:filename():match("(.*"..sep..")") 
      local name = self.fileopen:filename():match("([^\\]+)$")  
      newpagecommand:new(self.pagecontainer_, name, self.preventedkeys):execute(); 
      page = self.pagecontainer_:lastinsert()
      page:loadfile(self.fileopen:filename())         
    end
  end
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():setfocus()
  end
end

function loadpagecommand:dopageexist(fname)
  local found = nil
  local windows = self.pagecontainer_.children:windows()    
  for i=1, #windows do
    local page = windows[i]
    if page:filename() == fname then
      found = page
      break
    end
  end
  return found
end

local saveresumepagecommand = command:new()

function saveresumepagecommand:new(pagecontainer, fileexplorer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  c.fileexplorer_ = fileexplorer
  c.filesaveas = filesave:new() 
  return c
end

function saveresumepagecommand:execute()
  local page = self.pagecontainer_:activepage()
  if page then
    local fname = ""     
    if page:hasfile() then
      fname = page:filename()    
      local sep = "\\"  
      local dir = fname:match("(.*" .. sep .. ")")    
      page:savefile(fname)
      fname = fname:match("([^\\]+)$")
      self.pagecontainer_:setlabel(page, fname)
    else    
      self.filesaveas:setfolder(self.fileexplorer_:path())
      self.filesaveas:show()
      if self.filesaveas:isok() then      
        fname = self.filesaveas:filename()          
        page:savefile(fname)
        fname = fname:match("([^\\]+)$")
        self.pagecontainer_:setlabel(page, fname)
      end
    end
  end
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():setfocus()
  end
end

local abandonpagecommand = command:new()

function abandonpagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function abandonpagecommand:execute()
  self.pagecontainer_:removepage(self.pagecontainer_:activepage())
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():setfocus()
  end
end

local savedonepagecommand = command:new()

function savedonepagecommand:new(pagecontainer, fileexplorer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  c.fileexplorer_ = fileexplorer
  c.filesaveas = filesave:new() 
  return c
end

function savedonepagecommand:execute()
  saveresumepagecommand:new(self.pagecontainer_, self.fileexplorer_):execute()
  abandonpagecommand:new(self.pagecontainer_):execute()  
end

local saveexitpagecommand = command:new()

function saveexitpagecommand:new(pagecontainer, fileexplorer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  c.fileexplorer_ = fileexplorer
  c.filesaveas = filesave:new() 
  return c
end

function saveexitpagecommand:execute()
  saveresumepagecommand:new(self.pagecontainer_, self.fileexplorer_):execute()
  psycle.proxy:exit()
end

local sethelplevelcommand = command:new()

function sethelplevelcommand:new(commandhandler, level)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.commandhandler_ = commandhandler
  c.level_ = level;
  return c
end

function sethelplevelcommand:execute()
  self.commandhandler_.helplevel = self.level_;
end

local displaysearchcommand = command:new()

function displaysearchcommand:new(search)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.search_ = search
  return c
end

function displaysearchcommand:execute()
  self.search_:root():updatealign()  
  self.search_:show():onfocus()
end

local commandfactory = {}

commandfactory.NEWPAGE = 1
commandfactory.LOADPAGE = 2
commandfactory.SAVERESUMEPAGE = 3
commandfactory.SAVEDONEPAGE = 4
commandfactory.SAVEEXITPAGE = 5
commandfactory.ABANDONPAGE = 6 
commandfactory.HELPLEVEL = 7
commandfactory.DISPLAYSEARCH = 8

function commandfactory.build(cmd, ...)
  local result = nil
  if cmd == commandfactory.NEWPAGE then
    result = newpagecommand:new(...)
  elseif cmd == commandfactory.LOADPAGE then
    result = loadpagecommand:new(...)
  elseif cmd == commandfactory.SAVERESUMEPAGE then
    result = saveresumepagecommand:new(...)
  elseif cmd == commandfactory.SAVEDONEPAGE then
    result = savedonepagecommand:new(...)
  elseif cmd == commandfactory.SAVEEXITPAGE then
    result = saveexitpagecommand:new(...)
  elseif cmd == commandfactory.ABANDONPAGE then
    result = abandonpagecommand:new(...)  
  elseif cmd == commandfactory.HELPLEVEL then
    result = sethelplevelcommand:new(...)    
  elseif cmd == commandfactory.DISPLAYSEARCH then
    result = displaysearchcommand:new(...)    
  end 
  return result
end

return commandfactory
