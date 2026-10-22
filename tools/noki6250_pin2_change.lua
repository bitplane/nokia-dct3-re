-- Own NHM-3 Settings list: Security is four Down presses, not five.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_runtime_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(key,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[key])
    machine:logerror(string.format('6250_pin2_input: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_pin2_input: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6250_pin2_key_'..when..'.png')
end
local runner=coroutine.create(function()
    for index=1,4 do press('Keypad '..index,7+index) end
    press('Left Softkey / Menu',12)
    press('Left Softkey / Menu',20)
    for second=22,24 do press('Scroll Down',second) end
    press('Left Softkey / Menu',26)
    for second=28,31 do press('Scroll Down',second) end
    press('Left Softkey / Menu',34)
    for second=36,39 do press('Scroll Down',second) end
    press('Left Softkey / Menu',42)
    for second=44,45 do press('Scroll Down',second) end
    press('Left Softkey / Menu',47)
    local old,new=_G.nhm3_pin2_old or '5678',_G.nhm3_pin2_new or '8765'
    for _,phase in ipairs(_G.nhm3_pin2_phases or {{50,old},{58,new},{66,new}}) do
        for index=1,#phase[2] do press('Keypad '..phase[2]:sub(index,index),phase[1]+index-1) end
        press('Left Softkey / Menu',phase[1]+#phase[2])
    end
    local finish=_G.nhm3_pin2_finish or 76
    press('End',finish)
    press('Left Softkey / Menu',finish+2)
    machine:logerror('6250_pin2_input: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_pin2_change=runner
assert(coroutine.resume(runner))
