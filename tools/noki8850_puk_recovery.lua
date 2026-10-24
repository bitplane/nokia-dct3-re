-- Physical PUK1 recovery, new PIN confirmation and ordinary phone-code entry.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_startup_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name])
    for _,state in ipairs({1,0}) do
        machine:logerror(string.format('8850_puk_input: key=%s state=%d t=%.9f\n',
            name,state,machine.time:as_double()))
        field:set_value(state)
        assert(emu.wait(state==1 and 0.15 or 0.85))
    end
end
local function capture(label)
    machine.screens[':screen']:snapshot('8850_puk_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(7))
    capture('prompt')
    for i=1,8 do press('Keypad '..(_G.nsm2_wrong_puk and 9 or i),7+i) end
    press('Menu',16)
    capture('new_pin')
    for group=0,1 do
        for i,digit in ipairs({2,4,6,8}) do press('Keypad '..digit,19+group*8+i) end
        press('Menu',24+group*8)
        capture(group==0 and 'confirm' or 'result')
    end
    if _G.nsm2_wrong_puk then
        if _G.nsm2_permanent_puk then
            press('End',38)
            capture('permanent_after')
        end
        machine:logerror('8850_puk_input: '..(_G.nsm2_permanent_puk and 'permanent_complete' or 'wrong_complete')..
            '=1 firmware_state_forcing=0\n')
        return
    end
    for i,digit in ipairs({1,2,3,4,5}) do press('Keypad '..digit,39+i) end
    press('Menu',45)
    assert(emu.wait(49-machine.time:as_double()))
    capture('pre_menu')
    press('Menu',50)
    capture('menu')
    machine:logerror('8850_puk_input: complete=1 firmware_state_forcing=0\n')
end)
_G.noki8850_puk_input=input
assert(coroutine.resume(input))
