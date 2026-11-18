-- Minimal driver for the 6210 call benchmark: no observers, only the inputs.
-- The laboratory BTS pages at 18 s, the handset answers with Send at 20 s
-- and ends the call with End at 30 s (the lab-tone early incoming call).
local machine=manager.machine
local function hold(port,name,seconds)
    local field=assert(machine.ioport.ports[port].fields[name])
    field:set_value(1)
    local running=emu.wait(seconds)
    field:set_value(0)
    return running
end
_G.npe3_call_bench=coroutine.create(function()
    if not emu.wait(18) or not hold(':LAB_BTS','Laboratory BTS page',0.05) then return end
    if not emu.wait(20-18.05) or not hold(':COL.0','Send',0.15) then return end
    if not emu.wait(30-20.15) then return end
    hold(':COL.0','End',0.15)
end)
assert(coroutine.resume(_G.npe3_call_bench))
