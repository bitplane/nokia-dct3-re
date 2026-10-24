-- Own v5.00 call sites plus portable native I/O observation; no state writes.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse5_fullbackup_observe.lua')
dofile(directory .. 'c54x_rom4_port36_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local memory = cpu.spaces.program
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local taps, counts = {}, {}
local pending_checksum
for _, site in ipairs({0x3bb680,0x4d7f12,0x3d6e9c,0x3d6efe,0x3e3b3a,0x3e35a8,0x4eb934,0x3af262}) do
    counts[site] = 0
    taps[#taps+1] = memory:install_read_tap(site&~3,(site&~3)+3,'nse5_v500_'..site,
        function()
            if cpu.state.PC.value ~= site then return end
            if site == 0x3bb680 then
                local object = cpu.state.R1.value
                if cpu.state.R0.value ~= 3 or memory:read_u8(object+3) ~= 0x4c then return end
                local bytes = {}
                for i=0,7 do bytes[#bytes+1] = string.format('%02x',memory:read_u8(object+i)) end
                machine:logerror(string.format('nse5_v500_control_post: bytes=%s caller=%08x t=%.9f observer_writes=0\n',
                    table.concat(bytes),cpu.state.R14.value,machine.time:as_double()))
            elseif site == 0x4eb934 then
                if cpu.state.R14.value ~= 0x3af263 or cpu.state.R0.value ~= 0 or cpu.state.R2.value ~= 0x40 then return end
                pending_checksum = cpu.state.R1.value
            elseif site == 0x3af262 then
                assert(pending_checksum, 'missing own low-record read')
                local sum = 0
                for i=0,0x3b do sum = (sum+memory:read_u8(pending_checksum+i))&0xffff end
                machine:logerror(string.format('nse5_v500_low_checksum: computed=%04x stored=%08x t=%.9f observer_writes=0\n',
                    sum,memory:read_u32(pending_checksum+0x3c),machine.time:as_double()))
                pending_checksum = nil
            end
            counts[site] = counts[site]+1
            assert(counts[site] <= 32, 'own control observer cap reached')
            machine:logerror(string.format('nse5_v500_entry: site=%08x index=%d r0=%08x r1=%08x r2=%08x r3=%08x caller=%08x t=%.9f observer_writes=0\n',
                site,counts[site],cpu.state.R0.value,cpu.state.R1.value,cpu.state.R2.value,cpu.state.R3.value,
                cpu.state.R14.value,machine.time:as_double()))
        end)
end
local entries = 0
taps[#taps+1] = dsp.spaces.program:install_read_tap(0x9129,0x9129,'nse5_v500_restart',
    function()
        if dsp.state.PC.value ~= 0x912a then return end
        entries = entries+1
        assert(entries <= 4, 'native restart observer cap reached')
        machine:logerror(string.format('nse5_v500_serial_state: flags=%04x pending=%04x timing=%04x,%04x rt0=%04x rt1=%04x t=%.9f observer_writes=0\n',
            dsp.spaces.data:read_u16(0x6f3),dsp.spaces.data:read_u16(0x6f4),
            dsp.spaces.data:read_u16(0x6f5),dsp.spaces.data:read_u16(0x6f6),
            dsp.spaces.data:read_u16(dsp.state.SP.value),dsp.spaces.data:read_u16((dsp.state.SP.value+1)&0xffff),
            machine.time:as_double()))
    end)
_G.nse5_v500_taps = taps
