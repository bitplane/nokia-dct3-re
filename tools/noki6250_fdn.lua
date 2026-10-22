-- Own NHM-3 fixed-dialling menu on an explicit laboratory FDN card.
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
    machine:logerror(string.format('6250_fdn: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_fdn: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6250_fdn_'..when..'.png')
end
local runner=coroutine.create(function()
    for index=1,4 do press('Keypad '..index,7+index) end
    press('Left Softkey / Menu',12)
    press('Left Softkey / Menu',20)
    for second=22,24 do press('Scroll Down',second) end
    press('Left Softkey / Menu',26)
    for second=28,31 do press('Scroll Down',second) end
    press('Left Softkey / Menu',34)
    for second=36,37 do press('Scroll Down',second) end
    press('Left Softkey / Menu',40)
    if _G.nhm3_fdn_record_steps then
        for _,step in ipairs(_G.nhm3_fdn_record_steps) do press(step[2],step[1]) end
        local finish=assert(_G.nhm3_fdn_record_finish)
        press('End',finish)
        press('Left Softkey / Menu',finish+2)
    elseif _G.nhm3_fdn_toggle then
        press(_G.nhm3_fdn_disable and 'Scroll Down' or 'Scroll Up',42)
        press('Left Softkey / Menu',44)
        local pin=_G.nhm3_fdn_pin2 or '5678'
        for index=1,4 do press('Keypad '..pin:sub(index,index),46+index) end
        press('Left Softkey / Menu',51)
        press('End',58)
        press('Left Softkey / Menu',60)
    else
        press('End',50)
        press('Left Softkey / Menu',52)
    end
    machine:logerror('6250_fdn: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_fdn=runner
assert(coroutine.resume(runner))
