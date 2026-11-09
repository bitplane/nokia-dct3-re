-- Passive port-27 cadence census: groups sample reads into bursts and reports
-- burst length, read sites and spacing per DSP mode word 00ac. Changes nothing.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local memory = cpu.spaces["data"]
local gap = tonumber(os.getenv("ROM4_RF_BURST_GAP_US") or "200") / 1e6
local burst, stats, transitions = nil, {}, 0
local last_mode = nil
local port3 = cpu.spaces["io"]:read_u16(3)

local function key(mode, control) return string.format("%04x/%04x", mode, control) end

local function close()
    if not burst then return end
    local s = stats[burst.key] or {bursts = 0, lengths = {}, sites = {}, spans = {}, first = burst.start}
    stats[burst.key] = s
    s.bursts = s.bursts + 1
    s.lengths[burst.count] = (s.lengths[burst.count] or 0) + 1
    local span = math.floor((burst.last - burst.start) * 1e6 + 0.5)
    s.spans[span] = (s.spans[span] or 0) + 1
    for pc, n in pairs(burst.sites) do s.sites[pc] = (s.sites[pc] or 0) + n end
    s.last = burst.start
    burst = nil
end

-- Globals: collected tap or notifier handles would be removed mid-run.
cadence_taps = {}
local taps = cadence_taps
taps[#taps + 1] = cpu.spaces["io"]:install_write_tap(3, 3, "cadence_port3", function(_, value, mask)
    port3 = (port3 & ~mask) | (value & mask)
end)
taps[#taps + 1] = cpu.spaces["io"]:install_read_tap(0x27, 0x27, "cadence_port27", function(_, data)
    local now = machine.time:as_double()
    local mode = memory:read_u16(0x00ac)
    if mode ~= last_mode then
        transitions = transitions + 1
        if transitions <= 64 or mode == 0x000a or last_mode == 0x000a then
            machine:logerror(string.format("rom4_rf_cadence_mode: t=%.6f mode=%04x aa=%04x ad=%04x af=%04x port3=%04x\n",
                now, mode, memory:read_u16(0x00aa), memory:read_u16(0x00ad), memory:read_u16(0x00af), port3))
        end
        last_mode = mode
    end
    local k = key(mode, port3 & 1)
    if burst and (now - burst.last > gap or burst.key ~= k) then close() end
    if not burst then burst = {key = k, start = now, last = now, count = 0, sites = {}} end
    burst.count = burst.count + 1
    burst.last = now
    local pc = cpu.state["PC"].value
    burst.sites[pc] = (burst.sites[pc] or 0) + 1
    return data
end)

-- Publisher entry 37ce: AR4 type, AR5 byte length, AR3 payload (see rf_score).
local publications = {}
taps[#taps + 1] = cpu.spaces["program"]:install_read_tap(0x37ce, 0x37ce, "cadence_publish", function(offset)
    local pc = cpu.state["PC"].value
    if pc ~= offset and pc ~= offset + 1 then return end
    local kind, length = cpu.state["AR4"].value, cpu.state["AR5"].value
    publications[kind] = (publications[kind] or 0) + 1
    local words = {}
    for index = 0, math.min((length + 1) // 2, 24) - 1 do
        words[#words + 1] = string.format("%04x", memory:read_u16((cpu.state["AR3"].value + index) & 0xffff))
    end
    machine:logerror(string.format("rom4_rf_cadence_publish: t=%.6f type=%04x bytes=%d mode=%04x words=%s\n",
        machine.time:as_double(), kind, length, memory:read_u16(0x00ac), table.concat(words, ",")))
end)

local function sorted(t)
    local keys = {}
    for k in pairs(t) do keys[#keys + 1] = k end
    table.sort(keys)
    return keys
end

cadence_stop = emu.add_machine_stop_notifier(function()
    close()
    for _, k in ipairs(sorted(stats)) do
        local s = stats[k]
        local parts = {}
        for _, n in ipairs(sorted(s.lengths)) do parts[#parts + 1] = n .. "x" .. s.lengths[n] end
        local sites = {}
        for _, pc in ipairs(sorted(s.sites)) do sites[#sites + 1] = string.format("%04x:%d", pc, s.sites[pc]) end
        local spans = {}
        for _, n in ipairs(sorted(s.spans)) do spans[#spans + 1] = n .. "us x" .. s.spans[n] end
        machine:logerror(string.format("rom4_rf_cadence: mode/bit0=%s bursts=%d first=%.6f last=%.6f lengths=%s\n",
            k, s.bursts, s.first, s.last, table.concat(parts, ",")))
        machine:logerror("rom4_rf_cadence_sites: mode/bit0=" .. k .. " " .. table.concat(sites, ",") .. "\n")
        machine:logerror("rom4_rf_cadence_spans: mode/bit0=" .. k .. " " .. table.concat(spans, ",") .. "\n")
    end
    machine:logerror(string.format("rom4_rf_cadence_done: transitions=%d\n", transitions))
end)
