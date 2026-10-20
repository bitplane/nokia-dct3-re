-- Own physical input/card fixture, not a donor memory or DSP state fixture.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
dofile(directory .. 'nsk3_fdn_dial.lua')
dofile(directory .. 'dct3_native_upload_observer.lua')('nsk3')
