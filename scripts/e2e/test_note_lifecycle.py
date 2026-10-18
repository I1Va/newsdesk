import common


def test_duplicate_note_returns_duplicate_without_a_second_vote(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        threshold=0.75,
        clients=[{"id": "alice", "trust": 0.5, "role": "reporter"}],
    )
    port = common.free_port()
    server = common.start_server(str(config_path), port)
    try:
        conn = common.Connection("127.0.0.1", port)
        conn.hello("alice")

        first = conn.note("n1", "local", "fire")
        assert first["status"] == "accepted"

        second = conn.note("n1", "local", "fire")
        assert second["status"] == "duplicate"

        stats = conn.stats()
        assert stats["stats"][0]["accepted"] == 1
    finally:
        common.stop_server(server)


def test_empty_story_is_rejected(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        clients=[{"id": "alice", "trust": 0.5, "role": "reporter"}],
    )
    port = common.free_port()
    server = common.start_server(str(config_path), port)
    try:
        conn = common.Connection("127.0.0.1", port)
        conn.hello("alice")

        response = conn.note("n1", "   ", "topic")
        assert response["status"] == "rejected"
        assert response["reason"] == "empty_story"
    finally:
        common.stop_server(server)


def test_crossing_the_threshold_publishes_and_congratulates_in_rank_order(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        threshold=0.75,
        k=2,
        clients=[
            {"id": "alice", "trust": 0.5, "role": "reporter"},
            {"id": "bob", "trust": 0.3, "role": "reporter"},
            {"id": "carol", "trust": 0.3, "role": "reporter"},
        ],
    )
    port = common.free_port()
    server = common.start_server(str(config_path), port)
    try:
        alice = common.Connection("127.0.0.1", port)
        alice.hello("alice")
        bob = common.Connection("127.0.0.1", port)
        bob.hello("bob")
        carol = common.Connection("127.0.0.1", port)
        carol.hello("carol")

        alice.note("n1", "local", "fire")
        bob_receipt = bob.note("n2", "local", "fire")
        assert bob_receipt["status"] == "accepted"

        alice_congrats = alice.recv_line()
        assert alice_congrats["type"] == "congrats"
        assert alice_congrats["rank"] == 1

        bob_congrats = bob.recv_line()
        assert bob_congrats["type"] == "congrats"
        assert bob_congrats["rank"] == 2

        carol_receipt = carol.note("n3", "local", "fire")
        assert carol_receipt["status"] == "accepted"

        carol_stats = carol.stats()
        assert carol_stats["stats"][0]["published"] == 0
    finally:
        common.stop_server(server)


def test_stats_of_another_client_requires_editor_role(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(
        str(config_path),
        str(journal_path),
        clients=[
            {"id": "alice", "trust": 0.5, "role": "reporter"},
            {"id": "bob", "trust": 0.5, "role": "reporter"},
            {"id": "editor1", "trust": 0.0, "role": "editor"},
        ],
    )
    port = common.free_port()
    server = common.start_server(str(config_path), port)
    try:
        alice = common.Connection("127.0.0.1", port)
        alice.hello("alice")
        forbidden = alice.stats("bob")
        assert forbidden["type"] == "error"
        assert forbidden["code"] == "forbidden"

        editor = common.Connection("127.0.0.1", port)
        editor.hello("editor1")
        allowed = editor.stats("bob")
        assert allowed["type"] == "stats"
    finally:
        common.stop_server(server)
