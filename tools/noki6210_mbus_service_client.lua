-- Observe firmware TX echoes and acknowledge its version response on the wire.
local source = debug.getinfo(1, 'S').source:sub(2)
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces.program
local port = assert(machine.ioport.ports[':MBUS_RX'])
local buffer, pending = {}, nil
local function hex(bytes)
    local result = {}
    for _, byte in ipairs(bytes) do result[#result + 1] = string.format('%02x', byte) end
    return table.concat(result)
end
_G.nokia_mbus_service_tap = memory:install_read_tap(0x20018, 0x2001b,
    'mbus_service_tx_echo', function(_, value, mask)
        if machine.time:as_double() < 20 or mask & 0xff00 == 0 then return end
        -- Control reads are side-effect free; the mask guard prevents recursion.
        if memory:read_u8(0x20018) & 0x20 == 0 then return end
        local byte = (value >> 8) & 0xff
        machine:logerror(string.format('mbus_service_wire: data=%02x t=%.9f\n', byte, machine.time:as_double()))
        if #buffer == 0 and byte ~= 0x1f then return end
        buffer[#buffer + 1] = byte
        local size
        if #buffer >= 4 and buffer[4] == 0x7f then size = 6
        elseif #buffer >= 6 then size = 8 + buffer[5] * 256 + buffer[6] end
        assert(not size or size <= 512, 'oversized service response')
        if size and #buffer == size then
            local checksum = 0
            for _, item in ipairs(buffer) do checksum = checksum ~ item end
            assert(checksum == 0, 'invalid service response checksum')
            machine:logerror('mbus_service_frame: hex=' .. hex(buffer) .. '\n')
            if buffer[2] == 0x10 and buffer[3] == 0 and buffer[4] == 0xd2 then
                assert(not pending, 'duplicate unacknowledged service response')
                local sequence = buffer[#buffer - 1]
                pending = {0x1f, buffer[3], buffer[2], 0x7f, sequence,
                    0x1f ~ buffer[3] ~ buffer[2] ~ 0x7f ~ sequence}
            end
            buffer = {}
        end
    end)
local runner = coroutine.create(function()
    while not pending do assert(emu.wait(0.0005)) end
    assert(emu.wait(0.003))
    local strobe = 0x100
    for index, byte in ipairs(pending) do
        machine:logerror(string.format('mbus_service_ack: index=%d data=%02x t=%.9f\n', index, byte, machine.time:as_double()))
        port:write(byte | strobe, 0x1ff)
        strobe = strobe ~ 0x100
        assert(emu.wait(0.0011))
    end
    machine:logerror('mbus_service_ack: complete\n')
end)
_G.nokia_mbus_service_runner = runner
assert(coroutine.resume(runner))
dofile(source:match('^(.*[/])') .. 'noki6210_mbus_version_probe.lua')
