_G.nsm1_fdn_erase_cancel=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_fdn_record_erase.lua')
