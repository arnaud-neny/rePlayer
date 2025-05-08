-- scopes
-- register file for psycle's lua plugin system

function psycle.install()
  return [[
  link = {
    label = "Scopes",
    plugin = "scopes.lua",
    viewport = psycle.TOOLBARVIEWPORT,
    userinterface = psycle.SDI,
    creation = psycle.CREATEATSTART
  }
  psycle.addmenu("view", link)
]]
end


function psycle.info()
  local machinemodes = require("psycle.machinemodes")  
  return {
    vendor = "psycle",
    name = "scopes",
    mode = machinemodes.HOST,    
    version = 0,
    api = 0
  }
end

function psycle.start()  
  psycle.setmachine(require("scopes.machine"))
end

