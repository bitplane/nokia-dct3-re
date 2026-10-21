-- Cancellation is physical Back at the same confirmation boundary.
_G.nsb7_sms_delete_cancel=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_sms_delete_observe.lua')
