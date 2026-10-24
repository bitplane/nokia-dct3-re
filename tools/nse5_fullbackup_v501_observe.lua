-- The audit proves the complete v5.01 MCU prefix matches the mapped ROM.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse5_fullbackup_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local memory = cpu.spaces.program
local taps = {}
local caches = 0
local flag_writes = 0
taps[#taps+1] = memory:install_write_tap(0x17fe14,0x17fe17,'nse5_full_flags',
    function(offset, data, mask)
        for lane=0,3 do
            local shift = (3-lane)*8
            if offset+lane == 0x17fe15 and (mask & (255 << shift)) ~= 0 then
                flag_writes = flag_writes+1
                assert(flag_writes <= 128, 'full-backup flag observer cap reached')
                machine:logerror(string.format('nse5_full_flags: index=%d data=%02x pc=%08x t=%.9f observer_writes=0\n',
                    flag_writes,(data >> shift)&255,cpu.state.PC.value,machine.time:as_double()))
                -- This STRB is mid-block: observe its actual write rather
                -- than rely on a PC-keyed instruction-fetch tap firing.
                if cpu.state.PC.value == 0x30e63e then
                    machine:logerror(string.format('nse5_full_failed_item: item=%02x value=%02x t=%.9f observer_writes=0\n',
                        cpu.state.R0.value,memory:read_u8(cpu.state.R1.value),machine.time:as_double()))
                end
            end
        end
    end)
taps[#taps+1] = memory:install_read_tap(0x3fafd8,0x3fafdb,'nse5_full_journal_done',
    function()
        if cpu.state.PC.value ~= 0x3fafd8 then return end
        caches = caches+1
        assert(caches <= 4, 'full-backup journal observer cap reached')
        local bytes = {}
        for offset=0,0x897 do
            bytes[#bytes+1] = string.format('%02x', memory:read_u8(0x157424+offset))
        end
        machine:logerror(string.format('nse5_full_cache: index=%d source=%08x bytes=%s observer_writes=0\n',
            caches, memory:read_u32(cpu.state.R6.value), table.concat(bytes)))
        local source = memory:read_u32(cpu.state.R6.value)
        local sector = {}
        assert(source == 0x5fc000, 'unexpected full-backup journal source')
        for offset=0,0x1fff do
            sector[#sector+1] = string.format('%02x', memory:read_u8(source+offset))
        end
        machine:logerror(string.format('nse5_full_sector: index=%d bytes=%s observer_writes=0\n',
            caches,table.concat(sector)))
    end)
for _, entry in ipairs({{0x3af540,'identity_check'},{0x3af64e,'identity_invalid'},
                         {0x3bc2a8,'service_init'}, {0x3ae594,'context_result'},
                         {0x49fddc,'subsystem_release'}, {0x49fe36,'subsystem_skip'},
                         {0x30e7a6,'selftest_reply'}}) do
    local count = 0
    taps[#taps+1] = memory:install_read_tap(entry[1]&~3,(entry[1]&~3)+3,'nse5_full_'..entry[2],
        function()
            if cpu.state.PC.value ~= entry[1] then return end
            count = count+1
            if count <= 12 then
                machine:logerror(string.format('nse5_full_boundary: name=%s pc=%08x index=%d t=%.9f observer_writes=0\n',
                    entry[2],entry[1],count,machine.time:as_double()))
                machine:logerror(string.format('nse5_full_boundary_state: name=%s index=%d flags=%02x r0=%08x lr=%08x t=%.9f observer_writes=0\n',
                    entry[2],count,memory:read_u8(0x17fe15),cpu.state.R0.value,
                    cpu.state.R14.value,machine.time:as_double()))
                if entry[2] == 'identity_check' then
                    local bytes = {}
                    for offset=0,61 do
                        bytes[#bytes+1] = string.format('%02x', memory:read_u8(cpu.state.R6.value+offset))
                    end
                    machine:logerror(string.format('nse5_full_identity: index=%d bytes=%s observer_writes=0\n',
                        count,table.concat(bytes)))
                elseif entry[2] == 'selftest_reply' then
                    local bytes = {}
                    for offset=0,11 do
                        bytes[#bytes+1] = string.format('%02x', memory:read_u8(cpu.state.R4.value+offset))
                    end
                    machine:logerror(string.format('nse5_full_selftest: index=%d bytes=%s observer_writes=0\n',
                        count,table.concat(bytes)))
                end
            end
        end)
end
_G.nse5_fullbackup_taps = taps
