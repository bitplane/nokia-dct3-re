-- Physical Options/PIN2/Edit, retaining the name and changing the last digit.
_G.nsk3_fdn_record_actions={
    {1,2,0x19},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19},
    {1,2,0x19},{1,2,0x19},{1,2,0x19},{1,16,0x1a},{4,4,6},{1,2,0x19}
}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_fdn_read.lua')
