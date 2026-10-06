-- Own NSM-3 physical input probe, not an injected UI event.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_staged_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
cpu.debug:bpset(0x307df4, nil,
    'logerror "8210_keypad_decoded: key=%02x\\n",r0;g')
local input = coroutine.create(function()
    if not emu.wait(12) then return end
    local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    machine:logerror('8210_menu_physical: press=1\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('8210_after_menu.png')
end)
_G.noki8210_menu_input = input
assert(coroutine.resume(input))
