local function recursivenodeiterator(node)
  local i = {-1}
  local level_ = 1
  local n = node 
  return 
  { 
    level = function() return level_ - 1 end,
    next = function()
            i[level_] = i[level_] + 1
            if i[level_] == 0 then return n end
            if i[level_] <= n:size() then
              local child = n:at(i[level_])  
              if child:size() > 0 then
                level_ = level_ + 1
                i[level_] = 0
                n = child                
              end                 
              return child
            elseif level_ > 1 then
              level_ = level_ - 1              
              i[level_] = i[level_] + 1
              if i[level_] <= n:parent():size() then
                n = n:parent()
                return n:at(i[level_])
              end
            end
          end                                     
  }
end

return recursivenodeiterator