-- Preserve name A and physically replace the number's last digit5 with6.
_G.nsb3_fdn_option_steps = {
    {66, 'Left Softkey / Menu'}, {68, 'Left Softkey / Menu'},
    {70, 'Right Softkey / C'}, {72, 'Keypad 6'}, {74, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_record_options.lua')
