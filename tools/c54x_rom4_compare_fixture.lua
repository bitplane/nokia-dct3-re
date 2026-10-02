-- Destructive, disposable controller conformance; never boot acceptance.
-- Freeze DSP instruction execution so firmware cannot rewrite the tested ports.
local machine = manager.machine
local dsp = assert(machine.devices[":dsp_c54x:cpu"])
local backend = machine.devices[":dsp_c54x"]
local io = dsp.spaces["io"]
local slots = emu.item(assert(backend.items["0/m_slot_timer_expiries"]))
local completed = false
local save_snapshot, reference_snapshot
local active, writing, unexpected_writes = false, false, 0
local port_tap = io:install_write_tap(0x0d, 0x0f, "ctsi_fixture_isolation",
    function()
        if active and not writing then unexpected_writes = unexpected_writes + 1 end
    end)
local tick = 12 / 13000000
local function write(port, value)
    writing = true
    io:write_u16(port, value)
    writing = false
end
local function count() return slots:read(0) end
local function wait_ticks(n) assert(emu.wait(n * tick)) end
local function expect(n, label)
    assert(count() == n, string.format("%s: slot expiries=%d expected=%d", label, count(), n))
end
local function configure(counter, compare, reload)
    write(0x0f, 15000)
    write(0x0e, reload or 4999)
    write(0x0d, counter)
    write(0x0f, compare)
end
local save_subscription = emu.add_machine_pre_save_notifier(function()
    save_snapshot = {counter = io:read_u16(0x0d), slots = count(), time = machine.time:as_double()}
end)
local load_subscription = emu.add_machine_post_load_notifier(function()
    assert(save_snapshot and reference_snapshot, "missing save/reference snapshot")
    assert(io:read_u16(0x0d) == save_snapshot.counter, "counter position not restored exactly")
    expect(save_snapshot.slots, "expiry count restored")
    assert(machine.time:as_double() == save_snapshot.time, "saved emulated time not restored")
    local replay = coroutine.create(function()
        wait_ticks(100); expect(save_snapshot.slots, "restored before pending compare")
        wait_ticks(3500); expect(reference_snapshot.slots, "restored pending compare")
        local delta = (io:read_u16(0x0d) - reference_snapshot.counter + 2500) % 5000 - 2500
        assert(math.abs(delta) <= 1, "counter replay phase differs")
        assert(unexpected_writes == 0, "firmware rewrote a tested CTSI port")
        completed = true
        print(string.format("ROM4 compare save replay: saved_counter=%04x saved_slots=%d replay_slots=%d phase_delta=%d",
            save_snapshot.counter, save_snapshot.slots, count(), delta))
        print("ROM4 compare model conformance: PASS ahead behind equality repeat sentinel reload reset save")
        machine:exit()
    end)
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(0.25))
    -- ILLEGAL is the core's debugger execution-stop state, not a firmware
    -- variable. Mask interrupts too, preventing vector entry while stopped.
    dsp.state["IMR"].value = 0
    dsp.state["ILLEGAL"].value = 1
    active = true
    local n = count()
    configure(1000, 1500)
    wait_ticks(400); expect(n, "ahead before match")
    wait_ticks(200); expect(n + 1, "ahead after match")
    wait_ticks(5000); expect(n + 2, "periodic repeat")
    write(0x0f, 15000)
    wait_ticks(6000); expect(n + 2, "outside-period sentinel")

    n = count()
    configure(4000, 1000)
    wait_ticks(1900); expect(n, "behind before wrap match")
    wait_ticks(200); expect(n + 1, "behind after wrap match")

    n = count()
    configure(1000, 1000)
    wait_ticks(4900); expect(n, "equal waits next cycle")
    wait_ticks(200); expect(n + 1, "equal next-cycle match")

    n = count()
    configure(80, 90, 100)
    wait_ticks(5); expect(n, "short reload before match")
    wait_ticks(10); expect(n + 1, "short reload after match")
    write(0x0e, 9)
    wait_ticks(100); expect(n + 1, "reload makes compare unreachable")

    n = count()
    configure(2000, 2200)
    local arm = machine.devices[":maincpu"].spaces["program"]
    arm:write_u8(0x20002, 0) -- MAD2 holds the DSP reset line.
    local held = io:read_u16(0x0d)
    wait_ticks(6000)
    assert(io:read_u16(0x0d) == held, "counter advanced during reset hold")
    expect(n, "compare suppressed during reset hold")
    arm:write_u8(0x20002, 1)
    -- The CPU reset-line transition is applied by its scheduler. Let that
    -- complete before reinstating the debugger stop (reset clears it).
    wait_ticks(1)
    dsp.state["IMR"].value = 0
    dsp.state["ILLEGAL"].value = 1
    wait_ticks(100); expect(n, "retained compare before release match")
    wait_ticks(150); expect(n + 1, "retained compare after release match")
    assert(dsp.state["ILLEGAL"].value == 1 and dsp.state["IMR"].value == 0,
        "fixture lost execution isolation")
    assert(unexpected_writes == 0, "firmware rewrote a tested CTSI port")
    configure(1000, 4000)
    machine:save("ctsi_compare_fixture")
    wait_ticks(100)
    assert(save_snapshot, "save did not complete within observation window")
    expect(save_snapshot.slots, "reference before pending compare")
    wait_ticks(3500)
    expect(save_snapshot.slots + 1, "reference pending compare")
    reference_snapshot = {counter = io:read_u16(0x0d), slots = count()}
    machine:load("ctsi_compare_fixture")
end)
assert(coroutine.resume(runner))
local stop_subscription = emu.add_machine_stop_notifier(function()
    if not completed then print("ROM4 compare model conformance: FAIL incomplete") end
end)
assert(stop_subscription and port_tap and save_subscription and load_subscription)
