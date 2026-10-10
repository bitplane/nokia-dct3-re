-- Diagnostic mathematical port-input stimuli, not a GSM RF model.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local memory = cpu.spaces["data"]
local mode = os.getenv("ROM4_RF_SCORE_MODE") or "zero"
assert(mode == "zero" or mode == "dc" or mode == "tone")
local reads, results, nonzero, second, accepted = 0, 0, 0, 0, 0
local maximum = 0
local observing = false
local publication_counts = {}
local flag_writes = 0
local taps = {}
taps[#taps + 1] = memory:install_write_tap(0x1266, 0x1266,
    "rf_candidate_flags", function(offset, value)
        flag_writes = flag_writes + 1
        if flag_writes <= 24 then
            machine:logerror(string.format("rom4_rf_flag_write: t=%.9f pc=%04x value=%04x\n",
                machine.time:as_double(), cpu.state["PC"].value, value))
        end
    end)
taps[#taps + 1] = cpu.spaces["io"]:install_read_tap(0x27, 0x27,
    "mathematical_rf_input", function()
        local value = 0
        if mode == "dc" then
            value = reads % 2 == 0 and 2048 or 0
        elseif mode == "tone" then
            -- One turn per sixteen word pairs; no physical sample-rate claim.
            local angle = 2 * math.pi * (reads // 2 % 16) / 16
            local component = reads % 2 == 0 and math.cos(angle) or math.sin(angle)
            value = math.floor(2048 * component + 0.5) & 0xffff
        end
        reads = reads + 1
        return value
    end)
for _, address in ipairs({0x0ad1, 0x0add, 0x0ae4, 0x0ca3, 0x0cef}) do
    taps[#taps + 1] = cpu.spaces["program"]:install_read_tap(address, address,
        string.format("rf_score_%04x", address), function(offset)
            if observing then return end
            local pc = cpu.state["PC"].value
            if pc ~= offset and pc ~= offset + 1 then return end
            if address == 0x0ca3 or address == 0x0cef then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_selection_tail: address=%04x t=%.9f a=%010x score=%04x flags=%04x count=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        memory:read_u16(0x198d), memory:read_u16(0x1266), memory:read_u16(0x1344)))
                end
                return
            end
            if address == 0x0add then second = second + 1; return end
            if address == 0x0ae4 then
                accepted = accepted + 1
                if accepted <= 12 then
                    machine:logerror(string.format(
                        "rom4_rf_selection: t=%.9f a=%010x ar2=%04x ar3=%04x ar4=%04x ar5=%04x prior=%04x\n",
                        machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        cpu.state["AR2"].value, cpu.state["AR3"].value,
                        cpu.state["AR4"].value, cpu.state["AR5"].value,
                        memory:read_u16(0x198d)))
                end
                return
            end
            local value = cpu.state["A"].value & 0xffffffffff
            results = results + 1
            if value ~= 0 then nonzero = nonzero + 1 end
            maximum = math.max(maximum, value)
            if results <= 8 then
                machine:logerror(string.format(
                    "rom4_rf_score: mode=%s t=%.9f a=%010x threshold=%04x prior=%04x flag6fd=%04x\n",
                    mode, machine.time:as_double(), value, memory:read_u16(0x219b),
                    memory:read_u16(0x198d), memory:read_u16(0x06fd)))
            end
        end)
end
local stop = emu.add_machine_stop_notifier(function()
    observing = true
    for _, start in ipairs({0x0ae4, 0x0b24, 0x0b64, 0x0ba4, 0x0be4, 0x0ca3, 0x0cef}) do
        local words = {}
        for index = 0, 63 do
            words[#words + 1] = string.format("%04x", cpu.spaces["program"]:read_u16(start + index))
        end
        machine:logerror(string.format("rom4_rf_selection_code: address=%04x words=%s\n",
            start, table.concat(words, ",")))
    end
    for _, address in ipairs({0x0ca3, 0x0cef}) do
        machine:logerror(string.format("rom4_rf_selection_tail_count: address=%04x count=%d\n",
            address, publication_counts[address] or 0))
    end
    machine:logerror(string.format(
        "rom4_rf_score_summary: mode=%s reads=%d results=%d nonzero=%d maximum=%010x second=%d accepted=%d acquisition_claim=0\n",
        mode, reads, results, nonzero, maximum, second, accepted))
end)
_G.rom4_rf_score = {taps = taps, stop = stop}
