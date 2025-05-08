local cfg = require("psycle.config"):new("PatternVisual")
local node = require("psycle.node")
local orderedtable = require("psycle.orderedtable")
local property = require("psycle.property")
local filehelper = require("psycle.file")
local listener = require("psycle.listener")
local serpent = require("psycle.serpent")
local recursivenodeiterator = require("psycle.recursivenodeiterator")

local settingsmanager = {}

local endl = "\n"

function settingsmanager:new(pluginname)  
  local c = {}
  setmetatable(c, self)
  self.__index = self
  c:init(pluginname)
  return c
end

function settingsmanager:init(pluginname)
  self.listener_ = listener:new("onapplysetting")
  self.pluginname_ = pluginname
  self:loadglobal()
  self:load(filenamewithoutextension(self.globalsetting_.settingname))  
end

function filenamewithoutextension(filename)
  local result = ""
  local found, len, remainder = string.find(filename, "^(.*)%.[^%.]*$")
  if found then
    result = remainder
  else
    result = path
  end
  return result		
end

function settingsmanager:psycleconfig()
  local result = node:new():settext("Psycle"):setname("psycle")
  local dirs = node:new(result):settext("Directories"):setname("dirs")    
  dirs.properties = orderedtable.new()
  dirs.properties.lua = property:new(cfg:luapath(), "Lua scripts directory")
  dirs.properties.preset = property:new(cfg:presetpath(), "User preset directory")
  dirs.properties.icons = property:new(cfg:luapath() .. "\\psycle\\ui\\icons\\", "Standard Icons")
  return result
end

function settingsmanager:apply()
  self.listener_:notify(self.setting_)
end

function settingsmanager:addlistener(listener)
  self.listener_:addlistener(listener)
end

function settingsmanager:find(key)
  local result = nil
  for element in recursivenodeiterator(self.setting_).next do
    if element:name() == key then
      result = element
      break
    end
  end
  return result
end

function settingsmanager:replaceline(line)
  local output = ""      
  local last = 1
  local err = false
  for text, expr, index in string.gmatch(line, "(.-)$(%b())()") do 
    last = index
    if text ~= "" then
      output = output .. text
    end             
    local trimmedexp = expr:sub(2, -2)      
    local realvalue = self:dir(trimmedexp)
    if not realvalue then
       err = true
       break
    else        
      output = output .. realvalue
    end
  end    
  output = output .. string.sub(line, last)    
  return output, err
end

function settingsmanager:dir(key)
  local result = nil
  local node = self:find("dirs")
  if node then
    if not node.properties[key] then
      psycle.alert("Setting : Directory " .. key .. " not found.")
    else
      result = self:replaceline(node.properties[key]:value())
    end
  end
  return result
end

function settingsmanager:loadglobal()
  local path = cfg:presetpath().."\\" .. self.pluginname_ .. "\\global.lua"    
  local f = loadfile(path)
  if f then
    self.globalsetting_ = f()
  else
    local path = cfg:presetpath()
    filehelper.mkdir(path .. "\\" .. self.pluginname_)  
    self.globalsetting_ = require(self.pluginname_ .. ".global")
	  self:saveglobal("default")    
  end
  return self
end

function settingsmanager:saveglobal(name)
  self.globalsetting_.settingname = name .. ".lua"
  local endl = "\n"
  local path = cfg:presetpath() .. "\\" .. self.pluginname_ .. "\\global.lua"
  local file,err = io.open(path, "wb")
  if err then
    return err 
  end
  file:write([[
local global = {
   settingname = "]] .. self.globalsetting_.settingname .. [["
 }
 
return global
]])    
  return self
end

function settingsmanager:setting()
  return self.setting_
end

function settingsmanager:pluginname()
  return self.pluginname_
end

function settingsmanager:settingname()  
  return filenamewithoutextension(self.globalsetting_.settingname)
end

function settingsmanager:path()
  return cfg:presetpath() .. "\\" .. self.pluginname_ .. "\\" .. self.globalsetting_.settingname
end

function settingsmanager:load(name)  
  if name .. ".lua"  ~= self.globalsetting_.settingname then     
    self:saveglobal(name)
  end  
  local f, err = loadfile(self:path())
  if f then
    self.setting_ = f()
    local default = self:loaddefault()
    assert(default)
	  if default.meta.version ~= self.setting_.meta.version then	  
      psycle.alert("New setting version. Overwrite old.")
	    self:copydefault()
      self.setting_ = dofile(self:path())
    end	
  else
    self:copydefault()	
	  self.setting_ = dofile(self:path())	
  end
  self.setting_:insertfront(self:psycleconfig())
  return self
end

function settingsmanager:loaddefault()
  local result = nil
  local default_path = cfg:luapath() .. "\\" .. self.pluginname_ .. "\\defaultsetting.lua"  
  local f = loadfile(default_path)
  if f then
    result = f()
  end
  return result
end

function settingsmanager:restoredefault()  
  self.setting_ = require(self.pluginname_ .. ".defaultsetting")  
  self.globalsetting_.settingname = "default.lua"  
  self:save()  
  self:saveglobal("default")
end

function settingsmanager:copydefault()
  local path = cfg:presetpath()
  filehelper.mkdir(path .. "\\" .. self.pluginname_) 
  local default_path = cfg:luapath() .. "\\" .. self.pluginname_ .. "\\defaultsetting.lua"  
  infile = io.open(default_path, "r")
  instr = infile:read("*a")
  infile:close()
  self.globalsetting_.settingname = "default.lua"
  outfile = io.open(self:path(), "w")
  outfile:write(instr)
  outfile:close()
end

function settingsmanager:saveas(name)  
  self:saveglobal(name)
  self:save()  
  self.globalsetting_.settingname = name .. "lua"
end

function settingsmanager:save()
  local setting = self:setting()  
  local file,err = io.open(self:path(), "wb")
  if err then
    return err 
  end
  file:write([[
local node = require("psycle.node")  
local orderedtable = require("psycle.orderedtable")
local fontinfo = require("psycle.ui.fontinfo")
local stock = require("psycle.stock")
local property = require("psycle.property")

local setting = node:new():setname("setting")  

setting.meta = {
  name = "]] .. self.setting_.meta.name .. [[",
  version = ]] .. self.setting_.meta.version .. 
[[

}

]])
  self:saverec(file, "settings", setting)
  file:write([[
return setting
 ]] .. endl)
 file.close()
 return self
end

function settingsmanager:saverec(file)
  local ispsyclesection = false
  local psyclelevel = 0
  for section in recursivenodeiterator(self.setting_).next do    
    if section:level() > 0 then 
      if ispsyclesection and psyclelevel == section:level() then
        ispsyclesection = false
      elseif section:name() == "psycle"  then
        ispsyclesection = true
        psyclelevel = section:level()        
      end      
      if not ispsyclesection then
        file:write("local " .. section:name() .. " = node:new(" 
                   .. section:parent():name()
                   .. "):setname('" .. section:name()
                   .. "'):settext('" .. section:text() .. "')"
                   .. endl)
        if (section.properties) then
          local prefix = section:name() .. ".properties"
          file:write(prefix .. " = orderedtable.new()" .. endl)          
          for name, property in section.properties:opairs() do		  		  
            file:write(prefix .. "." .. name .. ' = ')            
            property:write(file)
            file:write(endl)		  
          end
        end
      end
      file:write(endl)
    end
  end
  return result
end

return settingsmanager
