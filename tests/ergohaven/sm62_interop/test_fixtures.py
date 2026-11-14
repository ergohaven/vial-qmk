"""Freeze the recovered fixture producer independently of firmware replies."""
import hashlib
import unittest

from fixtures import fixture


class FullFixtures(unittest.TestCase):
    def test_all_100_payloads_equal_frozen_original_producer(self):
        raws = [fixture(slot)[0] for slot in range(100)]
        self.assertEqual(len(set(raws)), 100)
        self.assertEqual({len(raw) for raw in raws}, {4560})
        self.assertEqual(hashlib.sha256(raws[0]).hexdigest(),
                         'c522d24c9ff9e375683251fca10afe8933e07c1e904dc4faae4116602eab7570')
        self.assertEqual(hashlib.sha256(b''.join(raws)).hexdigest(),
                         '381e04630fe0e0c4877cc35ba71652ab9da7c0ee998a2260404434f250160060')

    def test_payload_mutation_cannot_match_frozen_reference(self):
        raw = bytearray(fixture(0)[0])
        raw[64] ^= 1
        with self.assertRaises(AssertionError):
            self.assertEqual(hashlib.sha256(raw).hexdigest(),
                             'c522d24c9ff9e375683251fca10afe8933e07c1e904dc4faae4116602eab7570')


if __name__ == '__main__':
    unittest.main()
