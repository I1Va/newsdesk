import common


def test_offline_client_gets_backlog_on_reconnect(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        threshold=0.75,
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
        bob_receipt = bob.note("n2", "local", "fire")
        assert bob_receipt["status"] == "accepted"
        bob_congrats = bob.recv_line()
        assert bob_congrats["type"] == "congrats"

        alice2 = common.Connection("127.0.0.1", port)
        welcome = alice2.hello("alice", last_seq=0)
        assert welcome["type"] == "welcome"
        backlog = alice2.recv_line()
        assert backlog["type"] == "congrats"
        assert backlog["rank"] == 1
    finally:
        common.stop_server(server)


def test_state_and_pending_delivery_survive_a_restart(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        threshold=0.75,
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
        bob.recv_line()
        bob.close()
    finally:
        common.stop_server(server)

    port2 = common.free_port()
    server2 = common.start_server(str(config_path), port2)
    try:
        editor = common.Connection("127.0.0.1", port2)
        editor.hello("alice")
        backlog = editor.recv_line()
        assert backlog["type"] == "congrats"
        assert backlog["rank"] == 1

        stats = editor.stats()
        assert stats["stats"][0]["published"] == 1
        assert stats["stats"][0]["scoops"] == 1

        repeat = editor.note("n1", "local", "fire")
        assert repeat["status"] == "duplicate"
    finally:
        common.stop_server(server2)
