-- Read the persisted FDN entry through the own physical menu path.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
_G.nsk3_sim_tx_limit=4096
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start=15
_G.nsk3_followup_contacts={
    {1,2,0x19},{2,4,4},{3,4,5},{4,2,3},{1,2,0x19},
    {1,8,0x18},{1,2,0x19},{1,2,0x19},{1,2,0x19},{1,2,0x19}
}
if _G.nsk3_fdn_empty then
    while #_G.nsk3_followup_contacts>9 do table.remove(_G.nsk3_followup_contacts) end
end
for _,cell in ipairs(_G.nsk3_fdn_record_actions or {}) do
    _G.nsk3_followup_contacts[#_G.nsk3_followup_contacts+1]=cell
end
dofile(directory .. 'nsk3_security_input.lua')
local machine=manager.machine
local index=1
local times={25,26,27,29,31,33,35,37,39,41}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsk3_fdn_read_'..times[index]..'.png')
        index=index+1
    end
end)
