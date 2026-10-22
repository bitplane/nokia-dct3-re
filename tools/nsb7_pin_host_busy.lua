_G.nsb7_host_outgoing_capture_name='nsb7_host_outgoing_failure.png'
_G.nsb7_outgoing_call_followup={{44,'Left Softkey / Menu'},{47,'End'}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_host_outgoing_call.lua')
