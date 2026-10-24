-- Product UI sequence is fixture policy; credentials change only through APDUs.
_G.noki8850_security_only=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_security_input.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,label)
    local field=assert(fields[name])
    for _,state in ipairs({1,0}) do
        machine:logerror(string.format('8850_pin2: key=%s state=%d t=%.9f\n',
            name,state,machine.time:as_double()))
        field:set_value(state)
        assert(emu.wait(state==1 and 0.15 or 0.85))
    end
    machine.screens[':screen']:snapshot('8850_pin2_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(24))
    press('Menu','menu')
    for i=1,3 do press('Scroll Down','main_'..i) end
    press('Menu','settings')
    for i=1,5 do press('Scroll Down','setting_'..i) end
    press('Menu','security')
    for i=1,5 do press('Scroll Down','security_'..i) end
    press('Menu','access_codes')
    for i=1,2 do press('Scroll Down','code_'..i) end
    press('Menu','current_pin2')
    if _G.nsm2_permanent_puk2 then
        assert(emu.wait(46-machine.time:as_double()))
        press('Keypad 9','permanent_digit')
        assert(emu.wait(48-machine.time:as_double()))
        press('End','return')
        press('Menu','responsive_menu')
        machine:logerror('8850_pin2: complete=1 firmware_state_forcing=0\n')
        return
    end
    local old=_G.nsm2_pin2_reverse and {2,4,6,8} or {5,6,7,8}
    if _G.nsm2_pin2_wrong then old={9,9,9,9} end
    if _G.nsm2_puk2_recovery then old={8,7,6,5,4,3,2,1} end
    if _G.nsm2_wrong_puk2 then old={9,9,9,9,9,9,9,9} end
    local new=_G.nsm2_pin2_reverse and {5,6,7,8} or {2,4,6,8}
    for group,digits in ipairs({old,new,new}) do
        for index,digit in ipairs(digits) do press('Keypad '..digit,'digits_'..group..'_'..index) end
        press('Menu',({'new_pin2','confirm','result'})[group])
    end
    assert(emu.wait((_G.nsm2_puk2_recovery and 67 or 62)-machine.time:as_double()))
    press('End','return')
    press('Menu','responsive_menu')
    machine:logerror('8850_pin2: complete=1 firmware_state_forcing=0\n')
end)
_G.noki8850_pin2_change=input
assert(coroutine.resume(input))
