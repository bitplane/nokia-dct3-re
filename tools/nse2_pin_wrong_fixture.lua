_G.nse2_pin_value='1111'
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_pin_fixture.lua')
