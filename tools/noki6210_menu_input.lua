-- Product-local decoded keys, physical Menu only; no internal UI events.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
-- Own normal-key table loader at 4facfa; consumer observes decoded value.
cpu.debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('6210_before_menu.png')
    local key = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
    machine:logerror('6210_menu_physical: press=1\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6210_after_menu.png')
end)
_G.noki6210_menu_input = input
assert(coroutine.resume(input))
