-- Physical number/Send comparison; no network or native speech claim.
local source=debug.getinfo(1,'S').source:sub(2)
local digits={
    [1]={2,2,1},[2]={3,2,2},[3]={4,2,3},[4]={2,4,4},[5]={3,4,5},[6]={4,4,6}
}
local number=_G.nsm1_fdn_dial_blocked and {5,4,3,2,1} or {1,2,3,4,6}
_G.nsm1_followup_contacts={}
for _,digit in ipairs(number) do
    _G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]=digits[digit]
end
_G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]={0,4,0x0e}
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local index=1
local times={24,26,28,30,32,36}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_fdn_dial_'..times[index]..'.png')
        index=index+1
    end
end)
