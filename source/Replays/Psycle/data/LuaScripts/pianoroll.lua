-- pianoroll
-- register file for psycle's lua plugin system

function psycle.install()
  return [[
  link = {
    label = "Pianoroll",
    plugin = "pianoroll.lua",
    viewport = psycle.CHILDVIEWPORT,
    userinterface = psycle.SDI
  }
  psycle.addmenu("view", link)
]]
end


function psycle.info()
  local machinemodes = require("psycle.machinemodes")  
  return {
    vendor = "psycle",
    name = "pianoroll",
    mode = machinemodes.HOST,
    version = 0,
    api = 0
  }
end

function psycle.start()  
  psycle.setmachine(require("pianoroll.machine"))
end

