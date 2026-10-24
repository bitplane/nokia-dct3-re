-- Physical fixed-dialling settings; the card owns ADN validity.
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
        machine:logerror(string.format('8850_fdn: key=%s state=%d t=%.9f\n',
            name,state,machine.time:as_double()))
        field:set_value(state)
        assert(emu.wait(state==1 and 0.15 or 0.85))
    end
    machine.screens[':screen']:snapshot('8850_fdn_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(24))
    press('Menu','menu')
    for i=1,3 do press('Scroll Down','main_'..i) end
    press('Menu','settings')
    for i=1,5 do press('Scroll Down','setting_'..i) end
    press('Menu','security')
    for i=1,2 do press('Scroll Down','security_'..i) end
    press('Menu','current')
    local finish=48
    if _G.nsm2_fdn_record_steps then
        for _,step in ipairs(_G.nsm2_fdn_record_steps) do press(step[1],step[2]) end
        finish=assert(_G.nsm2_fdn_record_finish)
    elseif _G.nsm2_fdn_toggle then
        press(_G.nsm2_fdn_off and 'Scroll Down' or 'Scroll Up','selected')
        press('Menu','pin2')
        for i,digit in ipairs({5,6,7,8}) do press('Keypad '..digit,'pin2_'..i) end
        press('Menu','result')
    end
    assert(emu.wait(finish-machine.time:as_double()))
    press('End','return')
    press('Menu','responsive_menu')
    machine:logerror('8850_fdn: complete=1 firmware_state_forcing=0\n')
end)
_G.noki8850_fdn=input
assert(coroutine.resume(input))
