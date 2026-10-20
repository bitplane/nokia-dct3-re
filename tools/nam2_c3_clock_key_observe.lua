-- Physical wake-source control: press during the first observed sleep interval.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nam2_c3_clock_observe.lua')
local machine=manager.machine
local key=assert(machine.ioport.ports[':COL.2'].fields['Keypad 1'])
local contact=coroutine.create(function()
    if not emu.wait(8.95) then return end
    key:set_value(1)
    machine:logerror('nam2_clock_early_key: down=1 t=8.95 physical_only=1\n')
    if not emu.wait(0.2) then key:set_value(0); return end
    key:set_value(0)
    machine:logerror('nam2_clock_early_key: down=0 t=9.15 physical_only=1\n')
end)
_G.nam2_clock_early_contact=contact
assert(coroutine.resume(contact))
