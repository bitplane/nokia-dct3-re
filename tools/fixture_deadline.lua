-- Save/load can wake emu.wait early with false; recheck absolute emulated time.
return function(deadline,initial_delay)
    local machine=manager.machine
    -- Preserve literal relative delays on the first wait: (now+d)-now can
    -- round differently from d at an emulated scheduling boundary.
    if machine.time:as_double()+1e-9<deadline then
        emu.wait(initial_delay or deadline-machine.time:as_double())
    end
    while machine.time:as_double()+1e-9<deadline do
        emu.wait(deadline-machine.time:as_double())
    end
end
