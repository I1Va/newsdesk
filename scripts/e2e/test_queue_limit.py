import common


def test_outbox_overflow_drops_new_messages_but_keeps_the_old_ones(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        threshold=0.75,
        k=1,
        outbox_max_size=1,
        clients=[
            {"id": "alice", "trust": 0.5, "role": "reporter"},
            {"id": "bob", "trust": 0.3, "role": "reporter"},
        ],
    )
    port = common.free_port()
    server = common.start_server(str(config_path), port)
    try:
        alice = common.Connection("127.0.0.1", port)
        alice.hello("alice")
        alice.note("n1", "local", "fire")
        alice.close()

        bob = common.Connection("127.0.0.1", port)
        bob.hello("bob")
        bob.note("n2", "local", "fire")

        alice = common.Connection("127.0.0.1", port)
        alice.hello("alice", last_seq=0)
        alice.recv_line()
        alice.note("n3", "local", "flood")
        alice.close()

        bob.note("n4", "local", "flood")

        alice = common.Connection("127.0.0.1", port)
        alice.hello("alice", last_seq=0)
        first = alice.recv_line()
        assert first["type"] == "congrats"
        assert first["story"]["topic"] == "fire"

        assert alice.no_more_messages()
    finally:
        common.stop_server(server)
