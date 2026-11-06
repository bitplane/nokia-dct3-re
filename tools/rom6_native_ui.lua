-- Physical NSM-3 input with the captured native DSP, not HLE completion.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_observe.lua')
local machine=manager.machine
local arm=machine.devices[':maincpu']
machine.debugger:command('temp5=0')
arm.debug:bpset(0x307df4,'temp5<40',
    'temp5=temp5+1;logerror "rom6_native_key: decoded=%02x observer_writes=0\\n",r0;g')
local function press(column,name)
    local key=assert(machine.ioport.ports[':COL.'..column].fields[name])
    machine:logerror(string.format('rom6_native_physical: key=%s t=%.6f\n',name,machine.time:as_double()))
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.35)
end
local input=coroutine.create(function()
    if not emu.wait(12) then return end
    machine.screens[':screen']:snapshot('rom6_native_security.png')
    for _,key in ipairs({{2,'Keypad 1'},{3,'Keypad 2'},{4,'Keypad 3'},
            {2,'Keypad 4'},{3,'Keypad 5'},{1,'Menu'}}) do
        if not press(key[1],key[2]) then return end
    end
    if not emu.wait(5) then return end
    machine.screens[':screen']:snapshot('rom6_native_unlocked.png')
    if not press(1,'Menu') or not emu.wait(2) then return end
    machine.screens[':screen']:snapshot('rom6_native_menu.png')
    if not emu.wait(32) or not press(1,'Scroll Down') or not emu.wait(2) then return end
    machine.screens[':screen']:snapshot('rom6_native_late_menu.png')
end)
_G.rom6_native_input=input
assert(coroutine.resume(input))
