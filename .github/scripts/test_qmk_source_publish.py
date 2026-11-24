"""Focused contracts for PR-free, exact-source publication; API/Git fixtures only."""
from contextlib import redirect_stdout

import importlib.util
import io
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('qmk_source_publish', ROOT / 'util/publish_ergohaven.py')
assert spec is not None and spec.loader is not None
publish = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publish)


def protection():
    return {'required_status_checks': {
        'strict': True, 'contexts': sorted(publish.CHECK_NAMES),
        'checks': [{'context': name, 'app_id': publish.APP_ID} for name in sorted(publish.CHECK_NAMES)]}}


def rollup():
    return {'state': 'SUCCESS', 'contexts': {
        'totalCount': 2, 'pageInfo': {'hasNextPage': False},
        'nodes': [{'__typename': 'CheckRun', 'name': name, 'status': 'COMPLETED',
                   'conclusion': 'SUCCESS', 'checkSuite': {'app': {'databaseId': publish.APP_ID}}}
                  for name in sorted(publish.CHECK_NAMES)]}}


class PublicationContract(unittest.TestCase):
    def test_protection_must_keep_strict_app_bound_checks_and_permit_direct_updates(self):
        self.assertEqual(publish.protection_checks(protection(), []),
                         {(n, publish.APP_ID) for n in publish.CHECK_NAMES})
        mutations = []
        for change in ({'strict': False}, {'checks': []}, {'contexts': []}):
            value = protection()
            value['required_status_checks'].update(change)
            mutations.append(value)
        for app in (-1, None, 123):
            value = protection()
            value['required_status_checks']['checks'][0]['app_id'] = app
            mutations.append(value)
        for setting in ('allow_force_pushes', 'allow_deletions', 'lock_branch', 'required_signatures'):
            mutations.append({**protection(), setting: {'enabled': True}})
        mutations.append({**protection(), 'required_pull_request_reviews': {'required_approving_review_count': 1}})
        for value in mutations:
            with self.subTest(value=value), self.assertRaises(publish.PublishError):
                publish.protection_checks(value, [])
        with self.assertRaises(publish.PublishError):
            publish.protection_checks(protection(), [{'type': 'pull_request'}])

    def test_successful_run_cannot_replace_missing_wrong_app_or_incomplete_rollup(self):
        required = publish.protection_checks(protection(), [])
        self.assertTrue(publish.rollup_passes(rollup(), required))
        bad = [None, {'state': 'SUCCESS'}, {'state': 'FAILURE', 'contexts': rollup()['contexts']}]
        for field, value in [('status', 'IN_PROGRESS'), ('conclusion', 'FAILURE'), ('__typename', 'StatusContext')]:
            case = rollup()
            case['contexts']['nodes'][0][field] = value
            bad.append(case)
        case = rollup()
        case['contexts']['nodes'][0]['checkSuite']['app']['databaseId'] = 123
        bad.append(case)
        case = rollup()
        case['contexts']['pageInfo']['hasNextPage'] = True
        bad.append(case)
        case = rollup()
        case['contexts']['totalCount'] = 3
        bad.append(case)
        for case in bad:
            with self.subTest(case=case):
                self.assertFalse(publish.rollup_passes(case, required))
        self.assertFalse(publish.rollup_passes(rollup(), set()))

    def test_staging_only_changes_the_existing_lightweight_ci_trigger(self):
        original = subprocess.check_output([
            'git', 'show', '8bd528239a8ae1bc0747af53a6f0ba5a32f63fbc:.github/workflows/ci.yml'], cwd=ROOT)
        expected = original.replace(b'branches: [ergohaven]', b'branches: [ergohaven, ci/source-check]')
        self.assertEqual((ROOT / '.github/workflows/ci.yml').read_bytes(), expected)
        self.assertEqual(publish.PRODUCTION, 'ergohaven')
        self.assertEqual(publish.STAGING, 'ci/source-check')

    def test_controller_stages_once_and_never_promotes_unrecognized_checks(self):
        candidate, base, tree = 'c' * 40, 'b' * 40, 'a' * 40

        class FixturePublisher(publish.Publisher):
            def __init__(self, recognized):
                self.recognized = recognized
                self.source, self.production = None, base
                self.pushes = []

            def identity(self):
                pass

            def git(self, *args):
                if args == ('branch', '--show-current'):
                    return 'main'
                if args == ('rev-parse', 'HEAD'):
                    return candidate
                if args == ('rev-parse', 'HEAD^{tree}'):
                    return tree
                if args[0] == 'status':
                    return ''
                if args[0] == 'log':
                    return '\t'.join([candidate, publish.ACTOR, publish.EMAIL, publish.ACTOR, publish.EMAIL])
                raise AssertionError(args)

            def policy(self):
                value = protection()
                return self.production, publish.protection_checks(value, []), value

            def api(self, path, *args):
                if '/rules/branches/' in path:
                    return []
                raise AssertionError(path)

            def branch(self, name):
                return self.source if name == publish.STAGING else self.production

            def ancestry(self, parent, child):
                assert parent == base and child == candidate

            def push(self, child, ref):
                self.pushes.append(ref)
                if ref == publish.STAGING:
                    self.source = child
                else:
                    self.production = child

            def verify_remote_commit(self, sha, actual_tree):
                assert (sha, actual_tree) == (candidate, tree)

            def ci_run(self, sha):
                return {'status': 'completed', 'conclusion': 'success', 'html_url': 'fixture-ci-run'}

            def rollup(self, sha):
                return rollup() if self.recognized else None

            def save(self, name, value):
                pass

        with redirect_stdout(io.StringIO()):
            good = FixturePublisher(True)
            self.assertEqual(good.publish(1), candidate)
            self.assertEqual(good.pushes, [publish.STAGING, publish.PRODUCTION])
            blocked = FixturePublisher(False)
            with patch.object(publish.time, 'monotonic', side_effect=[0, 2]):
                with self.assertRaisesRegex(publish.PublishError, 'deadline'):
                    blocked.publish(1)
            self.assertEqual(blocked.pushes, [publish.STAGING])
            self.assertEqual(blocked.production, base)


if __name__ == '__main__':
    unittest.main()
