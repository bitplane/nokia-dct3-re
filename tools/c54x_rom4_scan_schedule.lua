-- NSE-1 v5.30 MCU search-builder provenance; passive, capped register snapshots.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local program = assert(cpu.spaces["program"])
local taps, counts = {}, {}
for _, pc in ipairs({0x20fc94, 0x20fe38, 0x211548, 0x269390}) do
    local aligned = pc & ~3
    taps[#taps + 1] = program:install_read_tap(aligned, aligned + 3,
        string.format("rom4_scan_schedule_%08x", pc), function(address)
            if address ~= aligned or cpu.state["PC"].value ~= pc then return end
            counts[pc] = (counts[pc] or 0) + 1
            if counts[pc] > 32 then return end
            -- Only inspect the known task object's ordinary RAM header.
            local object, status = cpu.state["R8"].value, 0
            if pc == 0x211548 and object >= 0x100000 and object < 0x180000 then
                status = program:read_u16(object)
            end
            machine:logerror(string.format(
                "rom4_scan_schedule: pc=%08x r0=%08x r1=%08x r2=%08x r3=%08x lr=%08x object=%08x status=%04x t=%.9f\n",
                pc, cpu.state["R0"].value, cpu.state["R1"].value,
                cpu.state["R2"].value, cpu.state["R3"].value,
                cpu.state["R14"].value, object, status, machine.time:as_double()))
        end)
end
_G.rom4_scan_schedule = {taps, counts}
