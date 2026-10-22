-- Physical FDN Settings navigation on an explicit laboratory card.
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
    machine:logerror(string.format('6210_fdn_input: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6210_fdn_input: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6210_fdn_key_'..when..'.png')
end
local runner=coroutine.create(function()
    for i=1,4 do press('Keypad '..i,7+i) end
    press('Left Softkey / Menu',12)
    press('Left Softkey / Menu',20)
    for i=0,2 do press('Scroll Down',22+i) end
    press('Left Softkey / Menu',26)
    for i=0,4 do press('Scroll Down',28+i) end
    press('Left Softkey / Menu',34)
    for i=0,1 do press('Scroll Down',36+i) end
    press('Left Softkey / Menu',40)
    local finish=50
    if _G.npe3_fdn_record_steps then
        for _,step in ipairs(_G.npe3_fdn_record_steps) do press(step[2],step[1]) end
        finish=assert(_G.npe3_fdn_record_finish)
    elseif _G.npe3_fdn_enable or _G.npe3_fdn_disable then
        press(_G.npe3_fdn_disable and 'Scroll Down' or 'Scroll Up',42)
        press('Left Softkey / Menu',44)
        local pin=_G.npe3_fdn_pin2 or '5678'
        for i=1,4 do press('Keypad '..pin:sub(i,i),46+i) end
        press('Left Softkey / Menu',51)
        finish=58
    end
    press('End',finish)
    press('Left Softkey / Menu',finish+2)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6210_fdn_then_menu.png')
    machine:logerror('6210_fdn_input: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_fdn_runner=runner
assert(coroutine.resume(runner))
