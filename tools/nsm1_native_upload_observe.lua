-- Retain the qualified physical dial while observing own native uploads.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
dofile(directory .. 'nsm1_fdn_dial.lua')
dofile(directory .. 'dct3_native_upload_observer.lua')('nsm1')
