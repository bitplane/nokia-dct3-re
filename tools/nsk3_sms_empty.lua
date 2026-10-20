_G.nsk3_sms_empty=true
_G.nsk3_sms_frames={16.5,19}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_sms_reload.lua')
