-- NSE-1 v5.30 MCU search-builder provenance; passive, capped register snapshots.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local program = assert(cpu.spaces["program"])
local taps, counts = {}, {}
for _, pc in ipairs({0x20fc94, 0x20fe38, 0x211548, 0x269390, 0x254c84,
                    0x26c394, 0x24f43a, 0x24f47c, 0x237df8, 0x238f28,
                    0x26c700, 0x2068d8, 0x208d40, 0x21fd22}) do
    local aligned = pc & ~3
    taps[#taps + 1] = program:install_read_tap(aligned, aligned + 3,
        string.format("rom4_scan_schedule_%08x", pc), function(address)
            if address ~= aligned or cpu.state["PC"].value ~= pc then return end
            if pc == 0x26c394 and cpu.state["R0"].value ~= 0x03e9 then return end
            if pc == 0x237df8 and cpu.state["R0"].value ~= 0x05e8 then return end
            if pc == 0x26c700 and cpu.state["R0"].value ~= 0x05df then return end
            if pc == 0x2068d8 and cpu.state["R0"].value ~= 0x07d5 then return end
            if pc == 0x21fd22 and cpu.state["R0"].value ~= 0x09ec then return end
            -- Only inspect the known task object's ordinary RAM header.
            local object, status = cpu.state["R8"].value, 0
            if pc == 0x254c84 then
                object = cpu.state["R1"].value
                if object < 0x100000 or object >= 0x180000 then return end
                status = program:read_u16(object)
                if status ~= 0x05e8 and status ~= 0x05df and status ~= 0x07d5 and
                    status ~= 0x09ec and
                    not (status == 0x03e9 and cpu.state["R0"].value == 10) then return end
            end
            if pc == 0x211548 and object >= 0x100000 and object < 0x180000 then
                status = program:read_u16(object)
            end
            counts[pc] = (counts[pc] or 0) + 1
            if counts[pc] > 32 then return end
            machine:logerror(string.format(
                "rom4_scan_schedule: pc=%08x r0=%08x r1=%08x r2=%08x r3=%08x r5=%08x lr=%08x object=%08x status=%04x t=%.9f\n",
                pc, cpu.state["R0"].value, cpu.state["R1"].value,
                cpu.state["R2"].value, cpu.state["R3"].value,
                cpu.state["R5"].value, cpu.state["R14"].value,
                object, status, machine.time:as_double()))
        end)
end
_G.rom4_scan_schedule = {taps, counts}
