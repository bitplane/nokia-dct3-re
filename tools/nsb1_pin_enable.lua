_G.nsb1_security_request_action='on'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_security_settings.lua')
