import common


def test_unknown_client_is_rejected(tmp_path):
    config_path = tmp_path / "config.json"
    journal_path = tmp_path / "server.wal"
    common.write_config(str(config_path), str(journal_path), clients=[])
    port = common.free_port()
    server = common.start_server(str(config_path), port)
    try:
        conn = common.Connection("127.0.0.1", port)
        response = conn.hello("ghost")
        assert response["type"] == "error"
        assert response["code"] == "unknown_client"
    finally:
        common.stop_server(server)


def test_known_client_gets_welcome(tmp_path):
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
        response = conn.hello("alice")
        assert response == {"type": "welcome", "client_id": "alice", "role": "reporter"}
    finally:
        common.stop_server(server)


def test_note_before_handshake_is_rejected(tmp_path):
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
        conn.send({"type": "note", "note_id": "n1", "rubric": "local", "topic": "fire",
                   "event_time": 0})
        response = conn.recv_line()
        assert response["type"] == "receipt"
        assert response["status"] == "rejected"
        assert response["reason"] == "not_authenticated"
    finally:
        common.stop_server(server)
