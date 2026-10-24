_G.nsb3_fdn_record_wrong = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_record_write.lua')
