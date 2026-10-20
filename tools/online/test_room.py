# SPDX-License-Identifier: GPL-2.0-only
import unittest
import room
import ctr_online_server as server


class RoomTests(unittest.TestCase):
    def test_encoding_and_auth_requirement(self):
        first = room.token('synthetic room')
        self.assertTrue(room.valid_token(first))
        self.assertEqual(first,room.token('synthetic room'))
        self.assertNotEqual(first,room.token('synthetic room '))
        self.assertTrue(room.valid_token(room.token('stanza \u00e8 \U0001f3c1')))
        for password in ('','a'*257):
            with self.assertRaises(ValueError):
                room.token(password)
        for bad in (first.upper(),first[:-1],'x'*64):
            self.assertFalse(room.valid_token(bad))
            status,_,_,event,_ = server.http_answer('POST','/auth/',
                {'host':'test-auth3.demonware.net','x-ctr-room-token':bad},b'{}')
            self.assertEqual((status,event),(400,'auth3_room_rejected'))

    def test_missing_token_is_public_matchmaking(self):
        self.assertFalse(room.valid_token(''))
        self.assertEqual(room.request_token(''),room.PUBLIC_TOKEN)
        self.assertTrue(room.valid_token(room.PUBLIC_TOKEN))
        self.assertNotIn(room.PUBLIC_TOKEN,(room.token('ctr-public-room-v1'),room.token('public')))
        # A missing token now reaches the auth3 request parser instead of the room check.
        status,_,_,event,_ = server.http_answer('POST','/auth/',{'host':'test-auth3.demonware.net'},b'{}')
        self.assertNotEqual(event,'auth3_room_rejected')


if __name__ == '__main__':
    unittest.main()
