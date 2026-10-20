-- Physical dial/Send only; local permission is not network connectivity.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
_G.nsk3_sim_tx_limit=4096
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start=15
local digits={
    [1]={2,2,1},[2]={3,2,2},[3]={4,2,3},
    [4]={2,4,4},[5]={3,4,5},[6]={4,4,6}
}
_G.nsk3_followup_contacts={}
for _,digit in ipairs(_G.nsk3_fdn_dial_blocked and {5,4,3,2,1} or {1,2,3,4,6}) do
    _G.nsk3_followup_contacts[#_G.nsk3_followup_contacts+1]=digits[digit]
end
_G.nsk3_followup_contacts[#_G.nsk3_followup_contacts+1]={0,4,0x0e}
dofile(directory .. 'nsk3_security_input.lua')
local machine=manager.machine
local index=1
local times={23.6,23.8,24,26,28,30}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsk3_fdn_dial_'..times[index]..'.png')
        index=index+1
    end
end)
