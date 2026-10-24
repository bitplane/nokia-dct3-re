-- Physical protected Erase confirmation; the firmware owns the record update.
_G.nsb3_fdn_option_steps = {
    {66, 'Scroll Down'}, {68, 'Left Softkey / Menu'},
    {70, _G.nsb3_fdn_erase_cancel and 'Right Softkey / C' or 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_record_options.lua')
