_G.nsb1_sms_empty = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb1_sms_draft_reload.lua')
