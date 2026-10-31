"""Expected results for retained firmware integrations, not generic CPU coverage."""

EXTRA_PROFILES = {
    'rom4-context': (
        'Nokia ROM4 context integration: PASS cases=358 restore=358 banks=8 silicon_phase_claim=0 coverage_fnv64=ce54ab5aff19dbc0',
        'Fatal error: Nokia ROM4 context integration complete',
    ),
    'cold-pipeline': (
        'TMS320C54x ROM4 staged cold frontier: fetch=000f00 pc=0f01 sp=0000 pmst=ffa8 cycles=12',
        'Fatal error: TMS320C54x ROM4 staged cold frontier complete',
    ),
}
