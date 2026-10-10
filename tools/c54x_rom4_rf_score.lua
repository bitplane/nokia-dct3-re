-- Diagnostic mathematical port-input stimuli, not a GSM RF model.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local memory = cpu.spaces["data"]
local mode = os.getenv("ROM4_RF_SCORE_MODE") or "zero"
assert(mode == "zero" or mode == "dc" or mode == "tone")
local reads, results, nonzero, second, accepted = 0, 0, 0, 0, 0
local maximum = 0
local taps = {}
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
for _, address in ipairs({0x0ad1, 0x0add, 0x0ae4}) do
    taps[#taps + 1] = cpu.spaces["program"]:install_read_tap(address, address,
        string.format("rf_score_%04x", address), function(offset)
            local pc = cpu.state["PC"].value
            if pc ~= offset and pc ~= offset + 1 then return end
            if address == 0x0add then second = second + 1; return end
            if address == 0x0ae4 then accepted = accepted + 1; return end
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
    machine:logerror(string.format(
        "rom4_rf_score_summary: mode=%s reads=%d results=%d nonzero=%d maximum=%010x second=%d accepted=%d acquisition_claim=0\n",
        mode, reads, results, nonzero, maximum, second, accepted))
end)
_G.rom4_rf_score = {taps = taps, stop = stop}
