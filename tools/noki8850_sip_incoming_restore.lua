-- Physical Answer and exact handset restoration; external SIP state is cleared.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8850_state_incoming = true
_G.noki8850_sip_restore_call = true
dofile(directory .. 'noki8850_state_roundtrip.lua')
_G.noki8850_sip_restore_observers = {
    emu.add_machine_pre_save_notifier(function()
        manager.machine:logerror('sip_state: saved\n')
    end),
    emu.add_machine_post_load_notifier(function()
        manager.machine:logerror('sip_state: restored\n')
    end)
}
