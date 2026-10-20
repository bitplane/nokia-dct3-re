_G.nsb1_startup_pin_contacts={4,3,2,1,6}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_pin_reload.lua')
