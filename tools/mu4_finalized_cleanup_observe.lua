-- Finalized-media EOF observation; no firmware or media writes.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'mu4_cleanup_ingress_observe.lua')
-- Do not compose cleanup_buffer here: both install a breakpoint at 02956f.
-- MAME services one breakpoint action there, so run that snapshot separately.
dofile(directory .. 'mu4_playback_lifecycle_observe.lua')
dofile(directory .. 'mu4_refill_completion_observe.lua')
