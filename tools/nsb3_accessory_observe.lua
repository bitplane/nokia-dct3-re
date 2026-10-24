-- Own raw accessory inputs and firmware state only; never supplies a result.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_correct.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local count = 0
_G.nsb3_accessory_state = memory:install_write_tap(0x10c3c8, 0x10c3cb, 'nsb3_accessory_state', function(_, data, mask)
    if mask & 255 == 0 then return end
    count = count + 1
    if count <= 32 then
        machine:logerror(string.format('nsb3_accessory_state: value=%02x pc=%08x t=%.9f observer_writes=0\n', data & 255, cpu.state.PC.value, machine.time:as_double()))
    end
end)
local samples = coroutine.create(function()
    for _, time in ipairs({5, 10, 24, 36, 40}) do
        assert(emu.wait(time - machine.time:as_double()))
        machine:logerror(string.format('nsb3_accessory_sample: second=%d state=%02x sample=%04x external=%02x t=%.9f observer_writes=0\n', time, memory:read_u8(0x10c3cb), memory:read_u16(0x10c3d6), memory:read_u8(0x2000e), machine.time:as_double()))
        machine.screens[':screen']:snapshot(string.format('nsb3_accessory_%02d.png', time))
    end
end)
assert(coroutine.resume(samples))
