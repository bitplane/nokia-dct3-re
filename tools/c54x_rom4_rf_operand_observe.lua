-- Bounded read-only RF operand/caller observation on the recovered NSE-1 ROM.
local machine = manager.machine
local dsp = assert(machine.devices[":dsp_c54x:cpu"])
local data, program = dsp.spaces["data"], dsp.spaces["program"]
local taps, counts = {}, {}
for _, address in ipairs({0xa22f, 0xa23e, 0x3710, 0x4025}) do
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address, "rf_operand_" .. address,
        function()
            -- fetch() advances PC before the cached program read.
            if dsp.state["PC"].value ~= address + 1 then return end
            counts[address] = counts[address] + 1
            if counts[address] > 16 then return end
            local sp = dsp.state["SP"].value
            local ar2, ar3, ar5 = dsp.state["AR2"].value, dsp.state["AR3"].value, dsp.state["AR5"].value
            machine:logerror(string.format(
                "rom4_rf_operand: pc=%04x hit=%d sp=%04x return=%04x ar2=%04x word2=%04x ar3=%04x word3=%04x ar5=%04x word5=%04x a=%010x t=%.9f\n",
                address, counts[address], sp, data:read_u16(sp),
                ar2, data:read_u16(ar2), ar3, data:read_u16(ar3),
                ar5, data:read_u16(ar5), dsp.state["A"].value, machine.time:as_double()))
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
