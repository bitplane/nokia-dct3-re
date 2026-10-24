-- Observe real SIMI traffic and presentation; never injects a firmware event.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_selftest_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local tx, rx, controls = 0, 0, 0
_G.nsb3_reset_request = memory:install_write_tap(0x20000, 0x20003, 'nsb3_reset_request', function(_, data, mask)
    if mask & 0x00ff0000 == 0 then return end
    local value = (data >> 16) & 255
    if value & 4 ~= 0 then
        machine:logerror(string.format('nsb3_board_reset_request: data=%02x pc=%08x lr=%08x reason=%08x count=%02x inhibit=%02x t=%.9f observer_writes=0\n',
            value, cpu.state['R15'].value, cpu.state['R14'].value, cpu.state['R4'].value,
            memory:read_u8(0x10fdee), memory:read_u8(0x10fdf3), machine.time:as_double()))
    end
end)
local function wire(address, data, mask, write)
    for lane = 0, 3 do
        local offset, shift = address + lane, (3 - lane) * 8
        if mask & (255 << shift) ~= 0 then
            local value = (data >> shift) & 255
            if write and offset == 0x20039 then
                controls = controls + 1
                if controls <= 32 then
                    machine:logerror(string.format('nsb3_simi_control: data=%02x t=%.9f observer_writes=0\n', value, machine.time:as_double()))
                end
            elseif (write and offset == 0x20036) or (not write and offset == 0x20037) then
                if write then tx = tx + 1 else rx = rx + 1 end
                if (write and tx or rx) <= 8192 then
                    machine:logerror(string.format('nsb3_sim_wire: direction=%s data=%02x t=%.9f observer_writes=0\n', write and 'TX' or 'RX', value, machine.time:as_double()))
                end
            end
        end
    end
end
_G.nsb3_sim_tx = memory:install_write_tap(0x20034, 0x2003f, 'nsb3_sim_tx', function(a, d, m) wire(a, d, m, true) end)
_G.nsb3_sim_rx = memory:install_read_tap(0x20034, 0x2003f, 'nsb3_sim_rx', function(a, d, m) wire(a, d, m, false) end)
local sample = coroutine.create(function()
    local previous = 0
    for _, time in ipairs({8, 12, 16}) do
        assert(emu.wait(time - previous))
        previous = time
        machine:logerror(string.format('nsb3_sim_state: second=%d tx=%d rx=%d controls=%d removed=%d flags=%02x observer_writes=0\n',
            time, tx, rx, controls, machine.ioport.ports[':SIM_REMOVED']:read(), memory:read_u8(0x10fde9)))
        machine.screens[':screen']:snapshot(string.format('nsb3_sim_%02d.png', time))
    end
end)
assert(coroutine.resume(sample))
