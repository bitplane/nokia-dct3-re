-- Read-only AA22 directory-record watch; PCs are context, not writer ownership.
local machine = manager.machine
assert(machine.system.name == 'mu4nand')
local cpu = assert(machine.devices[':cpu'])
local events, capped = 0, false
machine:logerror('mu4_metadata_extent_scope: first=14.9 last=20 first_address=1456 last_address=145a cap=4096 boundary=data_write context_pc=not_owner observer_writes=0 audio_acceptance=0\n')
local tap = cpu.spaces.data:install_write_tap(0x1456,0x145a,'mu4_metadata_extent',function(address,value,mask)
    local time = machine.time:as_double()
    if machine:side_effects_disabled() or time < 14.9 or time > 20 then return end
    events = events+1
    if events > 4096 then capped = true; return end
    machine:logerror(string.format('mu4_metadata_extent_write: ordinal=%u address=%04x value=%04x mask=%04x t=%.9f context_pc=%06x ar1=%04x ar7=%04x observer_writes=0\n',
        events,address,value,mask,time,cpu.state.GENPC.value,cpu.state.AR1.value,cpu.state.AR7.value))
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_metadata_extent_terminal: events=%u capped=%u illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        events,capped and 1 or 0,cpu.state.ILLEGAL.value,machine.time:as_double()))
end)
_G.mu4_metadata_extent_handles = {tap,stop}
