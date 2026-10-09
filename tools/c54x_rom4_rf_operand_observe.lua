-- Bounded read-only RF operand/caller observation on the recovered NSE-1 ROM.
local machine = manager.machine
local dsp = assert(machine.devices[":dsp_c54x:cpu"])
local data, program = dsp.spaces["data"], dsp.spaces["program"]
local taps, counts = {}, {}
local observing = false
local function words(space, base, count)
    local result = {}
    for index = 0, count - 1 do
        result[#result + 1] = string.format('%04x', space:read_u16((base + index) & 0xffff))
    end
    return table.concat(result, ',')
end
for _, address in ipairs({0xa22f, 0xa23e, 0x3710, 0x4025}) do
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address, "rf_operand_" .. address,
        function()
            if observing then return end
            -- fetch() advances PC before the cached program read.
            if dsp.state["PC"].value ~= address + 1 then return end
            counts[address] = counts[address] + 1
            if counts[address] > 16 then return end
            observing = true
            local sp = dsp.state["SP"].value
            local ar2, ar3, ar5 = dsp.state["AR2"].value, dsp.state["AR3"].value, dsp.state["AR5"].value
            machine:logerror(string.format(
                "rom4_rf_operand: pc=%04x hit=%d sp=%04x return=%04x ar2=%04x word2=%04x ar3=%04x word3=%04x ar5=%04x word5=%04x a=%010x t=%.9f\n",
                address, counts[address], sp, data:read_u16(sp),
                ar2, data:read_u16(ar2), ar3, data:read_u16(ar3),
                ar5, data:read_u16(ar5), dsp.state["A"].value, machine.time:as_double()))
            if address == 0xa22f then
                local return_pc = data:read_u16(sp)
                machine:logerror(string.format(
                    'rom4_rf_control_source: hit=%d ar2=%04x data2=%s ar3=%04x data3=%s ar5=%04x program5=%s return=%04x caller=%s routine=%s\n',
                    counts[address], ar2, words(data, ar2, 8), ar3, words(data, ar3, 8),
                    ar5, words(program, ar5, 8), return_pc,
                    words(program, (return_pc - 8) & 0xffff, 16), words(program, 0xa22f, 24)))
            elseif address == 0xa23e then
                machine:logerror(string.format(
                    'rom4_rf_control_table: hit=%d base=%04x words=%s pointer197b=%04x pointer197d=%04x setup=%s\n',
                    counts[address], (ar5 - 1) & 0xffff, words(data, (ar5 - 1) & 0xffff, 8),
                    data:read_u16(0x197b), data:read_u16(0x197d), words(program, 0xa228, 7)))
            elseif address == 0x4025 then
                machine:logerror(string.format(
                    'rom4_rf_control_accumulator: a=%010x b=%010x al=%04x ah=%04x routine=%s\n',
                    dsp.state['A'].value, dsp.state['B'].value,
                    dsp.state['A'].value & 0xffff, (dsp.state['A'].value >> 16) & 0xffff,
                    words(program, 0x4010, 32)))
            end
            observing = false
        end)
end
assert(#taps == 4)
local stop_subscription = emu.add_machine_stop_notifier(function()
    local passed = counts[0xa22f] == 2 and counts[0xa23e] == 2 and
        counts[0x3710] == 0 and counts[0x4025] == 1
    print(string.format("ROM4 fresh-profile RF operand observation: %s table_calls=%d table_writes=%d alternate_writes=%d accumulator_writes=%d",
        passed and "PASS" or "FAIL", counts[0xa22f], counts[0xa23e], counts[0x3710], counts[0x4025]))
end)
assert(stop_subscription)
-- Keep Lua callbacks alive for the full run; discarded tap handles remove taps.
_G.rom4_rf_operand_observer = {taps = taps, stop = stop_subscription}
