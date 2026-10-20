-- Physical startup authentication and candidate Change PIN2 menu path.
local source=debug.getinfo(1,'S').source:sub(2)
local keys={{1,2,0x19},{2,4,4},{3,4,5},{4,4,6},{1,2,0x19},{1,8,0x18},{1,8,0x18},{1,2,0x19}}
local digits={
    [1]={2,2,1},[2]={3,2,2},[3]={4,2,3},[4]={2,4,4},
    [5]={3,4,5},[6]={4,4,6},[7]={2,8,7},[8]={3,8,8}
}
local old=_G.nsm1_pin2_old or {5,6,7,8}
local new=_G.nsm1_pin2_new or {8,7,6,5}
for _,credential in ipairs({old,new,new}) do
    for _,digit in ipairs(credential) do keys[#keys+1]=assert(digits[digit]) end
    keys[#keys+1]={1,2,0x19}
end
_G.nsm1_followup_contacts=keys
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local index=1
local times={25,27,30,35,40,45,47}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_pin2_'..times[index]..'.png')
        index=index+1
    end
end)
