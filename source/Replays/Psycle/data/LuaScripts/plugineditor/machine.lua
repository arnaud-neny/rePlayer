-- psycle plugineditor (c) 2015 by psycledelics
-- File: machine.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

-- require('mobdebug').start()

machine = require("psycle.machine"):new()
  
local mainviewport = require("mainviewport")
local mainmenu = require("mainmenu")
local project = require("project")
local templateparser = require("templateparser")
local settingsmanager = require("psycle.settingsmanager")
local serpent = require("psycle.serpent")
  
function machine:info()
  return { 
    vendor  = "psycle",
    name    = "Plugineditor",
    mode    = machine.HOST,
    version = 0,
    api     = 0
  }
end

-- help text displayed by the host
function machine:help()
  return "Ctrl + JH3 sets full help"
end

function machine:init(samplerate)  
   self.settingsmanager = settingsmanager:new("plugineditor")
   psycle.setsetting(self.settingsmanager)   
   self.project = project:new()   
   self.mainviewport = mainviewport:new(self)   
   self:setviewport(self.mainviewport)
   self.mainmenu = mainmenu:new(self.mainviewport:filecommands(),
                                self.mainviewport:editcommands(),
                                self.mainviewport:runcommands())
   psycle.setmenurootnode(self.mainmenu:rootnode())
   local that = self
   local function f()
     that.mainviewport:onidle()
   end
   self.timerid = self:setinterval(f, 300)   
end

function machine:editmachineindex()
  local result = -1
  if self.project and self.project:plugininfo() then          
    result = self.project:pluginindex()
  end
  return result;
end

function machine:projectinfo()
  local result = nil
  if self.project and self.project:plugininfo() then          
    result = self.project:plugininfo()
  end
  --psycle.alert(serpent.dump(result))
  return result;
end

function machine:onexecute(msg, macidx, plugininfo, trace)    
  if msg ~= nil then
	  self.project:setplugininfo(plugininfo):setpluginindex(macidx)
	  self.mainviewport:fillinstancecombobox():setpluginindex(macidx)
	  self.mainviewport:setoutputtext(msg)
	  self.mainviewport:setcallstack(trace)
	  for i=1, #trace do
      if self.mainviewport:openinfo(trace[i]) then
		    break
	    end
	  end      
	  self.mainviewport:setplugindir(plugininfo)
	  if plugininfo then
      self:settitle(plugininfo:name())
	  end
  end
end

function machine:onactivated(viewport)  
  self.mainviewport:fillinstancecombobox()
  if self.project and self.project:pluginindex() ~= -1 then
    self.mainviewport:setpluginindex(self.project:pluginindex())
  end
end

function machine:ondeactivated()  
end

return machine
