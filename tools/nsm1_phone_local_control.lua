-- Same-duration cold control: code entry only, no contact selection or editing.
_G.nsm1_followup_contacts = {}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'nsm1_security_input.lua')
