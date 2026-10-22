-- Own NHM-3 Change access codes -> Change PIN code, physical contacts only.
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
    machine:logerror(string.format('6250_pin_change: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_pin_change: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6250_pin_change_'..when..'.png')
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
    press('Scroll Down',44)
    press('Left Softkey / Menu',46)
    for _,phase in ipairs({{50,'1234'},{58,'2468'},{66,'2468'}}) do
        for index=1,4 do press('Keypad '..phase[2]:sub(index,index),phase[1]+index-1) end
        press('Left Softkey / Menu',phase[1]+4)
    end
    press('End',76)
    press('Left Softkey / Menu',78)
    machine:logerror('6250_pin_change: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_pin_change=runner
assert(coroutine.resume(runner))
