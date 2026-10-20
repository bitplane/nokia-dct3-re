-- Physical FDN Numbers/Search/List/Details on an independently cold card.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsm1_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{4,2,3},{1,2,0x19},
    {1,8,0x18},{1,2,0x19},{1,2,0x19},{1,2,0x19},{1,2,0x19}}
for _,cell in ipairs(_G.nsm1_fdn_record_actions or {}) do
    _G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]=cell
end
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local index=1
local times={26,27,28,29,30,32,34,35,36,38,40,42}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_fdn_read_'..times[index]..'.png')
        index=index+1
    end
end)
