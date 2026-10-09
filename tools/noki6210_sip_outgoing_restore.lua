-- Restore handset architecture only; the host clears rather than restores SIP.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_state_call.lua')
_G.noki6210_sip_restore_observers = {
    emu.add_machine_pre_save_notifier(function()
        manager.machine:logerror('sip_state: saved\n')
    end),
    emu.add_machine_post_load_notifier(function()
        manager.machine:logerror('sip_state: restored\n')
    end)
}
