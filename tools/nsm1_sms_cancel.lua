_G.nsm1_sms_cancel=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_sms_delete.lua')
