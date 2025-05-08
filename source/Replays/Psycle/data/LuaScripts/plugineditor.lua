--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  plugineditor.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

function psycle.install()
  return 
[[
link = {
  label = "Plugin Editor",
  plugin = "plugineditor.lua",
  viewport = psycle.CHILDVIEWPORT,
  userinterface = psycle.MDI
}
psycle.addmenu("view", link)  
]]  
end

function psycle.info()
  local machinemodes = require("psycle.machinemodes")
  return { 
    vendor  = "psycle",
    name    = "Plugineditor",
    mode    = machinemodes.HOST,
    version = 0,
    api     = 0
  }
end

function psycle.start()  
  psycle.setmachine(require("plugineditor.machine"))
end
