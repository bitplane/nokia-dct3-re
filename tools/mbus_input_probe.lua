-- External byte events, not frame-sampled keys or writes to firmware/MMIO.
local machine = manager.machine
local port = assert(machine.ioport.ports[':MBUS_RX'])
-- Keep repeats inside a complete frame: an unexpected first byte can make
-- the firmware disable reception. The test payload has no accepted service semantics.
local bytes = _G.nokia_mbus_probe_bytes or
    {0x1f, 0, 0x10, 0xd1, 0, 6, 0xa5, 0xa5, 0xff, 0xff, 0, 0, 0, 0xd8}
local start = _G.nokia_mbus_probe_start or 8
local runner = coroutine.create(function()
    assert(emu.wait(start))
    local strobe = 0x100
    for index, byte in ipairs(bytes) do
        machine:logerror(string.format('mbus_input_probe: index=%d data=%02x t=%.9f\n',
            index, byte, machine.time:as_double()))
        port:write(byte | strobe, 0x1ff)
        strobe = strobe ~ 0x100
        -- Above a 9600-baud character, below the existing MBUSTIM idle period.
        assert(emu.wait(0.0011))
    end
    machine:logerror('mbus_input_probe: complete\n')
end)
_G.nokia_mbus_input_probe = runner
assert(coroutine.resume(runner))
