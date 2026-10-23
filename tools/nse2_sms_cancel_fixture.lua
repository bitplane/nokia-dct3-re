_G.nse2_sms_action='cancel'
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_sms_reload_fixture.lua')
