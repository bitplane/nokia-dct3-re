-- Retest native supervision after retaining the own executable upload bank.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
_G.nsk3_long_input_script='nsk3_fdn_dial.lua'
dofile(directory .. 'nsk3_native_radio_observe.lua')
dofile(directory .. 'dct3_native_upload_observer.lua')('nsk3')
