_G.nsb3_phonebook_extra_steps = {
    {52, 'Scroll Down'}, {55, 'Left Softkey / Menu'}, {58, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonebook_options.lua')
