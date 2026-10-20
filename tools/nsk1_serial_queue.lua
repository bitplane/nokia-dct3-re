-- Read-only serial/RF separation probe on the initialized native fixture.
local directory = debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsk1_scan_qualification.lua')
dofile(directory .. 'c54x_rom4_port36_observe.lua')
