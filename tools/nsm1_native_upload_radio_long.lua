-- Same native scan observer, with explicit retention of the own upload bank.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
dofile(directory .. 'nsm1_native_radio_long.lua')
dofile(directory .. 'dct3_native_upload_observer.lua')('nsm1')
