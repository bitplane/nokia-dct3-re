-- Dial only after the independently qualified 45s settled-idle boundary.
_G.nsb7_security_with_pin=true
_G.nsb7_outgoing_call_delay=26
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_outgoing_call_observe.lua')
local machine=manager.machine
local capture=coroutine.create(function()
    assert(emu.wait(45))
    machine.screens[':screen']:snapshot('nsb7_host_outgoing_ready.png')
    machine:logerror('nsb7_host_outgoing_ready: t=45.000000000\n')
    assert(emu.wait(15))
    machine.screens[':screen']:snapshot(_G.nsb7_host_outgoing_capture_name or 'nsb7_host_outgoing_connected.png')
end)
_G.nsb7_host_outgoing_capture=capture
assert(coroutine.resume(capture))
