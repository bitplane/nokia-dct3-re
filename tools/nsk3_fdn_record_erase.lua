-- Physical Options/PIN2/Erase, with an independent cancellation control.
_G.nsk3_fdn_record_actions={
    {1,2,0x19},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19},
    {1,2,0x19},{1,8,0x18},{1,2,0x19},
    _G.nsk3_fdn_erase_cancel and {1,16,0x1a} or {1,2,0x19}
}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_fdn_read.lua')
