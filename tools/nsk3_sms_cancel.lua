_G.nsk3_sms_cancel=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_sms_delete.lua')
