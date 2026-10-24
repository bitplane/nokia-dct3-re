-- Physical Numbers/Search/empty query/Details; never supplies a record.
_G.nsb3_fdn_record_steps = {
    {79, 'Left Softkey / Menu'}, {82, 'Left Softkey / Menu'}, {85, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_records_input.lua')
