-- Own NSM-3 physical input probe, not an injected UI event.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_staged_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
-- Acceptance needs decoded keys and readiness; journal reconstruction is static.
cpu.debug:bpset(0x307df4, nil,
    'logerror "8210_keypad_decoded: key=%02x\\n",r0;g')
cpu.debug:bpset(0x2885ac, 'r0==1 && temp1<128',
    'temp1=temp1+1;logerror "8210_startup_post: code=%04x caller=%08x\\n",r1,r14;g')
cpu.debug:bpset(0x24a8e4, 'temp8<64',
    'temp8=temp8+1;logerror "8210_readiness_input: code=%02x caller=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x24a9b0, 'temp7<64',
    'temp7=temp7+1;logerror "8210_readiness_flags: values=%02x%02x%02x%02x%02x%02x%02x%02x%02x\\n",b@137e44,b@137e45,b@137e46,b@137e47,b@137e48,b@137e49,b@137e4a,b@137e4b,b@137e4c;g')
local input = coroutine.create(function()
    if _G.noki8210_observe_only then return end
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
