_G.nsb3_fdn_fast_navigation = true
_G.nsb3_fdn_record_steps = {{54, 'Left Softkey / Menu'}, {56, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_records_input.lua')
