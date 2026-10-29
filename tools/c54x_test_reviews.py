"""Reviewed test contracts; unreviewed profiles do not inherit a classification."""
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def reviewed_profiles(root=ROOT):
    data = json.loads((root / 'fixtures/c54x_test_reviews.json').read_text())
    if data['schema'] != 1:
        raise ValueError('unsupported C54x test review schema')
    result, families = {}, set()
    for family in data['families']:
        if family['id'] in families:
            raise ValueError('duplicate C54x family review')
        families.add(family['id'])
        ids = family['case_ids']
        if len(ids) != len(set(ids)):
            raise ValueError('reviewed case identities must be unique')
        for profile in family['profiles']:
            if profile in result:
                raise ValueError('profile has multiple family reviews: ' + profile)
            coverage = family.get('profile_coverage', {}).get(profile)
            if not ids and not coverage:
                raise ValueError('isolated review needs case identities or an ordered coverage digest')
            result[profile] = dict(family=family['id'], classification=family['classification'],
                                   decision=family['decision'], case_ids=ids)
            if coverage:
                result[profile]['coverage'] = coverage
    return result


if __name__ == '__main__':
    from tools.c54x_test_oracles import EXTRA_PROFILES
    reviews = reviewed_profiles()
    unknown = set(reviews) - set(EXTRA_PROFILES)
    if unknown:
        raise SystemExit('unknown reviewed profiles: ' + ', '.join(sorted(unknown)))
    families = json.loads((ROOT / 'fixtures/c54x_test_reviews.json').read_text())['families']
    print(f'{len(families)} reviewed families; '
          f'{len(reviews)} reviewed isolated profiles; '
          f'{len(set(EXTRA_PROFILES) - set(reviews))} isolated profiles unreviewed')
