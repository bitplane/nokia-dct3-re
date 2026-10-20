-- Passive first-run MCU/DSP boundary and catalogue relocation; no guest writes.
local machine = manager.machine
assert(machine.system.name == 'noki3350')
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local non_erased = 0
for address = 0x530000, 0x5ffffc, 4 do
    if memory:read_u32(address) ~= 0xffffffff then non_erased = non_erased + 1 end
end
machine:logerror(string.format('nhm9_boot_initial_storage: first=00530000 bytes=851968 non_erased_words=%d observer_writes=0\n', non_erased))
local writes = 0
local tap = memory:install_write_tap(0x10000, 0x10fff, 'nhm9_boot_shared', function(offset, data, mask)
    writes = writes + 1
    if writes <= 24 then
        machine:logerror(string.format('nhm9_boot_shared: offset=%08x data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
            offset, data, mask, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
_G.nhm9_boot_shared_tap = tap
local releases = 0
local release_tap = memory:install_write_tap(0x20000, 0x20003, 'nhm9_boot_release', function(offset, data, mask)
    if (mask & 0xff00) == 0 or ((data >> 8) & 1) == 0 then return end
    releases = releases + 1
    if releases > 4 then return end
    local words = {}
    for index = 0, 6 do
        words[#words + 1] = string.format('%04x', memory:read_u16(0x100f6 + index * 2))
    end
    machine:logerror(string.format('nhm9_boot_release: arguments=%s pc=%08x t=%.9f observer_writes=0\n',
        table.concat(words), cpu.state['R15'].value, machine.time:as_double()))
end)
_G.nhm9_boot_release_tap = release_tap
local flash_writes = 0
local flash_tap = memory:install_write_tap(0x530000, 0x5fffff, 'nhm9_boot_flash', function(offset, data, mask)
    flash_writes = flash_writes + 1
    if flash_writes <= 12 then
        machine:logerror(string.format('nhm9_boot_flash: address=%08x data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
            offset, data, mask, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
_G.nhm9_boot_flash_tap = flash_tap
local times, next_sample = {0.1, 0.5, 1, 2, 5, 10}, 1
local catalogue_checked = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if not catalogue_checked and now >= 1 then
        catalogue_checked = true
        local matches = 0
        for address = 0x100000, 0x13ff90, 4 do
            if memory:read_u32(address) == 0x3ead0c then
                local equal = true
                for index = 1, 27 do
                    if memory:read_u32(address + index * 4) ~= memory:read_u32(0x3c2248 + index * 4) then
                        equal = false
                        break
                    end
                end
                if equal then
                    matches = matches + 1
                    machine:logerror(string.format('nhm9_boot_catalogue: address=%08x entries=28 observer_writes=0\n', address))
                end
            end
        end
        machine:logerror(string.format('nhm9_boot_catalogue_summary: matches=%d aligned_ram_bytes=262144 observer_writes=0\n', matches))
        local program, geometry = {}, {}
        for index = 0, 246 do
            program[#program + 1] = string.format('%04x', memory:read_u16(0x11e00 + index * 2))
        end
        for index = 0, 6 do
            geometry[#geometry + 1] = string.format('%04x', memory:read_u16(0x100f6 + index * 2))
        end
        machine:logerror(string.format('nhm9_boot_program: words=%s observer_writes=0\n', table.concat(program)))
        machine:logerror(string.format('nhm9_boot_arguments_sample: words=%s observer_writes=0\n', table.concat(geometry)))
    end
    if not times[next_sample] or now < times[next_sample] then return end
    local words = {}
    for offset = 0, 14, 2 do words[#words + 1] = string.format('%04x', memory:read_u16(0x10000 + offset)) end
    machine:logerror(string.format('nhm9_boot_sample: t=%.9f pc=%08x sp=%08x shared=%s writes=%d ctrl=%02x dsp_reset=%02x observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['R13'].value, table.concat(words), writes,
        memory:read_u8(0x2000c), memory:read_u8(0x20002)))
    if times[next_sample] == 10 then
        machine.screens[':screen']:snapshot('nhm9_boot_frontier.png')
        machine:logerror(string.format('nhm9_boot_flash_summary: writes=%d observer_writes=0\n', flash_writes))
    end
    next_sample = next_sample + 1
end)
