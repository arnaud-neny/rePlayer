-- psycle plugineditor (c) 2016 by psycledelics
-- File: templateparser.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local templateparser = {}

function templateparser.prep(file, env)
  local output = ""
  local evalscript = ""
  for line in file:lines() do
    if string.find(line, "^@") then
      -- skip
    elseif string.find(line, "^#") then          
    else
      output = output..templateparser.replaceline(line, env)
    end    
  end
  return output
end

function templateparser.replaceline(line, env)
  local output = ""      
  local last = 1
  for text, expr, index in string.gmatch(line, "(.-)$(%b())()") do 
    last = index
    if text ~= "" then
      output = output..text
    end        
    local trimmedexp = expr:sub(2, -2)      
    local realvalue = env[trimmedexp]
    if not realvalue then
       output = output .. "#ERROR with ".."$("..expr..") and "..trimmedexp.."#ERROREND"
    else        
      output = output..realvalue
    end
  end    
  output = output..string.sub(line, last) .. "\n"      
  return output
end

function templateparser.work(templatepath, outputpath, env)     
    return templateparser.write(outputpath, 
      templateparser.processtemplate(templatepath, env))
end

function templateparser.evaluate(templatepath)
  local function findevalscript(file, var1) return templateparser.findevalscript(file) end
  local evalstr = templateparser.readfile(templatepath, findevalscript)  
  if evalstr == "" then
    return ""
  else
    return load(evalstr)()
  end
end

function templateparser.findevalscript(file)  
  local evalscript = ""
  for line in file:lines() do
    if string.find(line, "^@") then
      evalscript = evalscript .. line:sub(2, -1)         
    else
      -- skip
    end    
  end
  return evalscript
end

function templateparser.processtemplate(templatepath, env)
  local function parse(file, var1) return templateparser.prep(file, var1) end
  return templateparser.readfile(templatepath, parse, env)  
end

function templateparser.readfile(templatepath, fun, var1)
  local file, err = io.open(templatepath, "r")
  if file then
    result = fun(file, var1)
    file:close() 
  else
    result = nil
  end  
  return result, err
end

function templateparser.write(filepath, text)
  local file, err = io.open(filepath, "w")
  if file then
    file:write(text)
    file:close() 
  else
    return err
  end   
end

return templateparser