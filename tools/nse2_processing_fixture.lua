-- Reuse the ROM4 passive processing census with the qualified physical UI path.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
dofile(directory .. 'c54x_rom4_processing_observe.lua')
