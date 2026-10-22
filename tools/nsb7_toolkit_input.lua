-- Phase-2+ card input and physical dismissal; no firmware-state writes.
_G.nsb7_security_with_pin=true
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_page_observe.lua')
local machine=manager.machine
local input=coroutine.create(function()
    -- The own firmware's first post-readiness STATUS poll is near 62s.
    assert(emu.wait(65))
    machine.screens[':screen']:snapshot('nsb7_toolkit_display.png')
    assert(emu.wait(1))
    local field
    for _,port in pairs(machine.ioport.ports) do
        for _,candidate in pairs(port.fields) do
            if candidate.name=='Left Softkey / Menu' then field=candidate end
        end
    end
    field=assert(field)
    machine:logerror((_G.nsb7_toolkit_marker or 'nsb7_toolkit_physical')..': action=dismiss\n')
    field:set_value(1)
    if not emu.wait(0.2) then field:set_value(0); return end
    field:set_value(0)
    assert(emu.wait(1.8))
    machine.screens[':screen']:snapshot('nsb7_toolkit_after_dismiss.png')
    if not _G.nsb7_toolkit_continue then machine:exit() end
end)
_G.nsb7_toolkit_input=input
assert(coroutine.resume(input))
