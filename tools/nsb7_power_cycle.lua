-- Physical power-off/PWRONX wake with fresh SIM PIN and phone-code entry.
_G.nsb7_security_with_pin=true
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_page_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function at(time) assert(emu.wait(time-machine.time:as_double())) end
local function press(name,time,duration)
    at(time)
    machine:logerror(string.format('nsb7_power_input: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    fields[name]:set_value(1)
    assert(emu.wait(duration or 0.2))
    fields[name]:set_value(0)
    machine:logerror(string.format('nsb7_power_input: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
end
local input=coroutine.create(function()
    at(44)
    machine.screens[':screen']:snapshot('nsb7_power_ready.png')
    machine:logerror('nsb7_power: ready t=44.000000000\n')
    press('Power',45,4)
    at(55)
    machine.screens[':screen']:snapshot('nsb7_power_off.png')
    assert(_G.nsb7_radio_observer_reset)()
    press('Power',65,2)
    at(72)
    machine.screens[':screen']:snapshot('nsb7_power_pin.png')
    for index,digit in ipairs({'1','2','3','4'}) do press('Keypad '..digit,72+index) end
    press('Left Softkey / Menu',77)
    at(80)
    machine.screens[':screen']:snapshot('nsb7_power_security.png')
    for index,digit in ipairs({'1','2','3','4','5'}) do press('Keypad '..digit,80+index) end
    press('Left Softkey / Menu',86)
    at(90)
    machine.screens[':screen']:snapshot('nsb7_power_startup.png')
    press('Left Softkey / Menu',95)
    at(96)
    machine.screens[':screen']:snapshot('nsb7_power_menu.png')
    press('End',98)
    at(101)
    machine.screens[':screen']:snapshot('nsb7_power_final.png')
    machine:logerror('nsb7_power: done t=101.000000000\n')
    machine:exit()
end)
_G.nsb7_power_cycle=input
assert(coroutine.resume(input))
