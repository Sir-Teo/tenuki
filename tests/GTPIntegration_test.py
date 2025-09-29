#!/usr/bin/env python3
import os
import re
import subprocess
import sys


def read_reply(proc):
    lines = []
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        line = line.decode('utf-8', errors='replace')
        lines.append(line)
        if line.strip() == '':
            break
    return ''.join(lines)


def send(proc, cmd):
    proc.stdin.write((cmd + "\n").encode('utf-8'))
    proc.stdin.flush()
    return read_reply(proc)


def expect_ok(reply):
    assert reply.startswith('='), f"Expected OK reply, got: {reply!r}"
    first_line = reply.split('\n', 1)[0]
    return first_line[1:].strip()


def expect_fail(reply, expected_substring=None):
    assert reply.startswith('?'), f"Expected failure reply, got: {reply!r}"
    first_line = reply.split('\n', 1)[0]
    payload = first_line[1:].strip()
    if expected_substring is not None:
        assert expected_substring in payload, f"Expected '{expected_substring}' in '{payload}'"
    return payload


def expect_vertex(move):
    assert move == 'pass' or re.match(r'^[A-HJ](?:[1-9]|1[0-9])$', move), f"Bad move: {move}"


def main():
    if len(sys.argv) < 2:
        print("Usage: GTPIntegration_test.py <engine_binary>")
        sys.exit(2)
    binary = sys.argv[1]
    assert os.path.exists(binary), f"Binary not found: {binary}"

    env = os.environ.copy()
    proc = subprocess.Popen(
        [binary],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )

    try:
        payload = expect_ok(send(proc, 'protocol_version'))
        assert payload == '2'

        payload = expect_ok(send(proc, 'name'))
        assert payload == 'Tenuki'

        expect_ok(send(proc, 'version'))

        expect_ok(send(proc, 'boardsize 9'))
        expect_ok(send(proc, 'clear_board'))
        expect_ok(send(proc, 'komi 7.5'))

        move = expect_ok(send(proc, 'genmove B'))
        expect_vertex(move)

        payload = expect_ok(send(proc, 'showboard'))
        assert 'A B C D E F G H J' in payload

        expect_ok(send(proc, 'play B D4'))
        expect_ok(send(proc, 'play W pass'))
        expect_ok(send(proc, 'play B J9'))

        score_payload = expect_ok(send(proc, 'final_score'))
        assert score_payload == '0' or re.match(r'^[BW]\+[0-9]+(\.[0-9])?$', score_payload)

        expect_fail(send(proc, 'unknown_command_xyz'), 'unknown_command')
        expect_fail(send(proc, 'boardsize cats'), 'invalid boardsize')
        expect_fail(send(proc, 'komi nope'), 'invalid komi')
        expect_fail(send(proc, 'play X D4'), 'invalid color')
        expect_fail(send(proc, 'play B I9'), 'invalid vertex')
        expect_fail(send(proc, 'genmove Q'), 'invalid color')

        # Deterministic scoring regression: 5x5, komi 0, single black stone.
        expect_ok(send(proc, 'boardsize 5'))
        expect_ok(send(proc, 'komi 0'))
        expect_ok(send(proc, 'clear_board'))
        expect_ok(send(proc, 'play B A1'))
        expect_ok(send(proc, 'play W pass'))
        expect_ok(send(proc, 'play B pass'))
        final_score = expect_ok(send(proc, 'final_score'))
        assert final_score == 'B+25.0', f"Unexpected deterministic score: {final_score}"

        # Vertex validation regression: out-of-range row number.
        expect_fail(send(proc, 'play B A0'), 'invalid vertex')
        expect_fail(send(proc, 'play B Z1'), 'invalid vertex')

        expect_ok(send(proc, 'quit'))
    finally:
        try:
            proc.terminate()
        except Exception:
            pass
        try:
            proc.wait(timeout=2)
        except Exception:
            proc.kill()


if __name__ == '__main__':
    main()

