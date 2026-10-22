-- Own NHM-3 Security settings -> PIN code request, physical contacts only.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_runtime_observe.lua')
local machine=manager.machine
local enable=_G.nhm3_pin_request_enable or false
local cold=_G.nhm3_pin_request_cold or false
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(key,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[key])
    machine:logerror(string.format('6250_pin_request: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_pin_request: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6250_pin_request_'..when..'.png')
end
local runner=coroutine.create(function()
    assert(emu.wait(cold and 14 or 8))
    machine.screens[':screen']:snapshot('6250_pin_request_boot.png')
    if not enable then
        for index=1,4 do press('Keypad '..index,(cold and 13 or 7)+index) end
        press('Left Softkey / Menu',cold and 18 or 12)
    end
    assert(emu.wait((cold and 24 or 18)-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_pin_request_startup.png')
    press('Left Softkey / Menu',cold and 28 or 20)
    if not cold then
        for second=22,24 do press('Scroll Down',second) end
        press('Left Softkey / Menu',26)
        for second=28,31 do press('Scroll Down',second) end
        press('Left Softkey / Menu',34)
        press('Left Softkey / Menu',36)
        for index=1,4 do press('Keypad '..index,37+index) end
        press('Left Softkey / Menu',42)
        press(enable and 'Scroll Up' or 'Scroll Down',44)
        press('Left Softkey / Menu',46)
        press('End',50)
        press('Left Softkey / Menu',52)
    end
    machine:logerror('6250_pin_request: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_pin_request=runner
assert(coroutine.resume(runner))
