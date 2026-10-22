-- Product-local PIN2 change through explicit scrolling and real contacts.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name])
    machine:logerror(string.format('6210_pin2_input: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6210_pin2_input: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6210_pin2_key_'..when..'.png')
end
local runner=coroutine.create(function()
    for i=1,4 do press('Keypad '..i,7+i) end
    press('Left Softkey / Menu',12)
    press('Left Softkey / Menu',20)
    for i=0,2 do press('Scroll Down',22+i) end
    press('Left Softkey / Menu',26)
    for i=0,4 do press('Scroll Down',28+i) end
    press('Left Softkey / Menu',34)
    -- FDN is absent on this card profile; Access codes is the fifth item.
    for i=0,3 do press('Scroll Down',36+i) end
    press('Left Softkey / Menu',42)
    for i=0,1 do press('Scroll Down',44+i) end
    press('Left Softkey / Menu',47)
    local old,new=_G.npe3_pin2_old or '5678',_G.npe3_pin2_new or '8765'
    for _,phase in ipairs(_G.npe3_pin2_phases or {{50,old},{58,new},{66,new}}) do
        for i=1,#phase[2] do press('Keypad '..phase[2]:sub(i,i),phase[1]+i-1) end
        press('Left Softkey / Menu',phase[1]+#phase[2])
    end
    local finish=_G.npe3_pin2_finish or 76
    press('End',finish)
    press('Left Softkey / Menu',finish+2)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6210_security_then_menu.png')
    machine:logerror('6210_pin2_input: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_pin2_change_runner=runner
assert(coroutine.resume(runner))
