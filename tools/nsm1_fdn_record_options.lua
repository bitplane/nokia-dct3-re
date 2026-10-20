_G.nsm1_fdn_record_actions={{1,2,0x19},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19}}
if _G.nsm1_fdn_erase then
    for _,cell in ipairs({{1,8,0x18},{1,2,0x19},_G.nsm1_fdn_erase_cancel and {1,16,0x1a} or {1,2,0x19}}) do
        _G.nsm1_fdn_record_actions[#_G.nsm1_fdn_record_actions+1]=cell
    end
elseif _G.nsm1_fdn_edit then
    for _,cell in ipairs({{1,2,0x19},{1,2,0x19},{1,16,0x1a},{4,4,6},{1,2,0x19}}) do
        _G.nsm1_fdn_record_actions[#_G.nsm1_fdn_record_actions+1]=cell
    end
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_fdn_read.lua')
