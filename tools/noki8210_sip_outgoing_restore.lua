-- Existing physical call and exact architectural save/load; never restore SIP.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_state_call.lua')
_G.noki8210_sip_restore_observers = {
    emu.add_machine_pre_save_notifier(function()
        manager.machine:logerror('sip_state: saved\n')
    end),
    emu.add_machine_post_load_notifier(function()
        manager.machine:logerror('sip_state: restored\n')
    end)
}
